#include <nativeui/popover.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/detail/overlay_service.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ui {
namespace {
float extent(float value, float fallback) noexcept {
    return std::isfinite(value) && value >= 0.0f ? value : fallback;
}
float sum(float value, float padding) noexcept {
    return static_cast<float>((std::min)(static_cast<double>((std::numeric_limits<float>::max)()),
                                         static_cast<double>(extent(value, 0.0f)) + 2.0 * padding));
}
PopoverStyle normalize(PopoverStyle value) {
    if (value.padding)
        value.padding = extent(*value.padding, 12.0f);
    if (value.radius)
        value.radius = extent(*value.radius, 8.0f);
    if (value.border_width)
        value.border_width = extent(*value.border_width, 1.0f);
    if (value.max_size)
        value.max_size = Size{extent(value.max_size->w, 320.0f), extent(value.max_size->h, 480.0f)};
    return value;
}
struct Recipe {
    Binding<bool> open;
    Spec anchor;
    Spec content;
    OverlayPlacement placement;
    bool match_width;
    bool focus_on_open;
    std::function<void()> on_close;
    PopoverStyle style;
};
class Root;
class Runtime final : public std::enable_shared_from_this<Runtime> {
  public:
    explicit Runtime(std::shared_ptr<const Recipe> value)
        : recipe(std::move(value)), source(recipe->open) {}
    std::shared_ptr<const Recipe> recipe;
    Binding<bool> source;
    Binding<bool>::Subscription subscription;
    detail::OverlayService *service{};
    std::function<void()> invalidate_layout;
    std::function<ComponentAvailability()> availability;
    OverlayHandle handle;
    NodeId anchor_id{};
    std::uint64_t session{};
    bool mounted{};
    bool active{true};
    bool installed{};
    bool syncing{};
    bool pending_user_close{};
    bool close_write_requested{};
    bool close_write_accepted{};
    std::uint64_t close_revision{};

