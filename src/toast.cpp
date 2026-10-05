#include <nativeui/toast.hpp>
#include <nativeui/button.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/interaction_observer.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/ui_lifecycle_observer.hpp>
#include <nativeui/ui.hpp>

#include "detail/form_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ui {
namespace detail {
struct ToastOwnerToken {
    std::weak_ptr<void> ui_lifetime;
    OverlayHandle overlay;
};
struct ToastLifetimeToken {};
} // namespace detail
ToastHandle::ToastHandle(std::weak_ptr<const detail::ToastOwnerToken> owner,
                         std::weak_ptr<const detail::ToastLifetimeToken> lifetime,
                         std::uint64_t id) noexcept
    : owner_(std::move(owner)), lifetime_(std::move(lifetime)), id_(id) {}
bool ToastHandle::valid() const noexcept {
    const auto owner = owner_.lock();
    return id_ != 0 && owner && !owner->ui_lifetime.expired() && owner->overlay.valid() &&
           !lifetime_.expired();
}
bool ToastHandle::operator==(const ToastHandle &other) const noexcept {
    return id_ == other.id_ && !owner_.owner_before(other.owner_) &&
           !other.owner_.owner_before(owner_);
}

struct Toast::Impl final : detail::UILifecycleObserver, std::enable_shared_from_this<Toast::Impl> {
    enum class Phase { Preparing, Active, Retired };
    struct Entry {
        std::uint64_t id{};
        ToastSpec value;
        std::shared_ptr<const detail::ToastLifetimeToken> lifetime;
        TimerHandle timer;
        DispatcherDuration remaining;
        std::chrono::steady_clock::time_point started;
        std::uint64_t timer_epoch{};
        Phase phase{Phase::Preparing};
        bool timeout_pending{};
        bool hovered{};
        bool focused{};
        bool visible{true};
        [[nodiscard]] bool paused() const noexcept { return hovered || focused; }
    };
    struct PendingAction {
        std::function<void()> callback;
    };
    class Message;
    class Row;
    class Stack;

    Impl(UI &value, Dispatcher dispatcher_value)
        : ui(&value), ui_lifetime(value.lifetime_), overlay_state(value.overlay_state_),
          dispatcher(std::move(dispatcher_value)),
          owner(std::make_shared<detail::ToastOwnerToken>()) {
        owner->ui_lifetime = ui_lifetime;
    }
    UI *ui;
    std::weak_ptr<void> ui_lifetime;
    std::weak_ptr<detail::OverlayState> overlay_state;
    Dispatcher dispatcher;
    std::shared_ptr<detail::ToastOwnerToken> owner;
    std::vector<std::shared_ptr<Entry>> entries;
    std::deque<PendingAction> actions;
    OverlayHandle overlay;
    std::function<void()> structure_invalidator;
    std::function<void()> availability_invalidator;
    std::uint64_t next_id{1};
    std::uint64_t stack_generation{};
    bool shutting_down{};
    bool view_active{};
    bool busy{};
    bool draining{};
    bool action_task_pending{};
    bool model_notification_pending{};
    bool geometry_availability_pending{};

