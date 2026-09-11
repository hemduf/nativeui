#pragma once

#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/ui.hpp>
#include <nativeui/widgets.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ui {

using DialogActionId = std::string;

enum class DialogResultKind {
    Action,
    Dismissed,
};

struct DialogResult {
    DialogResultKind kind{DialogResultKind::Dismissed};
    DialogActionId action_id;
};

enum class DialogActionRole {
    Normal,
    Default,
    Cancel,
};

struct DialogAction {
    DialogActionId id;
    std::string label;
    bool enabled{true};
    DialogActionRole role{DialogActionRole::Normal};
};

struct DialogSpec {
    std::string title;
    Spec body;
    std::vector<DialogAction> actions;
    Color backdrop_color{0.0f, 0.0f, 0.0f, 0.48f};
};

enum class DialogShowResult {
    Shown,
    Busy,
    InvalidSpec,
    Unavailable,
};

namespace detail {

inline constexpr float kDialogViewportMargin = 24.0f;
inline constexpr float kDialogMaximumWidth = 560.0f;
inline constexpr float kDialogPadding = 20.0f;
inline constexpr float kDialogSectionGap = 12.0f;
inline constexpr float kDialogActionGap = 8.0f;

struct DialogSpecValue {
    Spec value;
    Spec spec() && { return std::move(value); }
};

class DialogFixedEnabledComponent final : public Component {
public:
    explicit DialogFixedEnabledComponent(bool enabled) : enabled_(enabled) {}

    [[nodiscard]] ComponentAvailability local_availability() const noexcept override {
        ComponentAvailability availability{};
        availability.enabled = enabled_;
        return availability;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void paint(PaintContext&) const override {}

private:
    bool enabled_{true};
};

/// Full-viewport T063 content shell. T061 remains the sole overlay/modal stack;
/// this component only supplies the Dialog-specific visual backdrop and centers
/// the bounded panel inside the viewport. Because the T061 OverlayEntry itself
/// remains pointer-targetable over these full bounds, backdrop clicks are
/// consumed without introducing another hit-test or dismissal layer.
class DialogBackdropComponent final : public Component {
public:
    explicit DialogBackdropComponent(Color color) : color_(color) {}

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints.loosen();
    }

    [[nodiscard]] ChildMetrics measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override {
        const auto child = children.empty() ? ChildMetrics{} : children.front();
        Size preferred = child.preferred;
        if (constraints.bounded_width()) preferred.w = constraints.max.w;
        if (constraints.bounded_height()) preferred.h = constraints.max.h;
        preferred = constraints.constrain(preferred);
        return ChildMetrics{constraints.constrain({}), preferred};
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (children.empty() || placements.empty()) return;
        const auto preferred = children.front().preferred;
        const float width = std::min(bounds.w, preferred.w);
        const float height = std::min(bounds.h, preferred.h);
        placements.front().bounds = Rect{
            bounds.x + (bounds.w - width) * 0.5f,
            bounds.y + (bounds.h - height) * 0.5f,
            width,
            height};
    }

    void paint(PaintContext& context) const override {
        context.painter().fill_rounded_rect(context.bounds(), 0.0f, color_);
    }

private:
    Color color_{};
};

struct DialogPanelLayout {
    std::size_t body_index{};
    std::optional<std::size_t> title_index;
    std::vector<std::size_t> action_indices;
};

class DialogPanelComponent final : public Component, public ThemeBinding {
public:
    DialogPanelComponent(
        DialogPanelLayout layout,
        std::shared_ptr<ScrollState> body_scroll,
        std::function<void()> on_default)
        : layout_(std::move(layout)),
          body_scroll_(std::move(body_scroll)),
          on_default_(std::move(on_default)) {}