    bool anchor_available() const noexcept {
        return mounted && active && availability && availability().interactive();
    }
    void notice() {
        const auto invalidate = invalidate_layout;
        if (mounted && invalidate)
            invalidate();
    }
    void close() {
        if (!installed)
            return;
        if (handle.valid() && service) {
            // Retain this exact handle if the structural invalidator throws.
            (void)service->dismiss(handle);
        }
        installed = false;
        handle = {};
        ++session;
    }
    void reject_intent() {
        const auto self = shared_from_this();
        if (!source.valid() || !source.get())
            return;
        source.set_if(false, [weak = std::weak_ptr<Runtime>(self)] {
            const auto current = weak.lock();
            return current && current->mounted && current->source.valid();
        });
    }
    void reconcile_terminal() {
        if (!installed || handle.valid())
            return;
        const auto reason = handle.close_reason();
        installed = false;
        handle = {};
        ++session;
        if (reason == OverlayCloseReason::UserOutside || reason == OverlayCloseReason::UserEscape) {
            if (source.valid() && source.get() && anchor_available()) {
                pending_user_close = true;
                close_write_requested = false;
                close_write_accepted = false;
                close_revision = source.revision();
            }
        } else if (reason == OverlayCloseReason::AnchorUnavailable ||
                   reason == OverlayCloseReason::OwnerTeardown) {
            reject_intent();
        }
    }
    void clear_pending_close() noexcept {
        pending_user_close = false;
        close_write_requested = false;
        close_write_accepted = false;
    }
    bool accepted_close_current(std::uint64_t generation) const noexcept {
        return pending_user_close && close_write_accepted && mounted && active && source.valid() &&
               session == generation && !source.get() &&
               close_revision != (std::numeric_limits<std::uint64_t>::max)() &&
               source.revision() == close_revision + 1;
    }
    void deliver_user_close() {
        if (!pending_user_close)
            return;
        const auto self = shared_from_this();
        const auto generation = session;
        if (!mounted || !active || !source.valid()) {
            clear_pending_close();
            return;
        }
        if (!close_write_requested) {
            // Pin the user-close revision before any external write/copy. A
            // newer application intent owns its own session and notification.
            if (!source.get() || source.revision() != close_revision) {
                clear_pending_close();
                return;
            }
            const auto revision = close_revision;
            close_write_requested = true;
            source.set_if(false, [weak = std::weak_ptr<Runtime>(self), generation, revision] {
                const auto current = weak.lock();
                if (!current || !current->pending_user_close || !current->mounted ||
                    !current->active || !current->source.valid() ||
                    current->session != generation || current->close_revision != revision ||
                    current->source.revision() != revision || !current->source.get())
                    return false;
                current->close_write_accepted = true;
                return true;
            });
        }
        if (!accepted_close_current(generation)) {
            if (mounted && active && source.valid() && session == generation &&
                !close_write_accepted && source.get() && source.revision() == close_revision)
                return; // queued reentrant write has not reached its final guard
            clear_pending_close();
            return;
        }
        // A throwing copy leaves the false commit recoverable. Reopening from
        // a copy instead retires this stale unstarted notification.
        auto callback = recipe->on_close;
        if (!accepted_close_current(generation)) {
            clear_pending_close();
            return;
        }
        clear_pending_close();
        if (callback)
            callback(); // begun callbacks are never replayed
    }
    void present();
    void sync() {
        const auto self = shared_from_this();
        if (!mounted || syncing)
            return;
        syncing = true;
        struct Guard {
            Runtime &runtime;
            ~Guard() { runtime.syncing = false; }
        } guard{*self};
        reconcile_terminal();
        deliver_user_close();
        if (!mounted)
            return;
        if (!source.valid()) {
            close();
            return;
        }
        if (!source.get()) {
            close();
            return;
        }
        if (!anchor_available() || !service) {
            close();
            reject_intent();
            return;
        }
        if (!installed)
            present();
    }
};

class Surface final : public Component, public detail::ThemeBinding {
  public:
    explicit Surface(std::shared_ptr<Runtime> runtime) : runtime_(std::move(runtime)) {}
    bool is_focus_scope() const noexcept override { return runtime_->recipe->focus_on_open; }
    bool focus_scope_active() const noexcept override {
        // Retain the scope restoration record until actual retained removal.
        // Logical close can precede that removal and must not erase its restore target.
        return true;
    }
    bool focus_scope_traps() const noexcept override { return false; }
    std::size_t focus_scope_default_index() const noexcept override { return 0; }
    Size measure(const std::vector<ChildMetrics> &children) const override {
        return measure_constrained(Constraints::unbounded(), children).preferred;
    }
    Size minimum_size(const std::vector<ChildMetrics> &children) const override {
        return measure_constrained(Constraints::unbounded(), children).minimum;
    }
    ChildMetrics measure_constrained(const Constraints &constraints,
                                     const std::vector<ChildMetrics> &children) const override {
        const auto c = limited(constraints);
        const auto child = children.empty() ? ChildMetrics{} : children.front();
        const auto p = padding();
        ChildMetrics result{c.constrain({sum(child.minimum.w, p), sum(child.minimum.h, p)}),
                            c.constrain({sum(child.preferred.w, p), sum(child.preferred.h, p)})};
        if (child.first_baseline)
            result.first_baseline = *child.first_baseline + p;
        return result;
    }
    Constraints child_constraints(const Constraints &constraints, std::size_t,
                                  std::size_t) const override {
        return limited(constraints).inset(padding(), padding()).loosen();
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &placements) const override {
        if (placements.empty())
            return;
        const auto p = (std::min)(padding(), (std::min)(bounds.w, bounds.h) * 0.5f);
        placements.front().bounds = {bounds.x + p, bounds.y + p,
                                     (std::max)(0.0f, bounds.w - 2.0f * p),
                                     (std::max)(0.0f, bounds.h - 2.0f * p)};
    }
    SemanticInfo semantics() const override {
        SemanticInfo value;
        value.role = SemanticRole::Group;
        value.enabled = effective_enabled();
        value.read_only = effective_read_only();
        return value;
    }
    void paint(PaintContext &context) const override {
        const auto &style = runtime_->recipe->style;
        const auto radius = style.radius.value_or(current_theme().radii.medium);
        context.painter().fill_rounded_rect(
            context.bounds(), radius, style.surface.value_or(current_theme().palette.surface));
        const auto border = style.border_width.value_or(1.0f);
        if (border > 0.0f)
            context.painter().stroke_rounded_rect(
                context.bounds(), radius, border,
                style.border.value_or(current_theme().palette.border));
    }

