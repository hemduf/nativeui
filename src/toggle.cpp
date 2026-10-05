#include <nativeui/toggle.hpp>

namespace ui {

ToggleComponent::ToggleComponent(std::string label, Binding<bool> state,
                                 ToggleStyle style)
    : label_(std::move(label)), state_(std::move(state)),
      style_(std::move(style)) {}

bool ToggleComponent::focusable() const noexcept { return true; }

Size ToggleComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto resolved = resolved_style(focused_);
  return Size{resolved.control_width, resolved.control_height};
}

void ToggleComponent::mount(MountContext &ctx) {
  const bool checked_changes_layout = patch_affects_layout(style_.checked);
  auto invalidate = ctx.invalidator();
  auto invalidate_layout = ctx.layout_invalidator();
  subscription_ = state_.observe(
      [checked_changes_layout, invalidate = std::move(invalidate),
       invalidate_layout = std::move(invalidate_layout)](const bool &) {
        if (checked_changes_layout)
          invalidate_layout();
        invalidate();
      });
}

void ToggleComponent::unmount(LifecycleContext &) { subscription_.reset(); }

void ToggleComponent::focus_changed(bool focused, FocusContext &context) {
  const auto before = presentation_signature(focused_);
  focused_ = focused;
  interaction_.focus_changed(focused, context, false);
  if (!focused) {
    space_pressed_ = false;
    enter_pressed_ = false;
  }
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, context);
}

void ToggleComponent::deactivate(LifecycleContext &context) {
  const auto before = presentation_signature(focused_);
  focused_ = false;
  interaction_.deactivate(context, false);
  space_pressed_ = false;
  enter_pressed_ = false;
  const auto after = presentation_signature(focused_);
  invalidate_style_transition(before, after, context);
}

EventResult ToggleComponent::input(const InputEvent &event, InputContext &ctx) {
  const auto before = presentation_signature(focused_);
  const bool pointer_event = event.type == InputType::PointerDown ||
                             event.type == InputType::PointerMove ||
                             event.type == InputType::PointerUp ||
                             event.type == InputType::PointerCancel;

  if (pointer_event) {
    const bool mutating_pointer =
        event.type == InputType::PointerDown || interaction_.pressed();
    if ((effective_read_only() || !state_.valid()) && mutating_pointer) {
      interaction_.cancel_pending_mutation(ctx, false);
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
      return EventResult::Handled;
    }

    const auto outcome = interaction_.input(event, ctx, false, false);
    const auto after_interaction = presentation_signature(focused_);
    invalidate_style_transition(before, after_interaction, ctx);
    if (event.type == InputType::PointerDown && !effective_read_only() &&
        state_.valid()) {
      const bool next = !state_.get();
      if (state_.valid() && detail::InputMutationAccess::allowed(ctx))
        state_.set(next);
      return outcome.result;
    }
    return outcome.result;
  }

  if (event.type == InputType::KeyDown && event.key == Key::Space) {
    if (effective_read_only() || !state_.valid())
      return EventResult::Handled;
    if (!space_pressed_) {
      space_pressed_ = true;
      const auto after_interaction = presentation_signature(focused_);
      invalidate_style_transition(before, after_interaction, ctx);
      const bool next = !state_.get();
      if (state_.valid() && detail::InputMutationAccess::allowed(ctx))
        state_.set(next);
      return EventResult::Handled;
    }
    return EventResult::Handled;
  }
  if (event.type == InputType::KeyDown && event.key == Key::Enter) {
    if (effective_read_only() || !state_.valid())
      return EventResult::Handled;
    if (!enter_pressed_) {
      enter_pressed_ = true;
      const auto after_interaction = presentation_signature(focused_);
      invalidate_style_transition(before, after_interaction, ctx);
      const bool next = !state_.get();
      if (state_.valid() && detail::InputMutationAccess::allowed(ctx))
        state_.set(next);
      return EventResult::Handled;
    }
    return EventResult::Handled;
  }
  if (event.type == InputType::KeyUp && event.key == Key::Space) {
    if (space_pressed_) {
      space_pressed_ = false;
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
    }
    return EventResult::Handled;
  }
  if (event.type == InputType::KeyUp && event.key == Key::Enter) {
    if (enter_pressed_) {
      enter_pressed_ = false;
      const auto after = presentation_signature(focused_);
      invalidate_style_transition(before, after, ctx);
    }
    return EventResult::Handled;
  }

  return EventResult::Ignored;
}

