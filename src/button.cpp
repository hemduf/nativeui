#include <nativeui/button.hpp>

namespace ui {

namespace detail {
ButtonComponent::ButtonComponent(std::string label,
                                 ActivateCallback on_activate,
                                 ButtonStyle style)
    : ButtonComponent(std::move(label), std::move(on_activate),
                      std::move(style), ButtonVariant::Standard, false) {}

ButtonComponent::ButtonComponent(std::string label,
                                 ActivateCallback on_activate,
                                 ButtonStyle style, ButtonVariant variant,
                                 bool has_content)
    : label_(std::move(label)), on_activate_(std::move(on_activate)),
      style_(std::move(style)), variant_(variant), has_content_(has_content) {}

bool ButtonComponent::roving_focus_target() const noexcept { return true; }

void ButtonComponent::bind_group_style(
    std::shared_ptr<const GroupButtonStyle> style) noexcept {
  group_style_ = std::move(style);
}

bool ButtonComponent::focusable() const noexcept { return true; }

Size ButtonComponent::measure(const std::vector<ChildMetrics> &children) const {
  const auto resolved = resolved_style(focused_);
  if (has_content_ && !children.empty()) {
    return {std::max(resolved.minimum_width,
                     children.front().preferred.w +
                         resolved.horizontal_padding * 2.0f),
            std::max(resolved.control_height, children.front().preferred.h)};
  }
  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;
  const auto text = TextService::measure(label_, text_style);
  return Size{std::max(resolved.minimum_width,
                       text.width + resolved.horizontal_padding * 2.0f),
              resolved.control_height};
}

bool ButtonComponent::clips_children() const noexcept { return has_content_; }
bool ButtonComponent::allows_child_interaction() const noexcept {
  return !has_content_;
}
Constraints ButtonComponent::child_constraints(const Constraints &constraints,
                                               std::size_t, std::size_t) const {
  if (!has_content_)
    return constraints.loosen();
  return constraints.inset(resolved_style(focused_).horizontal_padding, 0.0f)
      .loosen();
}
void ButtonComponent::layout_children(
    Rect bounds, const std::vector<ChildMetrics> &children,
    std::vector<ChildPlacement> &placements) const {
  if (!has_content_ || children.empty() || placements.empty())
    return;
  const auto resolved = resolved_style(focused_);
  const float padding =
      std::clamp(resolved.horizontal_padding, 0.0f, bounds.w * 0.5f);
  const float width = std::min(children.front().preferred.w,
                               std::max(0.0f, bounds.w - padding * 2.0f));
  const float height = std::min(children.front().preferred.h, bounds.h);
  placements.front().bounds = {bounds.x + (bounds.w - width) * 0.5f,
                               bounds.y + (bounds.h - height) * 0.5f, width,
                               height};
}

void ButtonComponent::focus_changed(bool focused, FocusContext &context) {
  const auto before = presentation_signature(focused_);
  focused_ = focused;
  interaction_.focus_changed(focused, context, false);
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, context);
}

void ButtonComponent::deactivate(LifecycleContext &context) {
  const auto before = presentation_signature(focused_);
  focused_ = false;
  interaction_.deactivate(context, false);
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, context);
}

EventResult ButtonComponent::input(const InputEvent &event,
                                   InputContext &context) {
  const auto before = presentation_signature(focused_);
  const auto outcome = interaction_.input(event, context, true, false);
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, context);
  if (!outcome.activate)
    return outcome.result;

  // User code may synchronously disable/hide/remove this component. Copy
  // the callback only after the shared interaction machine has completed
  // all component/context work, then never touch `this` or `context`.
  auto callback = on_activate_;
  if (callback)
    callback();
  return outcome.result;
}

ButtonVisualState ButtonComponent::visual_state(bool focused) const noexcept {
  if (!effective_enabled())
    return ButtonVisualState::Disabled;
  if (interaction_.pressed())
    return ButtonVisualState::Pressed;
  if (interaction_.hovered())
    return ButtonVisualState::Hover;
  if (focused)
    return ButtonVisualState::Focused;
  return ButtonVisualState::Normal;
}

SemanticInfo ButtonComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Button;
  info.name = label_;
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  if (info.enabled)
    info.actions = {SemanticAction::Activate, SemanticAction::Focus};
  return info;
}

void ButtonComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto resolved = resolved_style(context.focused());

  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.color = resolved.text;
  text_style.align = TextAlign::Center;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;

  auto &painter = context.painter();
  painter.fill_rounded_rect(bounds, resolved.corner_radius, resolved.fill);
  painter.stroke_rounded_rect(bounds, resolved.corner_radius,
                              resolved.border_width, resolved.border);
  if (!has_content_)
    painter.text(Point{bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f},
                 label_, text_style);
}

void ButtonComponent::apply_patch(PresentationSignature &target,
                                  const ButtonStylePatch &patch) noexcept {
  if (patch.fill)
    target.fill = *patch.fill;
  if (patch.border)
    target.border = *patch.border;
  if (patch.text)
    target.text = *patch.text;
  if (patch.border_width)
    target.border_width = *patch.border_width;
  if (patch.corner_radius)
    target.corner_radius = *patch.corner_radius;
  if (patch.minimum_width)
    target.minimum_width = *patch.minimum_width;
  if (patch.control_height)
    target.control_height = *patch.control_height;
  if (patch.horizontal_padding)
    target.horizontal_padding = *patch.horizontal_padding;
  if (patch.text_size)
    target.text_size = *patch.text_size;
  if (patch.text_weight)
    target.text_weight = *patch.text_weight;
  if (patch.text_slant)
    target.text_slant = *patch.text_slant;
  if (patch.font_family)
    target.font_family = *patch.font_family;
  if (patch.fallback_families)
    target.fallback_families = &*patch.fallback_families;
}

ButtonComponent::PresentationSignature
ButtonComponent::presentation_signature(bool focused) const noexcept {
  return presentation_signature(effective_availability(), focused);
}

ButtonComponent::PresentationSignature
ButtonComponent::presentation_signature(ComponentAvailability availability,
                                        bool focused) const noexcept {
  const auto &theme = current_theme();
  PresentationSignature result{
      .fill = theme.palette.surface,
      .border = theme.palette.border,
      .text = theme.palette.text,
      .border_width = theme.controls.border_width,
      .corner_radius = theme.radii.medium,
      .minimum_width = theme.controls.minimum_width,
      .control_height = theme.controls.control_height,
      .horizontal_padding = theme.spacing.large,
      .text_size = theme.typography.control_size,
      .text_weight = theme.typography.control_weight,
      .text_slant = theme.typography.slant,
      .font_family = theme.typography.family,
      .fallback_families = &theme.typography.fallback_families,
  };
  if (variant_ == ButtonVariant::Primary) {
    result.fill = theme.palette.accent;
    result.border = theme.palette.accent;
    result.text = theme.palette.background;
  } else if (variant_ == ButtonVariant::Toolbar) {
    result.fill.a = 0.0f;
    result.border.a = 0.0f;
  }
  if (group_style_)
    apply_patch(result, group_style_->buttons.base);
  apply_patch(result, style_.base);

  const auto state = current_visual_state(availability, focused);
  switch (resolve_interaction_state(state)) {
  case InteractionVisualState::Normal:
    break;
  case InteractionVisualState::Hovered:
    result.fill = variant_ == ButtonVariant::Primary
                      ? theme.palette.accent
                      : theme.palette.control_hover;
    if (group_style_)
      apply_patch(result, group_style_->buttons.hovered);
    apply_patch(result, style_.hovered);
    break;
  case InteractionVisualState::Pressed:
    result.fill = theme.palette.accent;
    result.border = theme.palette.accent;
    result.text = theme.palette.background;
    if (group_style_)
      apply_patch(result, group_style_->buttons.pressed);
    apply_patch(result, style_.pressed);
    break;
  case InteractionVisualState::Disabled:
    result.fill = theme.palette.control_background;
    result.text = theme.palette.disabled;
    if (group_style_)
      apply_patch(result, group_style_->buttons.disabled);
    apply_patch(result, style_.disabled);
    break;
  }
  if (state.read_only) {
    if (group_style_)
      apply_patch(result, group_style_->buttons.read_only);
    apply_patch(result, style_.read_only);
  }
  if (state.focused) {
    result.border = theme.palette.focus;
    result.border_width = theme.controls.focus_ring_width;
    if (group_style_)
      apply_patch(result, group_style_->buttons.focused);
    apply_patch(result, style_.focused);
  }
  return result;
}

