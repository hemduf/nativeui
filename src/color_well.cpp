#include "detail/widget_color.hpp"
#include <cmath>
#include <limits>
#include <nativeui/color_well.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
namespace ui {
namespace {
float logical(double value) {
  return static_cast<float>(
      std::min(value, double(std::numeric_limits<float>::max())));
}
struct WellRuntime : std::enable_shared_from_this<WellRuntime> {
  explicit WellRuntime(Binding<Color> value)
      : source(std::move(value)), preview(detail::color_preview(source.get())) {
  }
  Binding<Color> source;
  detail::ColorPreview preview;
  std::function<void(Color)> callback;
  std::function<void()> invalidate;
  std::function<bool()> guard;
  std::optional<detail::OverlayComponentCommand> pending;
  OverlayHandle handle;
  NodeId node{kInvalidNodeId};
  std::uint64_t generation{};
  bool mounted{}, mutable_value{true}, live{};
  bool allowed() const {
    return mounted && mutable_value && source.valid() && (!guard || guard());
  }
  void sync() {
    if (!mounted)
      return;
    if (!source.valid())
      live = false;
    const auto next = detail::color_preview(source.get());
    if (next.hex != preview.hex || next.invalid != preview.invalid ||
        next.color != preview.color) {
      preview = next;
      if (invalidate)
        invalidate();
    }
  }
  void close() {
    if (!live && !handle.valid() && !pending)
      return;
    const auto old = handle.valid() ? handle
                     : pending      ? pending->handle
                                    : OverlayHandle{};
    ++generation;
    live = false;
    handle = {};
    pending =
        old.valid()
            ? std::optional{detail::OverlayComponentCommand::close_then_invoke(
                  old, node, false, {})}
            : std::nullopt;
    if (invalidate)
      invalidate();
  }
};
class WellFrame final : public Component,
                        public detail::ThemeBinding,
                        public detail::OverlayCommandSource,
                        public detail::OverlayAnchorPolicy {
public:
  WellFrame(std::string label, Binding<Color> source, bool alpha,
            std::vector<ColorSwatch> swatches,
            std::function<void(Color)> callback, ColorWellStyle style)
      : label_(std::move(label)), alpha_(alpha), swatches_(std::move(swatches)),
        style_(std::move(style)),
        state_(std::make_shared<WellRuntime>(std::move(source))) {
    state_->callback = std::move(callback);
  }
  bool focusable() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool dismiss_overlay_when_read_only() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override { return state_->live; }
  Size measure(const std::vector<ChildMetrics> &) const override {
    return {logical(style_.preview_width + 2 * style_.padding),
            logical(style_.preview_height + 2 * style_.padding)};
  }
  void mount(MountContext &context) override {
    state_->mounted = true;
    state_->node = context.node_id();
    state_->guard = detail::InputMutationAccess::guard(context);
    state_->invalidate = context.invalidator();
    const std::weak_ptr<WellRuntime> weak = state_;
    subscription_ = state_->source.observe([weak](const auto &) {
      if (const auto state = weak.lock())
        state->sync();
    });
  }
  void unmount(LifecycleContext &) override {
    state_->mounted = false;
    ++state_->generation;
    state_->live = false;
    state_->pending.reset();
    state_->handle = {};
    state_->guard = {};
    state_->invalidate = {};
    subscription_.reset();
  }
  void deactivate(LifecycleContext &context) override {
    press_.deactivate(context, false);
    state_->close();
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    press_.focus_changed(focused, context, false);
    context.invalidate();
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    state->sync();
    if (!state->allowed()) {
      press_.cancel_pending_mutation(context, false);
      return EventResult::Handled;
    }
    const auto outcome = press_.input(event, context, true);
    if (outcome.activate) {
      if (state->live)
        state->close();
      else
        open(context);
    }
    return outcome.result;
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action == SemanticAction::Activate) {
      if (state_->live)
        state_->close();
      else
        open(context);
      return EventResult::Handled;
    }
    return EventResult::Ignored;
  }
  std::optional<detail::OverlayComponentCommand>
  take_overlay_command() override {
    return std::exchange(state_->pending, {});
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = label_;
    info.text_value = state_->preview.hex;
    info.description =
        state_->preview.invalid ? "Invalid color" : state_->preview.hex;
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !state_->source.valid();
    info.expanded = state_->live && state_->handle.valid()
                        ? SemanticExpandedState::Expanded
                        : SemanticExpandedState::Collapsed;
    if (info.enabled)
      info.actions = {SemanticAction::Focus};
    if (info.enabled && !info.read_only)
      info.actions.push_back(SemanticAction::Activate);
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto bounds = context.bounds();
    auto &p = context.painter();
    auto clip = p.scoped_clip(bounds, logical(style_.corner_radius));
    p.fill_rounded_rect(
        bounds, logical(style_.corner_radius),
        style_.background.value_or(current_theme().palette.control_background));
    const float padding =
        std::min({logical(style_.padding), std::max(0.f, bounds.w * .5f),
                  std::max(0.f, bounds.h * .5f)});
    const float width = std::min(logical(style_.preview_width),
                                 std::max(0.f, bounds.w - 2 * padding)),
                height = std::min(logical(style_.preview_height),
                                  std::max(0.f, bounds.h - 2 * padding));
    const auto scale = std::min(
        style_.preview_width > 0 ? width / logical(style_.preview_width) : 0,
        style_.preview_height > 0 ? height / logical(style_.preview_height)
                                  : 0);
    const float w = logical(style_.preview_width) * scale,
                h = logical(style_.preview_height) * scale;
    detail::paint_color_preview(
        p,
        {bounds.x + (bounds.w - w) * .5f, bounds.y + (bounds.h - h) * .5f, w,
         h},
        state_->preview.color, style_.corner_radius, style_.checker_size,
        style_.checker_light.value_or(Color{.8f, .8f, .8f, 1}),
        style_.checker_dark.value_or(Color{.5f, .5f, .5f, 1}));
    p.stroke_rounded_rect(
        bounds, logical(style_.corner_radius), logical(style_.border_width),
        focused_ ? style_.focus.value_or(current_theme().palette.focus)
                 : style_.border.value_or(current_theme().palette.border));
  }

private:
  void open(InputContext &context) {
    const auto state = state_;
    if (!state->allowed() || state->pending)
      return;
    const auto serial = ++state->generation;
    const std::weak_ptr<WellRuntime> weak = state;
    auto permission = [weak, serial] {
      const auto current = weak.lock();
      return current && current->allowed() && current->live &&
             current->generation == serial;
    };
    auto changed = [weak, serial](Color value) {
      const auto current = weak.lock();
      if (!current || !current->allowed() || current->generation != serial ||
          current->source.get() != value)
        return;
      auto callback = current->callback;
      if (current->allowed() && current->generation == serial &&
          current->source.get() == value && callback)
        callback(value);
    };
    auto picker = detail::color_picker_spec(
        label_, state->source, alpha_, swatches_, std::move(changed),
        ColorPickerStyle{}, std::move(permission));
    OverlaySpec overlay;
    overlay.anchor = state->node;
    overlay.mode = OverlayMode::Modal;
    overlay.placement = OverlayPlacement::Auto;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = std::move(picker);
    context.invalidate();
    if (!state->allowed() || state->generation != serial ||
        !detail::InputMutationAccess::allowed(context))
      return;
    state->live = true;
    state->pending = detail::OverlayComponentCommand::show(
        std::move(overlay), [weak, serial](OverlayHandle handle) {
          const auto current = weak.lock();
          if (!current)
            return;
          if (!current->allowed() || current->generation != serial ||
              !current->live) {
            if (handle.valid())
              current->pending =
                  detail::OverlayComponentCommand::close_then_invoke(
                      handle, current->node, false, {});
            return;
          }
          current->handle = handle;
          if (!handle.valid())
            current->live = false;
        });
  }
  void retained_checkpoint() override {
    state_->sync();
    if (!state_->handle.valid() && !state_->pending)
      state_->live = false;
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->mutable_value = after.interactive() && !after.read_only;
    if (!state_->mutable_value)
      state_->live = false;
  }
  std::string label_;
  bool alpha_{};
  std::vector<ColorSwatch> swatches_;
  ColorWellStyle style_;
  std::shared_ptr<WellRuntime> state_;
  Binding<Color>::Subscription subscription_;
  detail::PressActivationState press_;
  bool focused_{};
};
} // namespace
ColorWell::ColorWell(std::string label, Binding<Color> value)
    : label_(std::move(label)), value_(std::move(value)) {}
ColorWell::ColorWell(std::string label, State<Color> &value)
    : ColorWell(std::move(label), value.binding()) {}
ColorWell &&ColorWell::alpha_enabled(bool value) && {
  alpha_ = value;
  return std::move(*this);
}
ColorWell &&ColorWell::swatches(std::vector<ColorSwatch> value) && {
  detail::validate_swatches(value);
  swatches_ = std::move(value);
  return std::move(*this);
}
ColorWell &&ColorWell::on_change(std::function<void(Color)> value) && {
  callback_ = std::move(value);
  return std::move(*this);
}
ColorWell &&ColorWell::style(ColorWellStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec ColorWell::spec() && {
  detail::validate_swatches(swatches_);
  for (double metric :
       {style_.preview_width, style_.preview_height, style_.padding,
        style_.corner_radius, style_.border_width, style_.checker_size})
    if (!std::isfinite(metric) || metric < 0)
      throw std::invalid_argument(
          "ColorWell metrics must be finite and nonnegative");
  return Spec{[label = label_, value = value_, alpha = alpha_,
               swatches = swatches_, callback = callback_, style = style_] {
                return std::make_unique<WellFrame>(label, value, alpha,
                                                   swatches, callback, style);
              },
              {}};
}
} // namespace ui