    // The nested focus scope lets T061 first enter this Dialog as one modal
    // unit, then select its first logical descendant. build_content() orders an
    // enabled Default action first when one exists; otherwise body descendants
    // are first. If there is no focusable descendant the panel itself remains
    // focused, while UI-level T063 Escape/programmatic close stay operational.
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return true; }
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override { return 1; }
    [[nodiscard]] bool clips_children() const noexcept override { return true; }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        const auto outer = outer_constraints(constraints);
        const float inner_width = std::isfinite(outer.max.w)
            ? std::max(0.0f, outer.max.w - kDialogPadding * 2.0f)
            : kUnboundedExtent;
        return Constraints::loose({inner_width, kUnboundedExtent});
    }

    [[nodiscard]] ChildMetrics measure_constrained(
        const Constraints& constraints,
        const std::vector<ChildMetrics>& children) const override {
        // Keep the T034 ScrollState alive for every measurement/layout of its
        // borrowed ScrollView descendant; this read also makes the lifetime
        // ownership explicit to warning-clean compilers.
        (void)body_scroll_.get();
        const auto outer = outer_constraints(constraints);
        const auto natural = panel_extent(children, false);
        const auto minimum = outer.constrain(panel_extent(children, true));
        auto preferred = outer.constrain(natural);
        preferred.w = std::max(preferred.w, minimum.w);
        preferred.h = std::max(preferred.h, minimum.h);
        return ChildMetrics{minimum, preferred};
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return panel_extent(children, false);
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return panel_extent(children, true);
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (placements.empty() || layout_.body_index >= placements.size()) return;

        const float inner_x = bounds.x + kDialogPadding;
        const float inner_y = bounds.y + kDialogPadding;
        const float inner_w = std::max(0.0f, bounds.w - kDialogPadding * 2.0f);
        const float inner_h = std::max(0.0f, bounds.h - kDialogPadding * 2.0f);

        const bool has_title = layout_.title_index && *layout_.title_index < children.size();
        const bool has_actions = !layout_.action_indices.empty();
        const float requested_title_h = has_title
            ? children[*layout_.title_index].preferred.h
            : 0.0f;
        const float requested_action_h = action_row_height(children, false);
        const float title_gap = has_title ? kDialogSectionGap : 0.0f;
        const float action_gap = has_actions ? kDialogSectionGap : 0.0f;

        // Fixed chrome wins over the body on tiny viewports. The body receives
        // exactly the remaining bounded height and T034 ScrollView owns any
        // overflow; title/actions never participate in the scroll content.
        float remaining = inner_h;
        const float action_h = std::min(requested_action_h, remaining);
        remaining = std::max(0.0f, remaining - action_h);
        const float effective_action_gap = has_actions
            ? std::min(action_gap, remaining)
            : 0.0f;
        remaining = std::max(0.0f, remaining - effective_action_gap);
        const float title_h = std::min(requested_title_h, remaining);
        remaining = std::max(0.0f, remaining - title_h);
        const float effective_title_gap = has_title
            ? std::min(title_gap, remaining)
            : 0.0f;
        remaining = std::max(0.0f, remaining - effective_title_gap);
        const float body_h = remaining;

        float body_y = inner_y;
        if (has_title) {
            placements[*layout_.title_index].bounds =
                Rect{inner_x, inner_y, inner_w, title_h};
            body_y += title_h + effective_title_gap;
        }
        placements[layout_.body_index].bounds = Rect{inner_x, body_y, inner_w, body_h};

        if (has_actions) {
            const float row_y = inner_y + inner_h - action_h;
            float row_width = 0.0f;
            for (const auto index : layout_.action_indices) {
                if (index >= children.size()) continue;
                if (row_width > 0.0f) row_width += kDialogActionGap;
                row_width += std::min(children[index].preferred.w, inner_w);
            }
            float x = inner_x + std::max(0.0f, inner_w - row_width);
            for (const auto index : layout_.action_indices) {
                if (index >= children.size() || index >= placements.size()) continue;
                const float width = std::min(children[index].preferred.w, inner_w);
                placements[index].bounds = Rect{x, row_y, width, action_h};
                x += width + kDialogActionGap;
            }
        }
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        // Enter reaches this ancestor only after the focused descendant has
        // returned Ignored. TextInput/TextArea and any other child that owns
        // Enter therefore win before the Default action fallback.
        if (event.type == InputType::KeyDown && event.key == Key::Enter && on_default_) {
            auto callback = on_default_;
            callback();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }

    void paint(PaintContext& context) const override {
        const auto& theme = current_theme();
        auto& painter = context.painter();
        painter.fill_rounded_rect(context.bounds(), theme.radii.medium, theme.palette.surface);
        painter.stroke_rounded_rect(
            context.bounds(),
            theme.radii.medium,
            theme.controls.border_width,
            theme.palette.border);
    }

private:
    [[nodiscard]] static Constraints outer_constraints(const Constraints& viewport) noexcept {
        const float maximum_width = viewport.bounded_width()
            ? std::min(kDialogMaximumWidth,
                       std::max(0.0f, viewport.max.w - kDialogViewportMargin * 2.0f))
            : kDialogMaximumWidth;
        const float maximum_height = viewport.bounded_height()
            ? std::max(0.0f, viewport.max.h - kDialogViewportMargin * 2.0f)
            : kUnboundedExtent;
        return Constraints::loose({maximum_width, maximum_height});
    }

    [[nodiscard]] float action_row_width(
        const std::vector<ChildMetrics>& children, bool minimum) const noexcept {
        float width = 0.0f;
        std::size_t count = 0;
        for (const auto index : layout_.action_indices) {
            if (index >= children.size()) continue;
            if (count++) width += kDialogActionGap;
            width += minimum ? children[index].minimum.w : children[index].preferred.w;
        }
        return width;
    }

    [[nodiscard]] float action_row_height(
        const std::vector<ChildMetrics>& children, bool minimum) const noexcept {
        float height = 0.0f;
        for (const auto index : layout_.action_indices) {
            if (index >= children.size()) continue;
            height = std::max(
                height, minimum ? children[index].minimum.h : children[index].preferred.h);
        }
        return height;
    }

    [[nodiscard]] Size panel_extent(
        const std::vector<ChildMetrics>& children, bool minimum) const noexcept {
        const auto extent = [&](std::size_t index) -> Size {
            if (index >= children.size()) return {};
            return minimum ? children[index].minimum : children[index].preferred;
        };

        const auto body = extent(layout_.body_index);
        const auto title = layout_.title_index ? extent(*layout_.title_index) : Size{};
        const float actions_w = action_row_width(children, minimum);
        const float actions_h = action_row_height(children, minimum);
        const float sections = (layout_.title_index ? 1.0f : 0.0f) +
                               (!layout_.action_indices.empty() ? 1.0f : 0.0f);

        return Size{
            std::max({body.w, title.w, actions_w}) + kDialogPadding * 2.0f,
            body.h + title.h + actions_h + sections * kDialogSectionGap +
                kDialogPadding * 2.0f};
    }

    DialogPanelLayout layout_;
    // Own the canonical T034 state at the panel lifetime boundary. ScrollView
    // descendants only borrow it and are destroyed before this parent.
    std::shared_ptr<ScrollState> body_scroll_;
    std::function<void()> on_default_;
};

} // namespace detail

