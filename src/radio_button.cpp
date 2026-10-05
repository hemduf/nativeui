#include <nativeui/radio_button.hpp>

#include <exception>
#include <optional>

namespace ui::detail {
RadioButtonComponent::RadioButtonComponent(
    std::shared_ptr<RadioGroupToken> group, std::string label,
    IsSelected is_selected, Select select, Observe observe, RadioStyle style)
    : RadioButtonComponent(std::move(group), std::move(label),
                           std::move(is_selected), std::move(select),
                           std::move(observe), std::move(style), {}) {}

RadioButtonComponent::RadioButtonComponent(
    std::shared_ptr<RadioGroupToken> group, std::string label,
    IsSelected is_selected, Select select, Observe observe, RadioStyle style,
    IsValid is_valid)
    : group_(std::move(group)), label_(std::move(label)),
      is_selected_(std::move(is_selected)), is_valid_(std::move(is_valid)),
      select_(std::move(select)), observe_(std::move(observe)),
      style_(std::move(style)) {}

void RadioButtonComponent::set_guarded_select(std::function<void(const std::function<bool()>&)> select) {
  guarded_select_ = std::move(select);
}

bool RadioButtonComponent::focusable() const noexcept { return true; }
bool RadioButtonComponent::cancel_capture_on_read_only() const noexcept {
  return true;
}

bool RadioButtonComponent::source_valid() const {
  return !is_valid_ || is_valid_();
}

EventResult RadioButtonComponent::semantic_action(SemanticAction action, InputContext &context) {
  if ((action != SemanticAction::Select && action != SemanticAction::Activate) ||
      !effective_availability().interactive() || effective_read_only() || !source_valid())
    return EventResult::Ignored;
  auto select = select_;
  auto guarded = guarded_select_;
  auto permission = InputMutationAccess::guard(context);
  const bool valid = source_valid();
  if (!valid || !InputMutationAccess::allowed(context)) return EventResult::Ignored;
  if (guarded) guarded(permission);
  else if (select) select();
  return EventResult::Handled;
}

SemanticInfo RadioButtonComponent::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::RadioButton;
  info.name = label_;
  info.selected = is_selected_ && is_selected_();
  info.checked = info.selected ? SemanticCheckedState::Checked
                               : SemanticCheckedState::Unchecked;
  info.focusable = true;
  info.focused = focused_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only() || !source_valid();
  if (info.enabled) {
    info.actions.push_back(SemanticAction::Focus);
    if (!info.read_only)
      info.actions.push_back(SemanticAction::Select);
  }
  return info;
}

Size RadioButtonComponent::measure(const std::vector<ChildMetrics> &) const {
  const auto resolved = resolved_style(focused_);
  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;
  const auto text = TextService::measure(label_, text_style);
  return Size{
      std::max(resolved.minimum_width, text.width + resolved.leading_padding +
                                           resolved.outer_radius * 2.0f +
                                           resolved.label_gap + 6.0f),
      resolved.control_height};
}

void RadioButtonComponent::mount(MountContext &context) {
  mutation_guard_ = InputMutationAccess::guard(context);
  if (!observe_)
    return;

  state_invalidation_ = std::make_shared<RadioStateInvalidation>();
  state_invalidation_->theme = &current_theme();
  state_invalidation_->style = style_;
  state_invalidation_->invalidate = context.invalidator();
  state_invalidation_->invalidate_layout = context.layout_invalidator();
  state_invalidation_->active = true;
  sync_state_invalidation(effective_availability(), focused_);

  auto invalidation = state_invalidation_;
  auto is_selected = is_selected_;
  subscription_ = observe_([invalidation = std::move(invalidation),
                            is_selected = std::move(is_selected)]() {
    invalidation->publish(is_selected && is_selected());
  });
}

void RadioButtonComponent::unmount(LifecycleContext &) {
  mutation_guard_ = {};
  if (state_invalidation_)
    state_invalidation_->active = false;
  subscription_.reset();
  state_invalidation_.reset();
  release_pointer_ = {};
}

