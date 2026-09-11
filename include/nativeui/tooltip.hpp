#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/interaction_observer.hpp>
#include <nativeui/detail/overlay_service.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/transient_presentation.hpp>
#include <nativeui/dispatcher.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

inline constexpr float kTooltipDefaultMaxWidth = 320.0f;

/// One-shot presentation state machine for a text-only Tooltip.
///
/// The controller owns only eligibility (hover/focus), one timer token and the
/// current visible flag. Timing comes from T065, overlay lifetime from T061.
/// State is per controller; there is deliberately no process-wide warm-up,
/// current-tooltip or shared timing domain.
class TooltipController final {
public:
    using Callback = std::function<void()>;

    TooltipController(Dispatcher dispatcher,
                      DispatcherDuration delay,
                      Callback show,
                      Callback hide)
        : state_(std::make_shared<State>(
              std::move(dispatcher),
              delay.count() < 0.0 ? DispatcherDuration::zero() : delay,
              std::move(show),
              std::move(hide))) {}

    TooltipController(const TooltipController&) = delete;
    TooltipController& operator=(const TooltipController&) = delete;
    TooltipController(TooltipController&&) = delete;
    TooltipController& operator=(TooltipController&&) = delete;

    ~TooltipController() { shutdown(); }

    void set_hovered(bool hovered) { set_trigger(Trigger::Hover, hovered); }
    void set_focused(bool focused) { set_trigger(Trigger::Focus, focused); }

    /// A pointer button interaction anywhere in the owning tree makes hover
    /// ineligible: a plain hover must never arm a tooltip during a drag. The
    /// end of the interaction is deliberately not a new eligibility
    /// transition, so releasing a stationary pointer does not immediately
    /// re-show after a PointerDown dismissal.
    void set_pointer_interaction_active(bool active) {
        if (!state_ || state_->shutting_down ||
            state_->pointer_interaction_active == active) {
            return;
        }

        state_->pointer_interaction_active = active;
        if (active) {
            state_->suppressed = true;
            cancel_pending(*state_);
            hide_visible(*state_);
        }
    }

    /// Hidden/Collapsed/Disabled anchors cannot present a tooltip. Losing
    /// availability cancels both pending and visible work and suppresses
    /// stationary eligibility until the retained hover/focus state actually
    /// becomes false and then true again. Availability restoration alone is
    /// therefore never treated as a synthetic hover/focus trigger.
    void set_anchor_available(bool available) {
        if (!state_ || state_->shutting_down || state_->anchor_available == available) {
            return;
        }

        state_->anchor_available = available;
        if (!available) {
            state_->suppressed = true;
            cancel_pending(*state_);
            hide_visible(*state_);
        }
    }

    /// T061 closed the presentation externally (anchor became unavailable,
    /// overlay-stack dismissal, view teardown). Drop the visible flag without
    /// invoking the hide callback because the overlay lifetime already ended.
    void notify_presentation_closed() noexcept {
        if (!state_ || !state_->visible) return;
        state_->visible = false;
    }

    /// PointerDown is a terminal dismissal for the current continuous
    /// eligibility interval. Remaining hovered/focused does not silently arm a
    /// new timer; at least one complete ineligible -> eligible transition is
    /// required before another presentation attempt.
    void dismiss_until_eligibility_transition() {
        if (!state_) return;
        state_->suppressed = true;
        cancel_pending(*state_);
        hide_visible(*state_);
    }

    /// Cancel the current pending/visible presentation without installing the
    /// stronger PointerDown suppression rule. A later eligibility transition
    /// may arm a fresh full delay.
    void cancel() {
        if (!state_) return;
        cancel_pending(*state_);
        hide_visible(*state_);
    }

    [[nodiscard]] bool pending() const noexcept {
        return state_ && state_->timer.valid();
    }

    [[nodiscard]] bool visible() const noexcept {
        return state_ && state_->visible;
    }

    [[nodiscard]] bool eligible() const noexcept {
        return state_ && state_->eligible();
    }

private:
    enum class Trigger {
        Hover,
        Focus,
    };

    struct State final {
        State(Dispatcher dispatcher_value,
              DispatcherDuration delay_value,
              Callback show_value,
              Callback hide_value)
            : dispatcher(std::move(dispatcher_value)),
              delay(delay_value),
              show(std::move(show_value)),
              hide(std::move(hide_value)) {}

        [[nodiscard]] bool triggered() const noexcept { return hovered || focused; }
        [[nodiscard]] bool eligible() const noexcept {
            return anchor_available && triggered() && !pointer_interaction_active;
        }

        Dispatcher dispatcher;
        DispatcherDuration delay{};
        Callback show;
        Callback hide;
        TimerHandle timer;
        bool hovered{};
        bool focused{};
        bool anchor_available{true};
        bool pointer_interaction_active{};
        bool visible{};
        bool suppressed{};
        bool shutting_down{};
    };