class Dialog {
public:
    using Completion = std::function<void(DialogResult)>;

    explicit Dialog(UI& ui)
        : ui_(&ui), state_(ui.dialog_state_), lifetime_(std::make_shared<int>(0)) {}

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    ~Dialog() {
        // Invalidate every retained action/lifecycle callback before a
        // controller can disappear. Explicit live-UI destruction still follows
        // close() and therefore delivers one Dismissed result; whole-UI teardown
        // is inert because DialogState is marked terminal first.
        lifetime_.reset();
        if (active()) (void)close();
        else clear_local_state();
    }

    [[nodiscard]] DialogShowResult show(DialogSpec spec, Completion completion) {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_) return DialogShowResult::Unavailable;
        if (generation_ != 0 || state->active_generation != 0) return DialogShowResult::Busy;
        if (!valid_spec(spec)) return DialogShowResult::InvalidSpec;

        // Build all allocation-heavy local policy before acquiring the per-UI
        // Dialog slot. A construction failure therefore cannot strand the UI in
        // Busy with no visible overlay. Only show_overlay() remains after the
        // acquire and is rolled back explicitly if it throws.
        const auto escape_result = escape_result_for(spec);
        OverlaySpec overlay;
        overlay.mode = OverlayMode::Modal;
        overlay.pointer_policy = OverlayPointerPolicy::Normal;
        overlay.placement = OverlayPlacement::Center;
        // T063 owns Escape through DialogState/UI before generic T061 routing.
        overlay.dismiss_on_escape = false;
        overlay.dismiss_on_outside_pointer_down = false;
        overlay.content = build_content(std::move(spec));

