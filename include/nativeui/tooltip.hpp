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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {
struct TooltipStyle {
    std::optional<Color> background, text, border;
    std::optional<float> border_width, radius, padding, max_width;
    std::optional<TextStyle> text_style;
};
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
                      Callback hide);

    TooltipController(const TooltipController&) = delete;
    TooltipController& operator=(const TooltipController&) = delete;
    TooltipController(TooltipController&&) = delete;
    TooltipController& operator=(TooltipController&&) = delete;

    ~TooltipController() noexcept;

    void set_hovered(bool hovered);
    void set_focused(bool focused);

    /// A pointer button interaction anywhere in the owning tree makes hover
    /// ineligible: a plain hover must never arm a tooltip during a drag. The
    /// end of the interaction is deliberately not a new eligibility
    /// transition, so releasing a stationary pointer does not immediately
    /// re-show after a PointerDown dismissal.
    void set_pointer_interaction_active(bool active);

    /// Hidden/Collapsed/Disabled anchors cannot present a tooltip. Losing
    /// availability cancels both pending and visible work and suppresses
    /// stationary eligibility until the retained hover/focus state actually
    /// becomes false and then true again. Availability restoration alone is
    /// therefore never treated as a synthetic hover/focus trigger.
    void set_anchor_available(bool available);

    /// T061 closed the presentation externally (anchor became unavailable,
    /// overlay-stack dismissal, view teardown). Drop the visible flag without
    /// invoking the hide callback because the overlay lifetime already ended.
    void notify_presentation_closed() noexcept;

    /// PointerDown is a terminal dismissal for the current continuous
    /// eligibility interval. Remaining hovered/focused does not silently arm a
    /// new timer; at least one complete ineligible -> eligible transition is
    /// required before another presentation attempt.
    void dismiss_until_eligibility_transition();

    /// Cancel the current pending/visible presentation without installing the
    /// stronger PointerDown suppression rule. A later eligibility transition
    /// may arm a fresh full delay.
    void cancel();

    [[nodiscard]] bool pending() const noexcept;

    [[nodiscard]] bool visible() const noexcept;

    [[nodiscard]] bool eligible() const noexcept;

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

    static void cancel_pending(State& state);

    static void hide_visible(State& state);

    static void arm(const std::shared_ptr<State>& state);

    void set_trigger(Trigger trigger, bool value);

    void shutdown() noexcept;

    std::shared_ptr<State> state_;
};

[[nodiscard]] std::size_t tooltip_utf8_sequence_length(unsigned char lead) noexcept;

/// Deterministic UTF-8 line wrapping for tooltip text. Words are split on
/// ASCII spaces/newlines only, so multi-byte sequences are never split except
/// by a codepoint-aligned hard break for a word wider than the limit.
[[nodiscard]] std::vector<std::string> wrap_tooltip_text(
    std::string_view text, const TextStyle& style, float max_width);

/// Non-interactive presentation surface for one tooltip overlay entry. It
/// resolves deterministic internal defaults that map 1:1 to the future typed
/// TooltipStyle fields (background/text/border/radius/padding/max_width); no
/// temporary public style API is introduced before T038/T039.
class TooltipSurfaceComponent final : public Component, public ThemeBinding {
public:
    TooltipSurfaceComponent(std::string text, float max_width);
    TooltipSurfaceComponent(std::string text, TooltipStyle style);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;

    void paint(PaintContext& context) const override;

private:
    [[nodiscard]] TextStyle text_style() const;
    [[nodiscard]] Color background() const;
    [[nodiscard]] Color border() const;
    [[nodiscard]] float border_width() const;
    [[nodiscard]] float radius() const;
    [[nodiscard]] float padding() const;

    std::string text_;
    float max_width_{};
    TooltipStyle style_;
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
    TooltipComponent(std::string text, std::chrono::milliseconds delay);
    TooltipComponent(std::string text, std::chrono::milliseconds delay, TooltipStyle style);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override;

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override;

    void mount(MountContext& context) override;

    void unmount(LifecycleContext&) override;

    void activate(LifecycleContext&) override;

    void deactivate(LifecycleContext&) override;

    void retained_pointer_hover_changed(
        bool hovered, bool pointer_interaction_active, Dispatcher dispatcher) override;

    void retained_focus_within_changed(
        bool focused, bool pointer_interaction_active, Dispatcher dispatcher) override;

    void dismiss_transient_presentation() override;

    [[nodiscard]] SemanticInfo semantics() const override;

    void paint(PaintContext&) const override;

private:
    [[nodiscard]] std::optional<DescendantSemanticDecoration> descendant_semantic_decoration() const override;
    [[nodiscard]] bool anchor_available() const noexcept;

    void refresh_anchor_availability();

    void ensure_controller(Dispatcher dispatcher);

    void reconcile_presentation();

    void present();

    void hide();

    std::string text_;
    std::chrono::milliseconds delay_;
    TooltipStyle style_;
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

    Tooltip&& style(TooltipStyle value) &&;
    Tooltip&& delay(std::chrono::milliseconds value) && noexcept;

    Tooltip& delay(std::chrono::milliseconds value) & noexcept;

    [[nodiscard]] std::chrono::milliseconds delay() const noexcept;
    [[nodiscard]] const std::string& text() const noexcept;

    Spec spec() &&;

private:
    void set_delay(std::chrono::milliseconds value) noexcept;

    std::string text_;
    std::chrono::milliseconds delay_{kDefaultDelay};
    TooltipStyle style_;
    std::vector<Spec> children_;
};

} // namespace ui