    static void cancel_pending(State& state) {
        if (!state.timer.valid()) return;
        (void)state.dispatcher.cancel(state.timer);
        state.timer = {};
    }

    static void hide_visible(State& state) {
        if (!state.visible) return;
        state.visible = false;
        auto hide = state.hide;
        if (hide) hide();
    }

    static void arm(const std::shared_ptr<State>& state) {
        if (!state || state->shutting_down || state->suppressed ||
            !state->eligible() || state->visible || state->timer.valid()) {
            return;
        }

        std::weak_ptr<State> weak = state;
        state->timer = state->dispatcher.schedule_after(state->delay, [weak] {
            const auto locked = weak.lock();
            if (!locked) return;

            locked->timer = {};
            if (locked->shutting_down || locked->suppressed ||
                !locked->eligible() || locked->visible) {
                return;
            }

            // Publish visibility before application/UI integration code runs so
            // a reentrant dismissal cannot observe a half-shown controller.
            locked->visible = true;
            auto show = locked->show;
            if (show) show();
        });
    }

    void set_trigger(Trigger trigger, bool value) {
        if (!state_ || state_->shutting_down) return;

        const bool was_eligible = state_->eligible();
        bool& field = trigger == Trigger::Hover ? state_->hovered : state_->focused;
        if (field == value) return;
        field = value;
        const bool is_eligible = state_->eligible();

        if (!is_eligible) {
            cancel_pending(*state_);
            hide_visible(*state_);
            // Only the retained hover/focus state reaching false clears
            // suppression. Availability restoration itself is deliberately not
            // a new presentation trigger, and neither is the end of a pointer
            // button interaction.
            if (!state_->triggered() && !state_->pointer_interaction_active) {
                state_->suppressed = false;
            }
            return;
        }

        if (!was_eligible && is_eligible) {
            state_->suppressed = false;
            arm(state_);
        }
    }

    void shutdown() noexcept {
        if (!state_) return;
        state_->shutting_down = true;
        cancel_pending(*state_);
        hide_visible(*state_);
        state_->show = {};
        state_->hide = {};
        state_.reset();
    }

    std::shared_ptr<State> state_;
};

[[nodiscard]] inline std::size_t tooltip_utf8_sequence_length(unsigned char lead) noexcept {
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xE) return 3;
    if ((lead >> 3) == 0x1E) return 4;
    return 1;
}

/// Deterministic UTF-8 line wrapping for tooltip text. Words are split on
/// ASCII spaces/newlines only, so multi-byte sequences are never split except
/// by a codepoint-aligned hard break for a word wider than the limit.
[[nodiscard]] inline std::vector<std::string> wrap_tooltip_text(
    std::string_view text, const TextStyle& style, float max_width) {
    const float limit = max_width > 0.0f ? max_width : 1.0f;
    std::vector<std::string> lines;
    std::string current;

    auto flush_current = [&] {
        while (!current.empty() && current.back() == ' ') current.pop_back();
        lines.push_back(std::move(current));
        current.clear();
    };
    auto fits = [&](std::string_view candidate) {
        return TextService::measure(candidate, style).width <= limit;
    };

    std::size_t index = 0;
    while (index < text.size()) {
        const char lead = text[index];
        if (lead == '\n') {
            flush_current();
            ++index;
            continue;
        }
        if (lead == ' ') {
            ++index;
            continue;
        }

        const std::size_t word_start = index;
        while (index < text.size() && text[index] != ' ' && text[index] != '\n') {
            const auto length = tooltip_utf8_sequence_length(
                static_cast<unsigned char>(text[index]));
            index += std::min(length, text.size() - index);
        }
        std::string_view word = text.substr(word_start, index - word_start);

        if (!current.empty()) {
            std::string candidate = current;
            candidate.push_back(' ');
            candidate.append(word);
            if (!fits(candidate)) flush_current();
        }

        if (current.empty() && !fits(word)) {
            // Hard-break an unbreakable word on codepoint boundaries.
            while (!word.empty()) {
                std::size_t bytes = 0;
                while (bytes < word.size()) {
                    const auto length = std::min(
                        tooltip_utf8_sequence_length(static_cast<unsigned char>(word[bytes])),
                        word.size() - bytes);
                    if (bytes > 0 && !fits(word.substr(0, bytes + length))) break;
                    bytes += length;
                }
                if (bytes == 0) bytes = 1;
                if (bytes >= word.size()) {
                    current.assign(word);
                    word = {};
                    break;
                }
                lines.push_back(std::string{word.substr(0, bytes)});
                word.remove_prefix(bytes);
            }
            continue;
        }

        if (!current.empty()) current.push_back(' ');
        current.append(word);
    }

    while (!current.empty() && current.back() == ' ') current.pop_back();
    if (!current.empty() || lines.empty()) lines.push_back(std::move(current));
    return lines;
}