        const auto generation = state->acquire();
        if (generation == 0) return DialogShowResult::Unavailable;

        generation_ = generation;
        completion_ = std::move(completion);
        if (!state->bind_handlers(
                generation,
                guarded_completion(escape_result),
                guarded_abandon())) {
            (void)state->release(generation);
            clear_local_state();
            return DialogShowResult::Unavailable;
        }

        OverlayHandle handle;
        try {
            handle = ui_->show_overlay(std::move(overlay));
        } catch (...) {
            (void)state->release(generation);
            clear_local_state();
            throw;
        }
        if (!handle.valid()) {
            (void)state->release(generation);
            clear_local_state();
            return DialogShowResult::Unavailable;
        }

        overlay_ = handle;
        return DialogShowResult::Shown;
    }

    [[nodiscard]] bool active() const noexcept {
        if (generation_ == 0 || !overlay_.valid()) return false;
        const auto state = state_.lock();
        return state && !state->ui_tearing_down && state->owns(generation_);
    }

    /// Explicit controller/programmatic close follows the same exactly-once
    /// teardown path as an action but reports Dismissed.
    bool close() {
        return complete(DialogResult{DialogResultKind::Dismissed, {}});
    }

private:
    [[nodiscard]] static bool valid_spec(const DialogSpec& spec) {
        if (!spec.body.factory) return false;

        std::unordered_set<DialogActionId> ids;
        bool default_seen = false;
        bool cancel_seen = false;
        for (const auto& action : spec.actions) {
            if (action.id.empty() || !ids.insert(action.id).second) return false;
            switch (action.role) {
                case DialogActionRole::Normal:
                    break;
                case DialogActionRole::Default:
                    if (default_seen) return false;
                    default_seen = true;
                    break;
                case DialogActionRole::Cancel:
                    if (cancel_seen) return false;
                    cancel_seen = true;
                    break;
            }
        }
        return true;
    }

    [[nodiscard]] static DialogResult escape_result_for(const DialogSpec& spec) {
        for (const auto& action : spec.actions) {
            if (action.role == DialogActionRole::Cancel && action.enabled) {
                return DialogResult{DialogResultKind::Action, action.id};
            }
        }
        return DialogResult{DialogResultKind::Dismissed, {}};
    }

    [[nodiscard]] std::function<void()> guarded_completion(DialogResult result) {
        std::weak_ptr<int> lifetime = lifetime_;
        auto* self = this;
        return [lifetime = std::move(lifetime), self, result = std::move(result)]() mutable {
            if (lifetime.expired()) return;
            (void)self->complete(std::move(result));
        };
    }

    [[nodiscard]] std::function<void()> guarded_abandon() {
        std::weak_ptr<int> lifetime = lifetime_;
        auto* self = this;
        return [lifetime = std::move(lifetime), self] {
            if (lifetime.expired()) return;
            self->abandon_without_completion();
        };
    }

    [[nodiscard]] Spec action_spec(const DialogAction& action) {
        auto button = make_spec(Button{
            action.label,
            guarded_completion(DialogResult{DialogResultKind::Action, action.id})});
        std::vector<Spec> children;
        children.push_back(std::move(button));
        return Spec{
            [enabled = action.enabled] {
                return std::make_unique<detail::DialogFixedEnabledComponent>(enabled);
            },
            std::move(children)};
    }

    [[nodiscard]] Spec build_content(DialogSpec spec) {
        std::optional<std::size_t> default_action;
        std::optional<std::size_t> enabled_default;
        for (std::size_t i = 0; i < spec.actions.size(); ++i) {
            if (spec.actions[i].role == DialogActionRole::Default) {
                default_action = i;
                if (spec.actions[i].enabled) enabled_default = i;
            }
        }

        std::vector<Spec> action_specs;
        action_specs.reserve(spec.actions.size());
        for (const auto& action : spec.actions) action_specs.push_back(action_spec(action));

        std::vector<Spec> children;
        std::vector<std::size_t> action_child_indices(spec.actions.size());
        std::vector<bool> action_moved(spec.actions.size(), false);

        // T061's modal focus scope enters its first available descendant. Put
        // the configured Default action first in retained order so the nested
        // Dialog scope enters it deterministically; layout still renders
        // actions in their original visual row order at the bottom.
        if (default_action) {
            action_child_indices[*default_action] = children.size();
            children.push_back(std::move(action_specs[*default_action]));
            action_moved[*default_action] = true;
        }

        auto body_scroll = std::make_shared<ScrollState>(ScrollAxis::Vertical);
        const auto body_index = children.size();
        children.push_back(make_spec(ScrollView{
            *body_scroll, detail::DialogSpecValue{std::move(spec.body)}}));

        std::optional<std::size_t> title_index;
        if (!spec.title.empty()) {
            title_index = children.size();
            children.push_back(make_spec(
                Label{std::move(spec.title)}.size(18.0f).bold()));
        }

        for (std::size_t i = 0; i < action_specs.size(); ++i) {
            if (action_moved[i]) continue;
            action_child_indices[i] = children.size();
            children.push_back(std::move(action_specs[i]));
        }

        detail::DialogPanelLayout layout;
        layout.body_index = body_index;
        layout.title_index = title_index;
        layout.action_indices.reserve(action_child_indices.size());
        for (const auto index : action_child_indices) layout.action_indices.push_back(index);

        std::function<void()> on_default;
        if (enabled_default) {
            on_default = guarded_completion(DialogResult{
                DialogResultKind::Action, spec.actions[*enabled_default].id});
        }

        Spec panel{
            [layout = std::move(layout),
             body_scroll = std::move(body_scroll),
             on_default = std::move(on_default)]() mutable {
                return std::make_unique<detail::DialogPanelComponent>(
                    std::move(layout),
                    std::move(body_scroll),
                    std::move(on_default));
            },
            std::move(children)};

        std::vector<Spec> backdrop_children;
        backdrop_children.push_back(std::move(panel));
        return Spec{
            [color = spec.backdrop_color] {
                return std::make_unique<detail::DialogBackdropComponent>(color);
            },
            std::move(backdrop_children)};
    }

    bool complete(DialogResult result) {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_ || generation_ == 0 ||
            !state->owns(generation_)) {
            return false;
        }

        // Mark this controller logically closed before touching retained state.
        // Any repeated key/pointer/close attempt therefore becomes a no-op even
        // if the T058 physical detach is deferred to the outer dispatch seam.
        const auto generation = std::exchange(generation_, 0);
        auto overlay = std::exchange(overlay_, OverlayHandle{});
        auto completion = std::move(completion_);

        (void)ui_->close_overlay(std::move(overlay));
        ui_->complete_dialog_close(
            generation,
            [completion = std::move(completion), result = std::move(result)]() mutable {
                if (completion) completion(std::move(result));
            });
        return true;
    }

    void abandon_without_completion() {
        auto state = state_.lock();
        if (!state || state->ui_tearing_down || !ui_ || generation_ == 0 ||
            !state->owns(generation_)) {
            clear_local_state();
            return;
        }

        const auto generation = std::exchange(generation_, 0);
        auto overlay = std::exchange(overlay_, OverlayHandle{});
        completion_ = {};
        (void)ui_->close_overlay(std::move(overlay));
        (void)state->release(generation);
    }

    void clear_local_state() noexcept {
        generation_ = 0;
        overlay_ = {};
        completion_ = {};
    }

    UI* ui_{};
    std::weak_ptr<detail::DialogState> state_;
    std::shared_ptr<int> lifetime_;
    std::uint64_t generation_{};
    OverlayHandle overlay_{};
    Completion completion_;
};

} // namespace ui