  private:
    float padding() const noexcept {
        return runtime_->recipe->style.padding.value_or(current_theme().spacing.medium);
    }
    Constraints limited(const Constraints &value) const noexcept {
        auto max = value.max;
        if (const auto size = runtime_->recipe->style.max_size) {
            max.w = (std::min)(max.w, size->w);
            max.h = (std::min)(max.h, size->h);
        }
        // An overlay-provided anchor minimum takes precedence over a smaller
        // style maximum, while the viewport always bounds the outer minimum.
        return {value.min, max};
    }
    std::shared_ptr<Runtime> runtime_;
};
void Runtime::present() {
    const auto self = shared_from_this();
    const auto revision = source.revision();
    if (session == (std::numeric_limits<std::uint64_t>::max)()) {
        reject_intent();
        throw std::overflow_error("Popover session identity exhausted");
    }
    const auto next = session + 1;
    try {
        OverlaySpec overlay;
        overlay.mode = OverlayMode::NonModal;
        overlay.pointer_policy = OverlayPointerPolicy::Normal;
        overlay.anchor = anchor_id;
        overlay.placement = recipe->placement;
        overlay.dismiss_on_escape = true;
        overlay.dismiss_on_outside_pointer_down = true;
        overlay.match_anchor_width = recipe->match_width;
        // All user Spec/function copies precede publication and are followed by
        // a fresh source/availability check. Each open has a separate child.
        Spec child = recipe->content;
        overlay.content =
            Spec{[self] { return std::make_unique<Surface>(self); }, {std::move(child)}};
        if (!source.valid() || source.revision() != revision || !source.get() ||
            !anchor_available())
            return;
        auto published = service->present(std::move(overlay));
        if (!published.valid()) {
            reject_intent();
            return;
        }
        handle = std::move(published);
        session = next;
        installed = true;
    } catch (...) {
        auto original = std::current_exception();
        try {
            reject_intent();
        } catch (...) {
        }
        std::rethrow_exception(original);
    }
    if (!mounted || !source.valid() || !source.get() || !anchor_available())
        close();
}

class Root final : public Component, public detail::OverlayAnchorPolicy {
  public:
    explicit Root(std::shared_ptr<Runtime> runtime) : runtime_(std::move(runtime)) {}
    const std::shared_ptr<Runtime> &runtime() const noexcept { return runtime_; }
    bool uses_retained_checkpoint() const noexcept override { return true; }
    ComponentAvailability local_availability() const noexcept override {
        // The static anchor root outlives this mounted wrapper. In particular,
        // an Enabled/Visibility builder used as Anchor retains its own getter.
        return anchor_ ? anchor_->local_availability() : ComponentAvailability{};
    }
    Size measure(const std::vector<ChildMetrics> &children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }
    Size minimum_size(const std::vector<ChildMetrics> &children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }
    ChildMetrics measure_constrained(const Constraints &constraints,
                                     const std::vector<ChildMetrics> &children) const override {
        auto result = Component::measure_constrained(constraints, children);
        if (!children.empty() && children.front().participates_in_layout)
            result.first_baseline = children.front().first_baseline;
        return result;
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &placements) const override {
        if (!placements.empty())
            placements.front().bounds = bounds;
    }
    void mount(MountContext &context) override {
        const auto runtime = runtime_;
        runtime->service = context.overlay_service();
        runtime->anchor_id = context.node_id();
        runtime->invalidate_layout = context.layout_invalidator();
        runtime->mounted = true;
        runtime->active = true;
        const std::weak_ptr<Runtime> weak = runtime;
        runtime->availability = [this, weak] {
            const auto current = weak.lock();
            if (!current || !current->mounted)
                return ComponentAvailability{VisibilityMode::Collapsed, false, false};
            auto result = effective_availability();
            const auto local = local_availability();
            if (local.visibility != VisibilityMode::Visible)
                result.visibility = local.visibility;
            result.enabled = result.enabled && local.enabled;
            return result;
        };
        runtime->subscription = runtime->source.observe([weak](bool) {
            if (const auto current = weak.lock())
                current->notice();
        });
    }
    void unmount(LifecycleContext &) override {
        const auto runtime = runtime_;
        runtime->mounted = false;
        runtime->active = false;
        runtime->pending_user_close = false;
        anchor_ = nullptr;
        runtime->subscription.reset();
        // Source identity removal is handled by the owning overlay stack at its
        // structural checkpoint. Never retain its borrowed service on teardown.
        runtime->service = nullptr;
        runtime->availability = {};
        runtime->invalidate_layout = {};
        runtime->installed = false;
        runtime->handle = {};
        ++runtime->session;
    }
    void activate(LifecycleContext &) override { runtime_->active = true; }
    void deactivate(LifecycleContext &) override {
        const auto runtime = runtime_;
        runtime->active = false;
        runtime->pending_user_close = false;
        runtime->close();
        runtime->reject_intent();
    }
    bool overlay_session_valid() const noexcept override {
        return runtime_->mounted && runtime_->active;
    }
    bool dismiss_overlay_when_disabled() const noexcept override { return true; }
    bool overlay_observes_user_close() const noexcept override { return true; }
    void paint(PaintContext &) const override {}