    [[nodiscard]] bool alive() const noexcept {
        return !shutting_down && !ui_lifetime.expired() && view_active && dispatcher.valid() &&
               ui->toast_owner_active();
    }
    [[nodiscard]] bool can_show() const noexcept {
        return !shutting_down && !busy && !ui_lifetime.expired() && dispatcher.valid() &&
               ui->toast_owner_available();
    }
    [[nodiscard]] std::shared_ptr<Entry> find(std::uint64_t id) const noexcept {
        const auto it = std::find_if(entries.begin(), entries.end(), [id](const auto &entry) {
            return entry->id == id && entry->phase == Phase::Active;
        });
        return it == entries.end() ? nullptr : *it;
    }
    void cancel_timer(const std::shared_ptr<Entry> &entry) {
        const auto timer = std::exchange(entry->timer, {});
        if (entry->timer_epoch != (std::numeric_limits<std::uint64_t>::max)())
            ++entry->timer_epoch;
        if (timer.valid())
            (void)dispatcher.cancel(timer);
    }
    void cancel_timer_noexcept(const std::shared_ptr<Entry> &entry) noexcept {
        try {
            cancel_timer(entry);
        } catch (...) {
        }
    }
    void close_noexcept() noexcept {
        const auto previous = std::exchange(overlay, {});
        if (owner)
            owner->overlay = {};
        if (stack_generation != (std::numeric_limits<std::uint64_t>::max)())
            ++stack_generation;
        structure_invalidator = {};
        availability_invalidator = {};
        if (!previous.valid())
            return;
        // The weak overlay owner is safe even after UI publishes its death
        // token. Quiet close has no application invalidation callback; UI owns
        // the later ordinary structure checkpoint.
        if (const auto state = overlay_state.lock())
            (void)state->close_quiet_noexcept(previous);
    }
    void abandon() noexcept {
        // Lifetimes become terminal before any invalidation/destruction can
        // reenter. No application action is invoked by this path.
        view_active = false;
        std::vector<std::shared_ptr<Entry>> removed;
        removed.swap(entries);
        for (const auto &entry : removed) {
            entry->phase = Phase::Retired;
            entry->lifetime.reset();
        }
        actions.clear();
        for (const auto &entry : removed)
            cancel_timer_noexcept(entry);
        action_task_pending = false;
        model_notification_pending = false;
        geometry_availability_pending = false;
        close_noexcept();
    }
    void ui_will_deactivate() noexcept override { abandon(); }
    void ui_will_teardown() noexcept override { shutdown(); }
    void shutdown() noexcept {
        if (shutting_down)
            return;
        shutting_down = true;
        abandon();
        owner.reset();
    }
    void notify() {
        if (!model_notification_pending || shutting_down || ui_lifetime.expired())
            return;
        const auto callback = structure_invalidator;
        if (callback) {
            model_notification_pending = false;
            callback();
        } else {
            const auto state = ui->overlay_state_;
            model_notification_pending = false;
            state->invalidate_structure();
        }
    }
    bool arm(const std::shared_ptr<Entry> &entry) {
        if (!alive() || entry->phase == Phase::Retired || entry->paused() ||
            entry->timer_epoch == (std::numeric_limits<std::uint64_t>::max)())
            return false;
        const auto epoch = ++entry->timer_epoch;
        entry->started = dispatcher.current_time();
        entry->timeout_pending = false;
        const std::weak_ptr<Impl> weak = shared_from_this();
        const std::weak_ptr<Entry> weak_entry = entry;
        auto timer = dispatcher.schedule_after(entry->remaining, [weak, weak_entry, epoch] {
            const auto self = weak.lock();
            const auto current = weak_entry.lock();
            if (!self || !current || current->timer_epoch != epoch ||
                current->phase == Phase::Retired)
                return;
            current->timer = {};
            current->remaining = DispatcherDuration::zero();
            current->timeout_pending = true;
            if (current->phase == Phase::Preparing) {
                current->timeout_pending = true;
                return;
            }
            if (!self->alive()) {
                self->abandon();
                return;
            }
            (void)self->retire(current->id, false);
        });
        if (!timer.valid())
            return false;
        if (entry->phase == Phase::Retired || entry->timeout_pending || !alive()) {
            (void)dispatcher.cancel(timer);
            return false;
        }
        entry->timer = std::move(timer);
        return true;
    }
    void schedule_actions() {
        if (actions.empty() || action_task_pending || shutting_down || ui_lifetime.expired())
            return;
        const std::weak_ptr<Impl> weak = shared_from_this();
        action_task_pending = true;
        try {
            const bool accepted = dispatcher.post([weak] {
                const auto self = weak.lock();
                if (!self)
                    return;
                self->action_task_pending = false;
                try {
                    self->close_empty();
                    self->drain_actions();
                } catch (...) {
                    try {
                        self->schedule_actions();
                    } catch (...) {
                    }
                    throw;
                }
            });
            if (!accepted)
                action_task_pending = false;
        } catch (...) {
            action_task_pending = false;
            throw;
        }
    }
    void close_empty() {
        // Keep the retained retry participant alive while an accepted action
        // has not begun. A refused recovery post must not remove its only path
        // to a later normal checkpoint.
        if (!entries.empty() || !actions.empty() || !overlay.valid() || ui_lifetime.expired())
            return;
        const auto self = shared_from_this();
        const auto handle = overlay;
        const auto state = ui->overlay_state_;
        // Closing publishes through application invalidators. A nested show
        // must not reuse the exact overlay that this transaction will erase.
        const bool previous_busy = std::exchange(busy, true);
        struct ClosingGuard {
            Impl& owner;
            bool previous;
            ~ClosingGuard() { owner.busy = previous; }
        } closing{*self, previous_busy};
        (void)state->close(handle);
        if (overlay == handle && !handle.valid()) {
            overlay = {};
            if (owner)
                owner->overlay = {};
            if (stack_generation != (std::numeric_limits<std::uint64_t>::max)())
                ++stack_generation;
            structure_invalidator = {};
            availability_invalidator = {};
        }
    }
    void drain_actions() {
        if (draining || actions.empty())
            return;
        const auto self = shared_from_this();
        if (!alive()) {
            abandon();
            return;
        }
        // Deferred recovery waits for a dispatcher checkpoint outside a retained
        // lifecycle gate. In particular a callback can immediately show again.
        if (!ui->toast_owner_available()) {
            schedule_actions();
            return;
        }
        draining = true;
        struct Guard {
            Impl &owner;
            ~Guard() { owner.draining = false; }
        } guard{*self};
        while (!actions.empty() && alive()) {
            auto callback = std::move(actions.front().callback);
            actions.pop_front(); // one-shot terminal before application code
            try {
                if (callback)
                    callback();
            } catch (...) {
                try {
                    schedule_actions();
                } catch (...) {
                }
                throw;
            }
        }
        // A reentrant action may have published another message. Close only
        // after begun callbacks return and the owned model is still empty.
        close_empty();
    }
    bool retire(std::uint64_t id, bool invoke_action) {
        const auto self = shared_from_this();
        const auto entry = find(id);
        if (!entry || busy || shutting_down)
            return false;
        std::vector<std::shared_ptr<Entry>> candidate;
        candidate.reserve(entries.size() - 1);
        for (const auto &current : entries)
            if (current != entry)
                candidate.push_back(current);
        // Allocate a durable callback slot before the logical terminal commit.
        if (invoke_action && entry->value.action)
            actions.emplace_back();
        if (invoke_action && entry->value.action)
            actions.back().callback = std::move(entry->value.action);
        std::function<void()> abandoned_action;
        if (!invoke_action)
            abandoned_action = std::move(entry->value.action);
        entry->phase = Phase::Retired;
        entry->lifetime.reset();
        entries.swap(candidate);
        model_notification_pending = true;
        try {
            cancel_timer(entry);
            notify();
            close_empty();
        } catch (...) {
            try {
                schedule_actions();
            } catch (...) {
            }
            throw;
        }
        drain_actions();
        return true;
    }
    void pause(std::uint64_t id, bool hover, bool value) {
        const auto self = shared_from_this();
        const auto entry = find(id);
        if (!entry)
            return;
        if (!alive()) {
            abandon();
            return;
        }
        const bool before = entry->paused();
        bool &flag = hover ? entry->hovered : entry->focused;
        if (flag == value)
            return;
        flag = value;
        const bool after = entry->paused();
        if (before == after)
            return;
        if (after) {
            const auto elapsed = std::chrono::duration_cast<DispatcherDuration>(
                dispatcher.current_time() - entry->started);
            entry->remaining = (std::max)(DispatcherDuration::zero(), entry->remaining - elapsed);
            cancel_timer(entry);
        } else {
            try {
                if (!arm(entry))
                    (void)retire(id, false);
            } catch (...) {
                auto failure = std::current_exception();
                try {
                    (void)retire(id, false);
                } catch (...) {
                }
                std::rethrow_exception(failure);
            }
        }
    }
    void sync() {
        const auto self = shared_from_this();
        if (!alive()) {
            abandon();
            return;
        }
        if (busy)
            return;
        if (!overlay.valid()) {
            abandon();
            return;
        }
        notify();
        if (geometry_availability_pending) {
            const auto callback = availability_invalidator;
            geometry_availability_pending = false;
            if (callback)
                callback();
        }
        const auto snapshot = entries;
        for (const auto &entry : snapshot) {
            if (entry->phase == Phase::Active && !entry->paused() && !entry->timer.valid()) {
                if (entry->timeout_pending || !arm(entry))
                    (void)retire(entry->id, false);
            }
        }
        if (!actions.empty())
            schedule_actions();
        close_empty();
    }
    Spec stack_spec(std::uint64_t generation);
    Spec row_spec(const std::shared_ptr<Entry> &entry);
    ToastShowResult show(ToastSpec value) {
        const auto self = shared_from_this();
        if (value.message.empty() ||
            static_cast<bool>(value.action) != !value.action_label.empty() ||
            (value.duration && value.duration->count() <= 0))
            return {ToastShowStatus::InvalidSpec, {}};
        if (!can_show())
            return {ToastShowStatus::Unavailable, {}};
        if (!entries.empty() && !overlay.valid())
            abandon();
        if (shutting_down || next_id == 0)
            return {ToastShowStatus::Unavailable, {}};
        view_active = true;
        const auto duplicate = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
            return entry->value.message == value.message;
        });
        const std::shared_ptr<Entry> replaced = duplicate == entries.end() ? nullptr : *duplicate;
        if (!replaced && entries.size() + actions.size() >= 32)
            return {ToastShowStatus::Unavailable, {}};
        busy = true;
        struct Guard {
            Impl &owner;
            ~Guard() { owner.busy = false; }
        } guard{*self};
        auto entry = std::make_shared<Entry>();
        entry->id = next_id;
        if (next_id == (std::numeric_limits<std::uint64_t>::max)())
            next_id = 0;
        else
            ++next_id;
        entry->remaining = DispatcherDuration{value.duration.value_or(
            value.action ? std::chrono::milliseconds{8000} : std::chrono::milliseconds{4000})};
        entry->value = std::move(value);
        entry->lifetime = std::make_shared<const detail::ToastLifetimeToken>();
        std::vector<std::shared_ptr<Entry>> candidate;
        candidate.reserve(entries.size() + (replaced ? 0 : 1));
        for (const auto &current : entries)
            if (current != replaced)
                candidate.push_back(current);
        candidate.push_back(entry);
        bool committed = false;
        std::function<void()> abandoned_action;
        try {
            if (!arm(entry)) {
                entry->lifetime.reset();
                return {ToastShowStatus::Unavailable, {}};
            }
            if (!overlay.valid()) {
                if (stack_generation == (std::numeric_limits<std::uint64_t>::max)()) {
                    entry->lifetime.reset();
                    cancel_timer(entry);
                    return {ToastShowStatus::Unavailable, {}};
                }
                const auto generation = stack_generation + 1;
                OverlaySpec presentation;
                presentation.placement = OverlayPlacement::ViewportBottomCenter;
                presentation.mode = OverlayMode::NonModal;
                presentation.pointer_policy = OverlayPointerPolicy::Normal;
                presentation.content = stack_spec(generation);
                const auto state = ui->overlay_state_;
                auto published = ui->show_overlay(std::move(presentation));
                if (shutting_down || ui_lifetime.expired() || !owner ||
                    !ui->toast_owner_available()) {
                    (void)state->close_quiet_noexcept(published);
                    entry->lifetime.reset();
                    cancel_timer(entry);
                    return {ToastShowStatus::Unavailable, {}};
                }
                if (!published.valid()) {
                    cancel_timer(entry);
                    entry->lifetime.reset();
                    return {ToastShowStatus::Unavailable, {}};
                }
                overlay = std::move(published);
                owner->overlay = overlay;
                stack_generation = generation;
            }
            if (shutting_down || ui_lifetime.expired() || !ui->toast_owner_available() ||
                entry->timeout_pending) {
                cancel_timer(entry);
                entry->lifetime.reset();
                if (entries.empty())
                    close_noexcept();
                return {ToastShowStatus::Unavailable, {}};
            }
            // Timers and the single overlay are prepared. Commit owned message
            // identities before signalling retained structure. A notification
            // throw leaves this accepted generation/timer owned for recovery.
            entry->phase = Phase::Active;
            entries.swap(candidate);
            committed = true;
            model_notification_pending = true;
            if (replaced) {
                abandoned_action = std::move(replaced->value.action);
                replaced->phase = Phase::Retired;
                replaced->lifetime.reset();
                cancel_timer(replaced);
            }
            notify();
        } catch (...) {
            if (!committed) {
                entry->phase = Phase::Retired;
                entry->lifetime.reset();
                cancel_timer_noexcept(entry);
                if (entries.empty())
                    close_noexcept();
            }
            throw;
        }
        if (shutting_down || !entry->lifetime || ui_lifetime.expired())
            return {ToastShowStatus::Unavailable, {}};
        return {ToastShowStatus::Shown, ToastHandle{owner, entry->lifetime, entry->id}};
    }
};