void RadioButtonComponent::focus_changed(bool focused, FocusContext &context) {
  std::optional<ResolvedRadioStyle> before;
  std::exception_ptr failure;
  try {
    before = resolved_style(focused_);
  } catch (...) {
    failure = std::current_exception();
  }
  focused_ = focused;
  interaction_.focus_changed(focused, context, false);
  auto release =
      focused ? std::function<void()>{} : std::move(release_pointer_);
  if (release)
    release();
  if (failure)
    std::rethrow_exception(failure);
  const auto after = resolved_style(focused_);
  invalidate_transition(*before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
}

void RadioButtonComponent::deactivate(LifecycleContext &context) {
  std::optional<ResolvedRadioStyle> before;
  std::exception_ptr failure;
  try {
    before = resolved_style(focused_);
  } catch (...) {
    failure = std::current_exception();
  }
  focused_ = false;
  interaction_.deactivate(context, false);
  auto release = std::move(release_pointer_);
  if (release)
    release();
  if (failure)
    std::rethrow_exception(failure);
  const auto after = resolved_style(focused_);
  invalidate_transition(*before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
}

void RadioButtonComponent::set_selection_revision(std::function<std::uint64_t()> reader) {
  selection_revision_ = std::move(reader);
}
std::uint64_t RadioButtonComponent::focus_group_selection_revision() const noexcept {
  return selection_revision_ ? selection_revision_() : 0;
}

const void *RadioButtonComponent::focus_group_identity() const noexcept {
  return group_.get();
}

bool RadioButtonComponent::focus_group_selected() const {
  return is_selected_ && is_selected_();
}
bool RadioButtonComponent::focus_group_accepts_boundary_key(
    Key) const noexcept {
  return true;
}

void RadioButtonComponent::focus_group_select() {
  focus_group_select_guarded(mutation_guard_);
}

void RadioButtonComponent::focus_group_select_guarded(
    const std::function<bool()>& allowed) {
  if ((allowed && !allowed()) || effective_read_only() || !source_valid())
    return;
  sync_state_invalidation(effective_availability(), focused_);
  auto select = select_;
  auto guarded = guarded_select_;
  auto permission = allowed;
  if (permission && !permission()) return;
  if (guarded) guarded(permission);
  else if (select) select();
}

EventResult RadioButtonComponent::input(const InputEvent &event,
                                        InputContext &context) {
  const auto before = resolved_style(focused_);
  if ((effective_read_only() || !source_valid()) &&
      read_only_value_input(event)) {
    auto release = std::move(release_pointer_);
    if (release)
      release();
    interaction_.cancel_pending_mutation(context, false);
    const auto after = resolved_style(focused_);
    invalidate_transition(before, after, context);
    sync_state_invalidation(effective_availability(), focused_);
    return EventResult::Handled;
  }

  auto select = select_;
  auto guarded = guarded_select_;
  auto permission = InputMutationAccess::guard(context);
  auto release = event.type == InputType::PointerDown
                     ? context.pointer_releaser()
                     : std::function<void()>{};
  auto cancel_release = event.type == InputType::PointerCancel
                            ? std::move(release_pointer_)
                            : std::function<void()>{};
  const auto outcome = interaction_.input(event, context, false, false);
  if (event.type == InputType::PointerDown)
    release_pointer_ = std::move(release);
  else if (event.type == InputType::PointerUp ||
           event.type == InputType::PointerCancel)
    release_pointer_ = {};
  if (cancel_release)
    cancel_release();
  const auto after = resolved_style(focused_);
  invalidate_transition(before, after, context);
  sync_state_invalidation(effective_availability(), focused_);
  if (!outcome.activate)
    return outcome.result;

  // Selection callbacks may rebuild the retained tree. Do not access the
  // component or InputContext after invoking user/application state.
  if (permission && !permission()) return outcome.result;
  if (guarded) guarded(permission);
  else if (select) select();
  return outcome.result;
}

void RadioButtonComponent::paint(PaintContext &context) const {
  const auto bounds = context.bounds();
  const auto resolved = resolved_style(context.focused());
  const Point center{bounds.x + resolved.leading_padding +
                         resolved.outer_radius,
                     bounds.y + bounds.h * 0.5f};

  TextStyle text_style{};
  text_style.size = resolved.text_size;
  text_style.color = resolved.text;
  text_style.align = TextAlign::Left;
  text_style.weight = resolved.text_weight;
  text_style.slant = resolved.text_slant;
  text_style.family = resolved.font_family;
  text_style.fallback_families = resolved.fallback_families;

  auto &painter = context.painter();
  painter.circle(center, resolved.outer_radius, resolved.outer_fill);
  painter.circle(center, resolved.inner_radius, resolved.inner_fill);
  if (is_selected_ && is_selected_()) {
    painter.circle(center, resolved.mark_radius, resolved.mark_fill);
  }
  painter.text(Point{center.x + resolved.outer_radius + resolved.label_gap,
                     bounds.y + bounds.h * 0.5f},
               label_, text_style);
}

VisualState RadioButtonComponent::current_visual_state(bool focused) const {
  return current_visual_state(effective_availability(), focused);
}

VisualState
RadioButtonComponent::current_visual_state(ComponentAvailability availability,
                                           bool focused) const {
  return VisualState{
      .enabled = availability.enabled,
      .read_only = availability.read_only,
      .hovered = interaction_.hovered(),
      .pressed = interaction_.pressed(),
      .focused = focused,
      .selected = is_selected_ && is_selected_(),
  };
}

void RadioButtonComponent::sync_state_invalidation(
    ComponentAvailability availability, bool focused) const noexcept {
  try {
    if (state_invalidation_)
      state_invalidation_->sync(current_visual_state(availability, focused));
  } catch (...) {
    // Arbitrary user equality can fail. Keep the accepted presentation
    // snapshot; a normal measure/paint access still reports the original
    // equality failure.
  }
}

ResolvedRadioStyle RadioButtonComponent::resolved_style(bool focused) const {
  return resolved_style(effective_availability(), focused);
}

ResolvedRadioStyle
RadioButtonComponent::resolved_style(ComponentAvailability availability,
                                     bool focused) const {
  return resolve_radio_style(default_radio_style(current_theme()), style_,
                             current_visual_state(availability, focused));
}

bool RadioButtonComponent::availability_change_affects_layout(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const {
  const bool changed = !radio_layout_equal(resolved_style(before, focused_),
                                           resolved_style(after, focused_));
  sync_state_invalidation(after, focused_);
  return changed;
}

bool RadioButtonComponent::availability_change_affects_paint(
    const ComponentAvailability &before,
    const ComponentAvailability &after) const noexcept {
  try {
    const bool changed = !radio_presentation_equal(
        resolved_style(before, focused_), resolved_style(after, focused_));
    sync_state_invalidation(after, focused_);
    return changed;
  } catch (...) {
    return true;
  }
}

template <class Context>
void RadioButtonComponent::invalidate_transition(
    const ResolvedRadioStyle &before, const ResolvedRadioStyle &after,
    Context &context) {
  invalidate_resolved_style_transition(
      before, after, context, radio_layout_equal, radio_presentation_equal);
}
} // namespace ui::detail
