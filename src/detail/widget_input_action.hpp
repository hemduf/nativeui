#pragma once
#include <cmath>
#include <nativeui/button.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <stdexcept>
namespace ui::detail {
inline void validate_control_extent(float value) {
  if (!std::isfinite(value) || value < 0.0f)
    throw std::invalid_argument(
        "Control metrics must be finite and nonnegative");
}
// Internal sub-part reuses the Button-family activation state machine. Its
// owner controls tab participation and generation, without a second tab stop
// for SearchField clear or stale navigation after a dataset change.
struct InputActionState {
  PressActivationState press;
  std::function<void()> action;
  std::function<void(InputContext &)> context_action;
  std::function<bool()> enabled;
  std::function<std::uint64_t()> generation;
  std::function<void()> release;
  std::uint64_t armed_generation{};
  bool mounted{};
  bool focused{};
};
class InputAction final : public Component, public ThemeBinding {
public:
  InputAction(std::string name, std::string glyph, ButtonStyle style,
              std::shared_ptr<InputActionState> state, bool tab,
              bool visibility_action, std::function<bool()> visible = {})
      : name_(std::move(name)), glyph_(std::move(glyph)),
        style_(std::move(style)), state_(std::move(state)), tab_(tab),
        visibility_action_(visibility_action), visible_(std::move(visible)) {}
  bool focusable() const noexcept override { return tab_; }
  bool pointer_targetable() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override {
    return !visibility_action_;
  }
  ComponentAvailability local_availability() const noexcept override {
    return {visible_ && !visible_() ? VisibilityMode::Hidden
                                    : VisibilityMode::Visible,
            !state_->enabled || state_->enabled(), false};
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto style = resolved();
    return {std::max(style.minimum_width,
                     TextService::measure(glyph_, style.text_size).width +
                         2.0f * style.horizontal_padding),
            style.control_height};
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    return measure(children);
  }
  void mount(MountContext &) override { state_->mounted = true; }
  void unmount(LifecycleContext &) override {
    stop();
    state_->mounted = false;
  }
  void deactivate(LifecycleContext &) override {
    stop();
    state_->focused = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    state_->focused = focused;
    if (!focused)
      stop();
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    const bool allowed = effective_enabled() &&
                         (visibility_action_ || !effective_read_only()) &&
                         (!state->enabled || state->enabled());
    if (!allowed) {
      stop();
      return EventResult::Ignored;
    }
    if (event.type == InputType::PointerDown ||
        event.type == InputType::KeyDown) {
      if (event.type == InputType::PointerDown)
        state->release = context.pointer_releaser();
      if (!state->press.pressed())
        state->armed_generation = state->generation ? state->generation() : 0;
    }
    PressActivationResult outcome;
    try {
      outcome = state->press.input(event, context, true);
    } catch (...) {
      stop_state(state);
      throw;
    }
    if (event.type == InputType::PointerUp ||
        event.type == InputType::PointerCancel)
      state->release = {};
    if (!outcome.activate)
      return outcome.result;
    const auto generation = state->armed_generation;
    auto callback = state->action;
    auto context_callback = state->context_action;
    if (state->mounted && (!state->enabled || state->enabled()) &&
        (!state->generation || generation == state->generation()) &&
        (callback || context_callback)) {
      if (context_callback)
        context_callback(context);
      else
        callback();
    }
    return outcome.result;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = name_;
    info.focusable = tab_;
    info.focused = state_->focused;
    info.enabled = effective_enabled();
    info.read_only = !visibility_action_ && effective_read_only();
    if (info.enabled && !info.read_only) {
      info.actions = {SemanticAction::Activate};
      if (tab_)
        info.actions.push_back(SemanticAction::Focus);
    }
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto style = resolved();
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    auto clip = painter.scoped_clip(bounds);
    painter.fill_rounded_rect(bounds, style.corner_radius, style.fill);
    painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                                style.border);
    TextStyle text;
    text.size = style.text_size;
    text.color = style.text;
    text.align = TextAlign::Center;
    text.weight = style.text_weight;
    text.slant = style.text_slant;
    text.family = style.font_family;
    text.fallback_families = style.fallback_families;
    painter.text({bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
                 glyph_, text);
  }

private:
  static void
  stop_state(const std::shared_ptr<InputActionState> &state) noexcept {
    state->press = PressActivationState{};
    auto release = std::exchange(state->release, {});
    if (release) {
      try {
        release();
      } catch (...) {
      }
    }
  }
  void stop() noexcept { stop_state(state_); }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    if (!after.interactive() || (after.read_only && !visibility_action_))
      stop();
  }
  ResolvedButtonStyle resolved() const {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = !visibility_action_ && effective_read_only();
    visual.focused = state_->focused;
    visual.hovered = state_->press.hovered();
    visual.pressed = state_->press.pressed();
    return resolve_button_style(default_button_style(current_theme()), style_,
                                visual);
  }
  std::string name_, glyph_;
  ButtonStyle style_;
  std::shared_ptr<InputActionState> state_;
  bool tab_{}, visibility_action_{};
  std::function<bool()> visible_;
};
} // namespace ui::detail