bool ButtonComponent::same_color(Color lhs, Color rhs) noexcept {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool ButtonComponent::same_fallbacks(const PresentationSignature &lhs,
                                     const PresentationSignature &rhs) {
  return lhs.fallback_families == rhs.fallback_families ||
         (lhs.fallback_families != nullptr &&
          rhs.fallback_families != nullptr &&
          *lhs.fallback_families == *rhs.fallback_families);
}

bool ButtonComponent::same_layout(const PresentationSignature &lhs,
                                  const PresentationSignature &rhs) {
  return lhs.minimum_width == rhs.minimum_width &&
         lhs.control_height == rhs.control_height &&
         lhs.horizontal_padding == rhs.horizontal_padding &&
         lhs.text_size == rhs.text_size && lhs.text_weight == rhs.text_weight &&
         lhs.text_slant == rhs.text_slant &&
         lhs.font_family == rhs.font_family && same_fallbacks(lhs, rhs);
}

bool ButtonComponent::same_presentation(const PresentationSignature &lhs,
                                        const PresentationSignature &rhs) {
  return same_color(lhs.fill, rhs.fill) && same_color(lhs.border, rhs.border) &&
         same_color(lhs.text, rhs.text) &&
         lhs.border_width == rhs.border_width &&
         lhs.corner_radius == rhs.corner_radius && same_layout(lhs, rhs);
}

template <class Context>
void ButtonComponent::invalidate_style_transition(
    const PresentationSignature &before, const PresentationSignature &after,
    Context &context) {
  if (same_presentation(before, after))
    return;
  if (!same_layout(before, after)) {
    context.invalidate_layout();
    return;
  }
  context.invalidate();
}

bool ButtonComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  return !same_layout(presentation_signature(before, focused_),
                      presentation_signature(after, focused_));
}

bool ButtonComponent::availability_change_affects_paint(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const noexcept {
  return !same_presentation(presentation_signature(before, focused_),
                            presentation_signature(after, focused_));
}

VisualState ButtonComponent::current_visual_state(bool focused) const noexcept {
  return current_visual_state(effective_availability(), focused);
}

VisualState
ButtonComponent::current_visual_state(ComponentAvailability availability,
                                      bool focused) const noexcept {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
      .hovered = interaction_.hovered(),
      .pressed = interaction_.pressed(),
      .focused = focused,
  };
}

ResolvedButtonStyle ButtonComponent::resolved_style(bool focused) const {
  const auto signature = presentation_signature(focused);
  return ResolvedButtonStyle{.fill = signature.fill,
                             .border = signature.border,
                             .text = signature.text,
                             .border_width = signature.border_width,
                             .corner_radius = signature.corner_radius,
                             .minimum_width = signature.minimum_width,
                             .control_height = signature.control_height,
                             .horizontal_padding = signature.horizontal_padding,
                             .text_size = signature.text_size,
                             .text_weight = signature.text_weight,
                             .text_slant = signature.text_slant,
                             .font_family = std::string{signature.font_family},
                             .fallback_families =
                                 signature.fallback_families
                                     ? *signature.fallback_families
                                     : std::vector<std::string>{}};
}
} // namespace detail

Button::Button(std::string label, ActivateCallback on_activate)
    : label_(std::move(label)), on_activate_(std::move(on_activate)) {}

Button &&Button::style(ButtonStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Button &&Button::variant(ButtonVariant value) && {
  variant_ = value;
  return std::move(*this);
}
Button &&Button::content(Spec value) && {
  children_.clear();
  children_.push_back(std::move(value));
  return std::move(*this);
}

Spec Button::spec() && {
  auto label = std::move(label_);
  auto callback = std::move(on_activate_);
  auto style = std::move(style_);
  const auto variant = variant_;
  const bool has_content = !children_.empty();
  return Spec{[label = std::move(label), callback = std::move(callback),
               style = std::move(style), variant, has_content]() mutable {
                return std::make_unique<detail::ButtonComponent>(
                    std::move(label), std::move(callback), std::move(style),
                    variant, has_content);
              },
              std::move(children_)};
}

} // namespace ui