/// Non-interactive presentation surface for one tooltip overlay entry. It
/// resolves deterministic internal defaults that map 1:1 to the future typed
/// TooltipStyle fields (background/text/border/radius/padding/max_width); no
/// temporary public style API is introduced before T038/T039.
class TooltipSurfaceComponent final : public Component, public ThemeBinding {
public:
    TooltipSurfaceComponent(std::string text, float max_width)
        : text_(std::move(text)), max_width_(max_width) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        const auto style = text_style();
        const float content_max = std::max(1.0f, max_width_ - padding() * 2.0f);
        const auto lines = wrap_tooltip_text(text_, style, content_max);
        float width = 0.0f;
        for (const auto& line : lines) {
            width = std::max(width, TextService::measure(line, style).width);
        }
        const float line_height = TextService::measure("Ag", style).height;
        return Size{
            width + padding() * 2.0f,
            static_cast<float>(lines.size()) * line_height + padding() * 2.0f};
    }

    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto style = text_style();
        auto& painter = context.painter();

        painter.fill_rounded_rect(bounds, radius(), background());
        if (border_width() > 0.0f) {
            painter.stroke_rounded_rect(bounds, radius(), border_width(), border());
        }

        const float inset = padding();
        const float content_max = std::max(1.0f, max_width_ - inset * 2.0f);
        const auto lines = wrap_tooltip_text(text_, style, content_max);
        const float line_height = TextService::measure("Ag", style).height;
        TextStyle line_style = style;
        line_style.align = TextAlign::Left;
        float center_y = bounds.y + inset + line_height * 0.5f;
        for (const auto& line : lines) {
            painter.text({bounds.x + inset, center_y}, line, line_style);
            center_y += line_height;
        }
    }

private:
    [[nodiscard]] TextStyle text_style() const {
        const auto& theme = current_theme();
        TextStyle style{};
        style.size = theme.typography.control_size;
        style.color = theme.palette.text;
        return style;
    }
    [[nodiscard]] Color background() const { return current_theme().palette.surface; }
    [[nodiscard]] Color border() const { return current_theme().palette.border; }
    [[nodiscard]] float border_width() const { return current_theme().controls.border_width; }
    [[nodiscard]] float radius() const { return current_theme().radii.sm; }
    [[nodiscard]] float padding() const { return current_theme().spacing.sm; }

    std::string text_;
    float max_width_{};
};