EventResult ToggleComponent::semantic_action(SemanticAction action, InputContext &context) {
  if ((action != SemanticAction::Toggle && action != SemanticAction::Activate) ||
      !effective_availability().interactive() || effective_read_only() || !state_.valid())
    return EventResult::Ignored;
  auto state = state_;
  const bool next = !state.get();
  if (!detail::InputMutationAccess::allowed(context)) return EventResult::Ignored;
  state.set(next);
  return EventResult::Handled;
}

SemanticInfo ToggleComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Toggle;
  info.name = label_;
  info.checked = state_.get() ? SemanticCheckedState::Checked
                              : SemanticCheckedState::Unchecked;
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only() || !state_.valid();
  if (info.enabled) {
    info.actions.push_back(SemanticAction::Focus);
    if (!info.read_only)
      info.actions.push_back(SemanticAction::Toggle);
  }
  return info;
}

void ToggleComponent::paint(PaintContext &p) const {
  const auto b = p.bounds();
  const auto resolved = resolved_style(p.focused());
  auto &dl = p.painter();

  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.color = resolved.text;
  text_style.align = TextAlign::Left;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;

  dl.fill_rounded_rect(b, resolved.corner_radius, resolved.fill);
  dl.stroke_rounded_rect(b, resolved.corner_radius, resolved.border_width,
                         resolved.border);
  dl.text(Point{b.x + resolved.leading_padding, b.y + b.h * 0.5f}, label_,
          text_style);

  const Rect track{b.x + b.w - resolved.trailing_padding - resolved.track_width,
                   b.y +
                       (resolved.control_height - resolved.track_height) * 0.5f,
                   resolved.track_width, resolved.track_height};
  const float track_radius = resolved.track_height * 0.5f;
  dl.fill_rounded_rect(track, track_radius, resolved.track);

  const float thumb_radius = resolved.thumb_diameter * 0.5f;
  const float thumb_x =
      state_.get() ? track.x + track.w - track_radius : track.x + track_radius;
  dl.circle(Point{thumb_x, track.y + track_radius}, thumb_radius,
            resolved.thumb);
}

bool ToggleComponent::patch_affects_layout(
    const ToggleStylePatch &patch) noexcept {
  return patch.control_width || patch.control_height || patch.leading_padding ||
         patch.trailing_padding || patch.track_width || patch.track_height ||
         patch.thumb_diameter || patch.text_size || patch.text_weight ||
         patch.text_slant || patch.font_family || patch.fallback_families;
}