  private:
    void retained_checkpoint() override {
        const auto runtime = runtime_;
        runtime->sync();
    }
    void bind_descendant_context(Component &child) const override {
        if (!anchor_)
            anchor_ = &child;
    }
    std::optional<detail::DescendantSemanticDecoration>
    descendant_semantic_decoration() const override {
        detail::DescendantSemanticDecoration result;
        result.expanded = runtime_->source.valid() && runtime_->source.get();
        return result;
    }
    std::shared_ptr<Runtime> runtime_;
    mutable Component *anchor_{};
};
} // namespace

Popover &&Popover::placement(OverlayPlacement value) && {
    placement_ = value;
    return std::move(*this);
}
Popover &&Popover::match_anchor_width(bool value) && {
    match_anchor_width_ = value;
    return std::move(*this);
}
Popover &&Popover::focus_on_open(bool value) && {
    focus_on_open_ = value;
    return std::move(*this);
}
Popover &&Popover::on_close(std::function<void()> value) && {
    on_close_ = std::move(value);
    return std::move(*this);
}
Popover &&Popover::style(PopoverStyle value) && {
    style_ = normalize(std::move(value));
    return std::move(*this);
}
Spec Popover::spec() && {
    if (!anchor_.factory || !content_.factory)
        throw std::invalid_argument("Popover requires an anchor and content factory");
    const auto recipe = std::make_shared<const Recipe>(Recipe{
        std::move(open_), std::move(anchor_), std::move(content_), placement_, match_anchor_width_,
        focus_on_open_, std::move(on_close_), normalize(std::move(style_))});
    Spec result{[recipe] { return std::make_unique<Root>(std::make_shared<Runtime>(recipe)); }, {}};
    result.children_factory = [](Component &component) {
        return std::vector<Spec>{static_cast<Root &>(component).runtime()->recipe->anchor};
    };
    return result;
}
} // namespace ui
