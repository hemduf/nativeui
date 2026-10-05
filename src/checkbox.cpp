#include <nativeui/checkbox.hpp>

namespace ui {

namespace detail {
bool read_only_value_input(const InputEvent &event) noexcept {
  if (event.type == InputType::PointerDown ||
      event.type == InputType::PointerUp)
    return true;
  return (event.type == InputType::KeyDown || event.type == InputType::KeyUp) &&
         event.key == Key::Space;
}

bool style_color_equal(Color lhs, Color rhs) noexcept {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

bool checkbox_layout_equal(const ResolvedCheckboxStyle &lhs,
                           const ResolvedCheckboxStyle &rhs) {
  return lhs.box_size == rhs.box_size &&
         lhs.minimum_width == rhs.minimum_width &&
         lhs.control_height == rhs.control_height &&
         lhs.leading_padding == rhs.leading_padding &&
         lhs.label_gap == rhs.label_gap && lhs.text_size == rhs.text_size &&
         lhs.text_weight == rhs.text_weight &&
         lhs.text_slant == rhs.text_slant &&
         lhs.font_family == rhs.font_family &&
         lhs.fallback_families == rhs.fallback_families;
}

bool checkbox_presentation_equal(const ResolvedCheckboxStyle &lhs,
                                 const ResolvedCheckboxStyle &rhs) {
  return checkbox_layout_equal(lhs, rhs) &&
         style_color_equal(lhs.box_fill, rhs.box_fill) &&
         style_color_equal(lhs.box_border, rhs.box_border) &&
         style_color_equal(lhs.checkmark, rhs.checkmark) &&
         style_color_equal(lhs.text, rhs.text) &&
         lhs.box_corner_radius == rhs.box_corner_radius &&
         lhs.box_border_width == rhs.box_border_width &&
         lhs.checkmark_width == rhs.checkmark_width;
}

bool checkbox_checkmark_visible(const ResolvedCheckboxStyle &style) noexcept {
  return style.box_size > 0.0f && style.checkmark_width > 0.0f &&
         style.checkmark.a > 0.0f;
}

bool radio_layout_equal(const ResolvedRadioStyle &lhs,
                        const ResolvedRadioStyle &rhs) {
  return lhs.outer_radius == rhs.outer_radius &&
         lhs.minimum_width == rhs.minimum_width &&
         lhs.control_height == rhs.control_height &&
         lhs.leading_padding == rhs.leading_padding &&
         lhs.label_gap == rhs.label_gap && lhs.text_size == rhs.text_size &&
         lhs.text_weight == rhs.text_weight &&
         lhs.text_slant == rhs.text_slant &&
         lhs.font_family == rhs.font_family &&
         lhs.fallback_families == rhs.fallback_families;
}

bool radio_presentation_equal(const ResolvedRadioStyle &lhs,
                              const ResolvedRadioStyle &rhs) {
  return radio_layout_equal(lhs, rhs) &&
         style_color_equal(lhs.outer_fill, rhs.outer_fill) &&
         style_color_equal(lhs.inner_fill, rhs.inner_fill) &&
         style_color_equal(lhs.mark_fill, rhs.mark_fill) &&
         style_color_equal(lhs.text, rhs.text) &&
         lhs.inner_radius == rhs.inner_radius &&
         lhs.mark_radius == rhs.mark_radius;
}

bool radio_mark_visible(const ResolvedRadioStyle &style) noexcept {
  return style.mark_radius > 0.0f && style.mark_fill.a > 0.0f;
}

void CheckboxStateInvalidation::sync(VisualState state) noexcept {
  visual = state;
}

void CheckboxStateInvalidation::publish(bool checked) {
  if (!active || !theme)
    return;

  auto before_state = visual;
  auto after_state = visual;
  after_state.checked = checked;

  const auto inherited = default_checkbox_style(*theme);
  const auto before = resolve_checkbox_style(inherited, style, before_state);
  const auto after = resolve_checkbox_style(inherited, style, after_state);
  const bool before_mark =
      before_state.checked && checkbox_checkmark_visible(before);
  const bool after_mark =
      after_state.checked && checkbox_checkmark_visible(after);

  visual = after_state;
  if (checkbox_presentation_equal(before, after) && before_mark == after_mark)
    return;
  if (!checkbox_layout_equal(before, after))
    invalidate_layout();
  invalidate();
}

void RadioStateInvalidation::sync(VisualState state) noexcept {
  visual = state;
}

void RadioStateInvalidation::publish(bool selected) {
  if (!active || !theme)
    return;

  auto before_state = visual;
  auto after_state = visual;
  after_state.selected = selected;

  const auto inherited = default_radio_style(*theme);
  const auto before = resolve_radio_style(inherited, style, before_state);
  const auto after = resolve_radio_style(inherited, style, after_state);
  const bool before_mark = before_state.selected && radio_mark_visible(before);
  const bool after_mark = after_state.selected && radio_mark_visible(after);

  visual = after_state;
  if (radio_presentation_equal(before, after) && before_mark == after_mark)
    return;
  if (!radio_layout_equal(before, after))
    invalidate_layout();
  invalidate();
}

CheckboxComponent::CheckboxComponent(Binding<bool> state, std::string label,
                                     CheckboxStyle style)
    : state_(std::move(state)), label_(std::move(label)),
      style_(std::move(style)) {}

bool CheckboxComponent::focusable() const noexcept { return true; }

Size CheckboxComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto resolved = resolved_style(focused_);
  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;
  const auto text = TextService::measure(label_, text_style);
  return Size{std::max(resolved.minimum_width,
                       text.width + resolved.leading_padding +
                           resolved.box_size + resolved.label_gap + 5.0f),
              resolved.control_height};
}

void CheckboxComponent::mount(MountContext &context) {
  // State/Binding notifications snapshot listeners before callbacks run.
  // Keep the classifier detached from the component so an earlier listener
  // may synchronously remove this node without leaving a raw `this` capture.
  state_invalidation_ = std::make_shared<CheckboxStateInvalidation>();
  state_invalidation_->theme = &current_theme();
  state_invalidation_->style = style_;
  state_invalidation_->invalidate = context.invalidator();
  state_invalidation_->invalidate_layout = context.layout_invalidator();
  state_invalidation_->active = true;
  sync_state_invalidation(effective_availability(), focused_);

  auto invalidation = state_invalidation_;
  subscription_ = state_.observe(
      [invalidation = std::move(invalidation)](const bool &checked) {
        invalidation->publish(checked);
      });
}

void CheckboxComponent::unmount(LifecycleContext &) {
  if (state_invalidation_)
    state_invalidation_->active = false;
  subscription_.reset();
  state_invalidation_.reset();
}

void CheckboxComponent::focus_changed(bool focused, FocusContext &context) {
  const auto before = resolved_style(focused_);
  focused_ = focused;
  interaction_.focus_changed(focused, context, false);
  const auto after = resolved_style(focused_);
  invalidate_transition(before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
}

void CheckboxComponent::deactivate(LifecycleContext &context) {
  const auto before = resolved_style(focused_);
  focused_ = false;
  interaction_.deactivate(context, false);
  const auto after = resolved_style(focused_);
  invalidate_transition(before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
}

EventResult CheckboxComponent::input(const InputEvent &event,
                                     InputContext &context) {
  const auto before = resolved_style(focused_);
  if ((effective_read_only() || !state_.valid()) &&
      read_only_value_input(event)) {
    interaction_.cancel_pending_mutation(context, false);
    const auto after = resolved_style(focused_);
    invalidate_transition(before, after, context);
    sync_state_invalidation(effective_availability(), focused_);
    return EventResult::Handled;
  }

  auto state = state_;
  const auto outcome = interaction_.input(event, context, false, false);
  const auto after = resolved_style(focused_);
  invalidate_transition(before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
  if (!outcome.activate)
    return outcome.result;

  // Binding observers may synchronously rebuild/destroy this component.
  // Complete all component/context work before publishing the value and
  // never touch `this` afterwards.
  if (state.valid() && InputMutationAccess::allowed(context))
    state.set(!state.get());
  return outcome.result;
}

EventResult CheckboxComponent::semantic_action(SemanticAction action, InputContext &context) {
  if ((action != SemanticAction::Toggle && action != SemanticAction::Activate) ||
      !effective_availability().interactive() || effective_read_only() || !state_.valid())
    return EventResult::Ignored;
  auto state = state_;
  const bool next = !state.get();
  if (!InputMutationAccess::allowed(context)) return EventResult::Ignored;
  state.set(next);
  return EventResult::Handled;
}

SemanticInfo CheckboxComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Checkbox;
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

void CheckboxComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto resolved = resolved_style(context.focused());
  const float box_size = resolved.box_size;
  const Rect box{bounds.x + resolved.leading_padding,
                 bounds.y + (bounds.h - box_size) * 0.5f, box_size, box_size};

  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.color = resolved.text;
  text_style.align = TextAlign::Left;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;

  auto &painter = context.painter();
  painter.fill_rounded_rect(box, resolved.box_corner_radius, resolved.box_fill);
  painter.stroke_rounded_rect(box, resolved.box_corner_radius,
                              resolved.box_border_width, resolved.box_border);
  if (state_.get()) {
    constexpr float kReferenceBoxSize = 18.0f;
    const float scale = box_size / kReferenceBoxSize;
    painter.line(Point{box.x + 4.0f * scale, box.y + 9.0f * scale},
                 Point{box.x + 8.0f * scale, box.y + 13.0f * scale},
                 resolved.checkmark_width, resolved.checkmark);
    painter.line(Point{box.x + 8.0f * scale, box.y + 13.0f * scale},
                 Point{box.x + 15.0f * scale, box.y + 5.0f * scale},
                 resolved.checkmark_width, resolved.checkmark);
  }
  painter.text(
      Point{box.x + box.w + resolved.label_gap, bounds.y + bounds.h * 0.5f},
      label_, text_style);
}

VisualState
CheckboxComponent::current_visual_state(bool focused) const noexcept {
  return current_visual_state(effective_availability(), focused);
}

VisualState
CheckboxComponent::current_visual_state(ComponentAvailability availability,
                                        bool focused) const noexcept {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
      .hovered = interaction_.hovered(),
      .pressed = interaction_.pressed(),
      .focused = focused,
      .checked = state_.get(),
  };
}

void CheckboxComponent::sync_state_invalidation(
    ComponentAvailability availability, bool focused) const noexcept {
  if (state_invalidation_) {
    state_invalidation_->sync(current_visual_state(availability, focused));
  }
}

ResolvedCheckboxStyle CheckboxComponent::resolved_style(bool focused) const {
  return resolved_style(effective_availability(), focused);
}

ResolvedCheckboxStyle
CheckboxComponent::resolved_style(ComponentAvailability availability,
                                  bool focused) const {
  return resolve_checkbox_style(default_checkbox_style(current_theme()), style_,
                                current_visual_state(availability, focused));
}

bool CheckboxComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  const bool changed = !checkbox_layout_equal(resolved_style(before, focused_),
                                              resolved_style(after, focused_));
  sync_state_invalidation(after, focused_);
  return changed;
}

bool CheckboxComponent::availability_change_affects_paint(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const noexcept {
  const bool changed = !checkbox_presentation_equal(
      resolved_style(before, focused_), resolved_style(after, focused_));
  sync_state_invalidation(after, focused_);
  return changed;
}

template <class Context>
void CheckboxComponent::invalidate_transition(
    const ResolvedCheckboxStyle &before, const ResolvedCheckboxStyle &after,
    Context &context) {
  invalidate_resolved_style_transition(before, after, context,
                                       checkbox_layout_equal,
                                       checkbox_presentation_equal);
}
} // namespace detail

Checkbox::Checkbox(Binding<bool> state, std::string label)
    : state_(std::move(state)), label_(std::move(label)) {}

Checkbox::Checkbox(State<bool> &state, std::string label)
    : Checkbox(state.binding(), std::move(label)) {}

Checkbox &&Checkbox::style(CheckboxStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec Checkbox::spec() && {
  auto state = std::move(state_);
  auto label = std::move(label_);
  auto style = std::move(style_);
  return Spec{[state = std::move(state), label = std::move(label),
               style = std::move(style)]() mutable {
                return std::make_unique<detail::CheckboxComponent>(
                    std::move(state), std::move(label), std::move(style));
              },
              {}};
}

} // namespace ui
