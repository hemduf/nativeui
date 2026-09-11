#pragma once

#include <nativeui/component.hpp>
#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

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

        [[nodiscard]] bool eligible() const noexcept { return hovered || focused; }

        Dispatcher dispatcher;
        DispatcherDuration delay{};
        Callback show;
        Callback hide;
        TimerHandle timer;
        bool hovered{};
        bool focused{};
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
            // Reaching an actually ineligible state satisfies the suppression
            // half of the required false -> true transition.
            state_->suppressed = false;
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

class TooltipComponent final : public Component {
public:
    TooltipComponent(std::string text, std::chrono::milliseconds delay)
        : text_(std::move(text)), delay_(delay) {}

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
    std::string text_;
    std::chrono::milliseconds delay_;
};

} // namespace detail

class Tooltip {
public:
    static constexpr std::chrono::milliseconds kDefaultDelay{500};
    static constexpr float kDefaultMaxWidth = 320.0f;

    template <class Child>
    Tooltip(std::string text, Child&& child)
        : text_(std::move(text)) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Tooltip&& delay(std::chrono::milliseconds value) && noexcept {
        delay_ = value.count() < 0 ? std::chrono::milliseconds{0} : value;
        return std::move(*this);
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
    std::string text_;
    std::chrono::milliseconds delay_{kDefaultDelay};
    std::vector<Spec> children_;
};

} // namespace ui