/// Text-only Tooltip decorator.
///
/// Responsibilities are deliberately narrow:
/// - plain owned UTF-8 text and the configured delay;
/// - hover/focus eligibility and one T065 timer token (TooltipController);
/// - one T061 NonModal/Auto/non-hit-test overlay whose placement/clamping is
///   owned entirely by T061.
///
/// It never renders into the anchor subtree, never captures pointer/focus and
/// keeps all state per decorated instance.
class TooltipComponent final : public Component,
                               public RetainedInteractionObserver,
                               public TransientPresentation {
public:
    TooltipComponent(std::string text, std::chrono::milliseconds delay)
        : text_(std::move(text)),
          delay_(delay.count() < 0 ? std::chrono::milliseconds{0} : delay) {}

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

    void mount(MountContext& context) override {
        node_id_ = context.node_id();
        overlay_service_ = context.overlay_service();
        layout_invalidator_ = context.layout_invalidator();
        mounted_ = true;
    }

    void unmount(LifecycleContext&) override {
        mounted_ = false;
        // Retained teardown must not invalidate a dying UI or close an overlay
        // through a platform callback. Drop the borrowed seams first; T061
        // removes any still-registered anchored overlay with the UI/overlay
        // state itself.
        overlay_service_ = nullptr;
        layout_invalidator_ = {};
        controller_.reset();
        overlay_ = {};
    }

    void activate(LifecycleContext&) override {
        reconcile_presentation();
        refresh_anchor_availability();
    }

    void deactivate(LifecycleContext&) override {
        dismiss_transient_presentation();
        overlay_ = {};
    }

    void retained_pointer_hover_changed(
        bool hovered, bool pointer_interaction_active, Dispatcher dispatcher) override {
        reconcile_presentation();
        refresh_anchor_availability();
        hovered_ = hovered;
        pointer_interaction_active_ = pointer_interaction_active;
        ensure_controller(dispatcher);
        if (controller_) {
            controller_->set_pointer_interaction_active(pointer_interaction_active_);
            controller_->set_hovered(hovered);
        }
    }

    void retained_focus_within_changed(
        bool focused, bool pointer_interaction_active, Dispatcher dispatcher) override {
        reconcile_presentation();
        refresh_anchor_availability();
        focused_ = focused;
        pointer_interaction_active_ = pointer_interaction_active;
        ensure_controller(dispatcher);
        if (controller_) {
            controller_->set_pointer_interaction_active(pointer_interaction_active_);
            controller_->set_focused(focused);
        }
    }

    void dismiss_transient_presentation() override {
        if (controller_) controller_->dismiss_until_eligibility_transition();
    }

    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        if (!text_.empty()) info.description = text_;
        return info;
    }

    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] bool anchor_available() const noexcept {
        return effective_availability().interactive();
    }

    void refresh_anchor_availability() {
        if (!controller_) return;
        controller_->set_anchor_available(anchor_available());
    }

    void ensure_controller(Dispatcher dispatcher) {
        if (controller_ || text_.empty() || !dispatcher.valid()) return;

        const auto delay = std::chrono::duration<double>(delay_);
        controller_ = std::make_shared<TooltipController>(
            dispatcher,
            delay,
            [this] { present(); },
            [this] { hide(); });
        controller_->set_anchor_available(anchor_available());
        controller_->set_pointer_interaction_active(pointer_interaction_active_);
        if (hovered_) controller_->set_hovered(true);
        if (focused_) controller_->set_focused(true);
    }

    void reconcile_presentation() {
        if (controller_ && controller_->visible() && !overlay_.valid()) {
            controller_->notify_presentation_closed();
        }
    }

    void present() {
        if (!mounted_ || text_.empty() || !overlay_service_) {
            if (controller_) controller_->notify_presentation_closed();
            return;
        }
        if (!anchor_available()) {
            // Availability can change while the timer is in flight. Cancel and
            // suppress stationary eligibility instead of showing for a
            // Hidden/Collapsed/Disabled anchor.
            controller_->set_anchor_available(false);
            return;
        }
        if (overlay_.valid()) return;

        OverlaySpec overlay;
        overlay.mode = OverlayMode::NonModal;
        overlay.pointer_policy = OverlayPointerPolicy::Ignore;
        overlay.anchor = node_id_;
        overlay.placement = OverlayPlacement::Auto;
        overlay.dismiss_on_escape = false;
        overlay.dismiss_on_outside_pointer_down = false;
        std::string text = text_;
        overlay.content = Spec{
            [text = std::move(text)]() mutable {
                return std::make_unique<TooltipSurfaceComponent>(
                    text, kTooltipDefaultMaxWidth);
            },
            {}};
        overlay_ = overlay_service_->present(std::move(overlay));
        if (layout_invalidator_) layout_invalidator_();
    }

    void hide() {
        if (overlay_service_ && overlay_.valid()) {
            (void)overlay_service_->dismiss(overlay_);
        }
        overlay_ = {};
        if (layout_invalidator_) layout_invalidator_();
    }

    std::string text_;
    std::chrono::milliseconds delay_;
    NodeId node_id_{kInvalidNodeId};
    OverlayService* overlay_service_{};
    std::function<void()> layout_invalidator_;
    std::shared_ptr<TooltipController> controller_;
    OverlayHandle overlay_;
    bool mounted_{};
    bool hovered_{};
    bool focused_{};
    bool pointer_interaction_active_{};
};

} // namespace detail

/// Retained Tooltip decorator. V1 content is plain UTF-8 text only.
///
/// ```cpp
/// ui::Tooltip{"Reset to default", child}
///     .delay(std::chrono::milliseconds{500});
/// ```
class Tooltip {
public:
    static constexpr std::chrono::milliseconds kDefaultDelay{500};
    static constexpr float kDefaultMaxWidth = detail::kTooltipDefaultMaxWidth;

    template <class Child>
    Tooltip(std::string text, Child&& child)
        : text_(std::move(text)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Tooltip&& delay(std::chrono::milliseconds value) && noexcept {
        set_delay(value);
        return std::move(*this);
    }

    Tooltip& delay(std::chrono::milliseconds value) & noexcept {
        set_delay(value);
        return *this;
    }

    [[nodiscard]] std::chrono::milliseconds delay() const noexcept { return delay_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

    Spec spec() && {
        auto text = std::move(text_);
        const auto delay_value = delay_;
        return Spec{
            [text = std::move(text), delay_value] {
                return std::make_unique<detail::TooltipComponent>(text, delay_value);
            },
            std::move(children_)};
    }

private:
    void set_delay(std::chrono::milliseconds value) noexcept {
        delay_ = value.count() < 0 ? std::chrono::milliseconds{0} : value;
    }

    std::string text_;
    std::chrono::milliseconds delay_{kDefaultDelay};
    std::vector<Spec> children_;
};

} // namespace ui