class Toast::Impl::Message final : public Component, public detail::ThemeBinding {
  public:
    explicit Message(std::shared_ptr<Entry> entry) : entry_(std::move(entry)) {}
    Size measure(const std::vector<ChildMetrics> &) const override {
        return calculate(Constraints::unbounded())->size;
    }
    ChildMetrics measure_constrained(const Constraints &constraints,
                                     const std::vector<ChildMetrics> &) const override {
        const auto geometry = calculate(constraints);
        return {Size{}, geometry->size};
    }
    SemanticInfo semantics() const override {
        SemanticInfo value;
        value.role = SemanticRole::Text;
        value.name = entry_->value.message;
        return value;
    }
    void paint(PaintContext &context) const override {
        const auto geometry = published_;
        if (!geometry)
            return;
        detail::paint_form_text(context.painter(), context.bounds(), entry_->value.message,
                                geometry->style, geometry->paragraph);
    }

  private:
    struct Geometry {
        detail::Paragraph paragraph;
        TextStyle style;
        Size size;
    };
    std::shared_ptr<Geometry> calculate(const Constraints &constraints) const {
        auto result = std::make_shared<Geometry>();
        result->style = detail::form_text_style(current_theme(), {}, current_theme().palette.text);
        result->paragraph =
            detail::wrap_form_text(entry_->value.message, result->style, constraints.max.w);
        result->size = constraints.constrain({result->paragraph.width, result->paragraph.height});
        return result;
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &) const override {
        candidate_ = calculate(Constraints::tight({bounds.w, bounds.h}));
    }
    void layout_committed(Rect, Rect) noexcept override { published_.swap(candidate_); }
    std::shared_ptr<Entry> entry_;
    mutable std::shared_ptr<Geometry> candidate_;
    std::shared_ptr<Geometry> published_;
};
class Toast::Impl::Row final : public Component,
                               public detail::ThemeBinding,
                               public detail::RetainedInteractionObserver {
  public:
    Row(std::weak_ptr<Impl> owner, std::shared_ptr<Entry> entry)
        : owner_(std::move(owner)), entry_(std::move(entry)) {}
    bool pointer_targetable() const noexcept override { return true; }
    bool clips_children() const noexcept override { return true; }
    ComponentAvailability local_availability() const noexcept override {
        return {entry_->phase == Phase::Retired ? VisibilityMode::Collapsed
                : entry_->visible               ? VisibilityMode::Visible
                                                : VisibilityMode::Hidden,
                entry_->phase != Phase::Retired, false};
    }
    Size measure(const std::vector<ChildMetrics> &children) const override {
        return size_for(children);
    }
    ChildMetrics measure_constrained(const Constraints &constraints,
                                     const std::vector<ChildMetrics> &children) const override {
        return {constraints.constrain({}), constraints.constrain(size_for(children))};
    }
    Constraints child_constraints(const Constraints &constraints, std::size_t index,
                                  std::size_t) const override {
        const auto inner = std::isfinite(constraints.max.w)
                               ? (std::max)(0.0f, constraints.max.w - 24.0f)
                               : kUnboundedExtent;
        if (index == 1)
            return {{}, {inner, kUnboundedExtent}};
        const auto action =
            entry_->value.action_label.empty() ? 0.0f : (std::min)(inner, action_width());
        const auto width = action > 0.0f ? (std::max)(0.0f, inner - action - 8.0f) : inner;
        return {{}, {width, kUnboundedExtent}};
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                         std::vector<ChildPlacement> &placements) const override {
        if (placements.empty())
            return;
        const auto p = (std::min)(12.0f, (std::min)(bounds.w, bounds.h) * 0.5f);
        const auto inner = (std::max)(0.0f, bounds.w - 2.0f * p);
        const auto height = (std::max)(0.0f, bounds.h - 2.0f * p);
        const auto button = children.size() > 1 ? (std::min)(inner, children[1].preferred.w) : 0.0f;
        const auto gap = button > 0.0f ? (std::min)(8.0f, (std::max)(0.0f, inner - button)) : 0.0f;
        placements[0].bounds = {bounds.x + p, bounds.y + p, (std::max)(0.0f, inner - button - gap),
                                height};
        if (placements.size() > 1) {
            const auto button_h = (std::min)(height, children[1].preferred.h);
            placements[1].bounds = {bounds.x + bounds.w - p - button,
                                    bounds.y + p + (height - button_h) * 0.5f, button, button_h};
        }
    }
    void retained_pointer_hover_changed(bool value, bool, Dispatcher) override {
        if (const auto owner = owner_.lock())
            owner->pause(entry_->id, true, value);
    }
    void retained_focus_within_changed(bool value, bool, Dispatcher) override {
        if (const auto owner = owner_.lock())
            owner->pause(entry_->id, false, value);
    }
    SemanticInfo semantics() const override {
        SemanticInfo value;
        value.role = SemanticRole::Group;
        value.enabled = entry_->phase == Phase::Active;
        return value;
    }
    void paint(PaintContext &context) const override {
        context.painter().fill_rounded_rect(context.bounds(), current_theme().radii.medium,
                                            current_theme().palette.surface);
        context.painter().stroke_rounded_rect(context.bounds(), current_theme().radii.medium, 1.0f,
                                              current_theme().palette.border);
    }

  private:
    float action_width() const {
        auto style = detail::form_text_style(current_theme(), {}, current_theme().palette.text);
        return TextService::measure(entry_->value.action_label, style).width + 16.0f;
    }
    Size size_for(const std::vector<ChildMetrics> &children) const {
        const auto first = children.empty() ? Size{} : children[0].preferred;
        const auto second = children.size() > 1 ? children[1].preferred : Size{};
        return {first.w + second.w + (children.size() > 1 ? 8.0f : 0.0f) + 24.0f,
                (std::max)(first.h, second.h) + 24.0f};
    }
    std::weak_ptr<Impl> owner_;
    std::shared_ptr<Entry> entry_;
};
class Toast::Impl::Stack final : public Component, public detail::DynamicChildrenSource {
  public:
    Stack(std::weak_ptr<Impl> owner, std::uint64_t generation)
        : owner_(std::move(owner)), generation_(generation) {}
    bool uses_retained_checkpoint() const noexcept override { return true; }
    bool clips_children() const noexcept override { return true; }
    Size measure(const std::vector<ChildMetrics> &children) const override {
        if (children.empty())
            return {};
        double width = 0.0, height = 24.0;
        for (std::size_t i = 0; i < children.size(); ++i) {
            width = (std::max)(width, static_cast<double>(children[i].preferred.w));
            height += children[i].preferred.h;
            if (i)
                height += 8.0;
        }
        return {detail::saturating_extent(width + 48.0), detail::saturating_extent(height)};
    }
    Constraints child_constraints(const Constraints &constraints, std::size_t,
                                  std::size_t) const override {
        return {{},
                {constraints.bounded_width() ? (std::max)(0.0f, constraints.max.w - 48.0f)
                                             : kUnboundedExtent,
                 kUnboundedExtent}};
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                         std::vector<ChildPlacement> &placements) const override {
        const auto owner = owner_.lock();
        if (!owner || generation_ != owner->stack_generation)
            return;
        const auto snapshot = owner->entries;
        if (snapshot.size() != children.size() || children.size() != placements.size())
            throw std::logic_error("Toast stack child snapshot mismatch");
        candidate_.clear();
        candidate_.reserve(snapshot.size());
        double total = 0.0;
        for (std::size_t i = 0; i < children.size(); ++i)
            total += children[i].preferred.h + (i ? 8.0 : 0.0);
        double y = static_cast<double>(bounds.y) + bounds.h - 24.0 - total;
        const auto padding = (std::min)(24.0f, bounds.w * 0.5f);
        const auto width = (std::max)(0.0f, bounds.w - 2.0f * padding);
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto row = Rect{bounds.x + padding, detail::saturating_coordinate(y), width,
                                  children[i].preferred.h};
            placements[i].bounds = row;
            candidate_.push_back({snapshot[i], !intersect(bounds, row).empty()});
            y += children[i].preferred.h + 8.0;
        }
    }
    std::vector<std::string> desired_keys() const override {
        const auto owner = owner_.lock();
        prepared_ = owner ? owner->entries : std::vector<std::shared_ptr<Entry>>{};
        std::vector<std::string> keys;
        keys.reserve(prepared_.size());
        for (const auto &entry : prepared_)
            keys.push_back(std::to_string(entry->id));
        return keys;
    }
    std::vector<detail::DynamicChildSpec> desired_children() const override {
        const auto owner = owner_.lock();
        if (!owner)
            return {};
        const auto snapshot = prepared_;
        std::vector<detail::DynamicChildSpec> result;
        result.reserve(snapshot.size());
        for (const auto &entry : snapshot)
            result.push_back({std::to_string(entry->id), owner->row_spec(entry)});
        return result;
    }
    void set_structure_invalidator(std::function<void()> invalidate) override {
        if (const auto owner = owner_.lock(); owner && generation_ == owner->stack_generation)
            owner->structure_invalidator = std::move(invalidate);
    }
    void mount(MountContext &context) override {
        if (const auto owner = owner_.lock(); owner && generation_ == owner->stack_generation)
            owner->availability_invalidator = context.availability_invalidator();
    }
    void deactivate(LifecycleContext &) override {
        if (const auto owner = owner_.lock(); owner && generation_ == owner->stack_generation)
            owner->abandon();
    }
    void unmount(LifecycleContext &) override {
        if (const auto owner = owner_.lock(); owner && generation_ == owner->stack_generation)
            owner->abandon();
    }
    SemanticInfo semantics() const override {
        SemanticInfo value;
        value.role = SemanticRole::Group;
        return value;
    }
    void paint(PaintContext &) const override {}

  private:
    void retained_checkpoint() override {
        if (const auto owner = owner_.lock(); owner && generation_ == owner->stack_generation)
            owner->sync();
    }
    void layout_committed(Rect, Rect) noexcept override {
        const auto owner = owner_.lock();
        if (!owner || generation_ != owner->stack_generation)
            return;
        for (const auto &candidate : candidate_) {
            if (candidate.entry->phase != Phase::Active)
                continue;
            if (candidate.entry->visible != candidate.visible) {
                candidate.entry->visible = candidate.visible;
                owner->geometry_availability_pending = true;
            }
        }
    }
    struct Candidate {
        std::shared_ptr<Entry> entry;
        bool visible{};
    };
    std::weak_ptr<Impl> owner_;
    std::uint64_t generation_;
    mutable std::vector<std::shared_ptr<Entry>> prepared_;
    mutable std::vector<Candidate> candidate_;
};
Spec Toast::Impl::row_spec(const std::shared_ptr<Entry> &entry) {
    const std::weak_ptr<Impl> weak = shared_from_this();
    std::vector<Spec> children;
    children.push_back(Spec{[entry] { return std::make_unique<Message>(entry); }, {}});
    if (!entry->value.action_label.empty()) {
        ButtonStyle style;
        style.base.minimum_width = 0.0f;
        style.base.horizontal_padding = 8.0f;
        style.base.control_height = 32.0f;
        children.push_back(Button{entry->value.action_label,
                                  [weak, id = entry->id] {
                                      if (const auto owner = weak.lock(); owner && owner->alive())
                                          (void)owner->retire(id, true);
                                  }}
                               .variant(ButtonVariant::Primary)
                               .style(std::move(style))
                               .spec());
    }
    return Spec{[weak, entry] { return std::make_unique<Row>(weak, entry); }, std::move(children)};
}
Spec Toast::Impl::stack_spec(std::uint64_t generation) {
    const std::weak_ptr<Impl> weak = shared_from_this();
    Spec result{[weak, generation] { return std::make_unique<Stack>(weak, generation); }, {}};
    result.children_factory = [weak](Component &) {
        const auto owner = weak.lock();
        if (!owner)
            return std::vector<Spec>{};
        const auto snapshot = owner->entries;
        std::vector<Spec> children;
        children.reserve(snapshot.size());
        for (const auto &entry : snapshot)
            children.push_back(owner->row_spec(entry));
        return children;
    };
    return result;
}
Toast::Toast(UI &ui, Dispatcher dispatcher)
    : impl_(std::make_shared<Impl>(ui, std::move(dispatcher))) {
    ui.register_lifecycle_observer(impl_);
}
Toast::~Toast() noexcept {
    if (impl_)
        impl_->shutdown();
}
ToastShowResult Toast::show(ToastSpec value) {
    const auto impl = impl_;
    return impl->show(std::move(value));
}
bool Toast::dismiss(ToastHandle handle) {
    const auto impl = impl_;
    if (!impl || !handle.valid())
        return false;
    const auto owner = handle.owner_.lock();
    const auto lifetime = handle.lifetime_.lock();
    if (owner != impl->owner || !lifetime)
        return false;
    const auto entry = impl->find(handle.id_);
    if (!entry || entry->lifetime != lifetime)
        return false;
    return impl->retire(entry->id, false);
}
} // namespace ui