void ToggleComponent::apply_patch(PresentationSignature &target,
                                  const ToggleStylePatch &patch) noexcept {
  if (patch.fill)
    target.fill = *patch.fill;
  if (patch.border)
    target.border = *patch.border;
  if (patch.text)
    target.text = *patch.text;
  if (patch.track)
    target.track = *patch.track;
  if (patch.thumb)
    target.thumb = *patch.thumb;
  if (patch.border_width)
    target.border_width = *patch.border_width;
  if (patch.corner_radius)
    target.corner_radius = *patch.corner_radius;
  if (patch.control_width)
    target.control_width = *patch.control_width;
  if (patch.control_height)
    target.control_height = *patch.control_height;
  if (patch.leading_padding)
    target.leading_padding = *patch.leading_padding;
  if (patch.trailing_padding)
    target.trailing_padding = *patch.trailing_padding;
  if (patch.track_width)
    target.track_width = *patch.track_width;
  if (patch.track_height)
    target.track_height = *patch.track_height;
  if (patch.thumb_diameter)
    target.thumb_diameter = *patch.thumb_diameter;
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

ToggleComponent::PresentationSignature
ToggleComponent::presentation_signature(bool focused) const noexcept {
  return presentation_signature(effective_availability(), focused);
}

ToggleComponent::PresentationSignature
ToggleComponent::presentation_signature(ComponentAvailability availability,
                                        bool focused) const noexcept {
  const auto &theme = current_theme();
  PresentationSignature result{
      .fill = theme.palette.surface,
      .border = theme.palette.border,
      .text = theme.palette.text,
      .track = colors::toggleOff,
      .thumb = theme.palette.text,
      .border_width = theme.controls.border_width,
      .corner_radius = theme.radii.large,
      .control_width = 210.0f,
      .control_height = 54.0f,
      .leading_padding = 18.0f,
      .trailing_padding = 18.0f,
      .track_width = 44.0f,
      .track_height = 26.0f,
      .thumb_diameter = 18.0f,
      .text_size = theme.typography.control_size,
      .text_weight = theme.typography.control_weight,
      .text_slant = theme.typography.slant,
      .font_family = theme.typography.family,
      .fallback_families = &theme.typography.fallback_families,
  };
  apply_patch(result, style_.base);

  const auto state = current_visual_state(availability, focused);
  if (state.checked) {
    result.track = theme.palette.accent;
    apply_patch(result, style_.checked);
  }

  switch (resolve_interaction_state(state)) {
  case InteractionVisualState::Normal:
    break;
  case InteractionVisualState::Hovered:
    result.border = theme.palette.control_hover;
    apply_patch(result, style_.hovered);
    break;
  case InteractionVisualState::Pressed:
    result.border = theme.palette.accent;
    result.thumb = theme.palette.active_highlight;
    apply_patch(result, style_.pressed);
    break;
  case InteractionVisualState::Disabled:
    result.fill = theme.palette.control_background;
    result.border = theme.palette.disabled;
    result.text = theme.palette.disabled;
    apply_patch(result, style_.disabled);
    break;
  }
  if (state.read_only) {
    result.border = theme.palette.track;
    result.text = theme.palette.muted_text;
    apply_patch(result, style_.read_only);
  }
  if (state.focused) {
    result.border = theme.palette.focus;
    result.border_width = theme.controls.focus_ring_width;
    apply_patch(result, style_.focused);
  }
  return result;
}

bool ToggleComponent::same_color(Color lhs, Color rhs) noexcept {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool ToggleComponent::same_fallbacks(const PresentationSignature &lhs,
                                     const PresentationSignature &rhs) {
  return lhs.fallback_families == rhs.fallback_families ||
         (lhs.fallback_families != nullptr &&
          rhs.fallback_families != nullptr &&
          *lhs.fallback_families == *rhs.fallback_families);
}

bool ToggleComponent::same_layout(const PresentationSignature &lhs,
                                  const PresentationSignature &rhs) {
  return lhs.control_width == rhs.control_width &&
         lhs.control_height == rhs.control_height &&
         lhs.leading_padding == rhs.leading_padding &&
         lhs.trailing_padding == rhs.trailing_padding &&
         lhs.track_width == rhs.track_width &&
         lhs.track_height == rhs.track_height &&
         lhs.thumb_diameter == rhs.thumb_diameter &&
         lhs.text_size == rhs.text_size && lhs.text_weight == rhs.text_weight &&
         lhs.text_slant == rhs.text_slant &&
         lhs.font_family == rhs.font_family && same_fallbacks(lhs, rhs);
}

bool ToggleComponent::same_presentation(const PresentationSignature &lhs,
                                        const PresentationSignature &rhs) {
  return same_color(lhs.fill, rhs.fill) && same_color(lhs.border, rhs.border) &&
         same_color(lhs.text, rhs.text) && same_color(lhs.track, rhs.track) &&
         same_color(lhs.thumb, rhs.thumb) &&
         lhs.border_width == rhs.border_width &&
         lhs.corner_radius == rhs.corner_radius && same_layout(lhs, rhs);
}

template <class Context>
void ToggleComponent::invalidate_style_transition(
    const PresentationSignature &before, const PresentationSignature &after,
    Context &context) {
  if (same_presentation(before, after))
    return;
  if (!same_layout(before, after)) {
    context.invalidate_layout();
    context.invalidate();
    return;
  }
  context.invalidate();
}

bool ToggleComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  return !same_layout(presentation_signature(before, focused_),
                      presentation_signature(after, focused_));
}

VisualState ToggleComponent::current_visual_state(bool focused) const noexcept {
  return current_visual_state(effective_availability(), focused);
}

VisualState
ToggleComponent::current_visual_state(ComponentAvailability availability,
                                      bool focused) const noexcept {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
      .hovered = interaction_.hovered(),
      .pressed = interaction_.pressed() || space_pressed_ || enter_pressed_,
      .focused = focused,
      .checked = state_.get(),
  };
}

ResolvedToggleStyle ToggleComponent::resolved_style(bool focused) const {
  return resolve_toggle_style(default_toggle_style(current_theme()), style_,
                              current_visual_state(focused));
}

Toggle::Toggle(std::string label, Binding<bool> state)
    : label_(std::move(label)), state_(std::move(state)) {}

Toggle::Toggle(std::string label, State<bool> &state)
    : Toggle(std::move(label), state.binding()) {}

Toggle &&Toggle::style(ToggleStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec Toggle::spec() && {
  auto label = std::move(label_);
  auto state = std::move(state_);
  auto style = std::move(style_);
  return Spec{[label = std::move(label), state = std::move(state),
               style = std::move(style)]() mutable {
                return std::make_unique<ToggleComponent>(
                    std::move(label), std::move(state), std::move(style));
              },
              {}};
}

} // namespace ui
