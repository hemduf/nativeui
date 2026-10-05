#include "detail/widget_control_group.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <nativeui/segmented_control.hpp>
#include <utility>
namespace ui::detail {
namespace {
struct SegmentRecipe {
  std::string label;
  std::vector<std::string> labels;
  std::vector<bool> enabled;
  SegmentedControlStyle style;
  SegmentedSelectionFactory factory;
};
struct SegmentContact {
  bool mounted{}, focused{}, hovered{}, pointer_armed{}, pointer_inside{},
      space_down{}, space_armed{}, enter_down{};
  PointerId pointer{};
  std::uint64_t serial{}, press_revision{};
  unsigned pending{};
  std::function<void()> release, invalidate, layout, availability;
  std::function<bool()> allowed;
};
void cancel_contact(const std::shared_ptr<SegmentContact> &contact,
                    bool reset_keys) {
  ++contact->serial;
  contact->pointer_armed = false;
  contact->pointer_inside = false;
  contact->space_armed = false;
  if (reset_keys) {
    contact->space_down = false;
    contact->enter_down = false;
    contact->hovered = false;
  }
  auto release = std::exchange(contact->release, {});
  if (release)
    release();
}
void cancel_contact_noexcept(const std::shared_ptr<SegmentContact> &contact,
                             bool reset_keys) noexcept {
  try {
    cancel_contact(contact, reset_keys);
  } catch (...) {
  }
}
struct SegmentState final : std::enable_shared_from_this<SegmentState> {
  explicit SegmentState(std::shared_ptr<const SegmentRecipe> value)
      : recipe(std::move(value)), model(recipe->factory()),
        selected(recipe->labels.size()), contacts(recipe->labels.size()) {
    if (!model)
      throw std::invalid_argument(
          "SegmentedControl selection factory returned null");
    source_valid = model->valid();
    if (auto initial = model->snapshot({})) {
      selected = std::move(initial->selected);
      seen = initial->revision;
    }
  }
  std::shared_ptr<const SegmentRecipe> recipe;
  std::unique_ptr<SegmentedSelection> model;
  std::unique_ptr<SegmentedSubscription> subscription;
  std::shared_ptr<ControlGroupMemory> memory{
      std::make_shared<ControlGroupMemory>()};
  std::vector<bool> selected;
  std::vector<std::weak_ptr<SegmentContact>> contacts;
  std::uint64_t seen{std::numeric_limits<std::uint64_t>::max()};
  bool source_valid{}, syncing{}, read_pending{};
  bool selected_at(std::size_t index) const noexcept {
    return index < selected.size() && selected[index];
  }
  bool enabled_at(std::size_t index) const noexcept {
    return index < recipe->enabled.size() && recipe->enabled[index];
  }
  std::optional<std::size_t> selected_enabled() const noexcept {
    for (std::size_t i = 0; i < selected.size(); ++i)
      if (selected[i] && enabled_at(i))
        return i;
    return {};
  }
  bool selection_changes_layout() const noexcept {
    const auto &patch = recipe->style.selected;
    return patch.minimum_width || patch.control_height ||
           patch.horizontal_padding || patch.text_size || patch.text_weight ||
           patch.text_slant || patch.font_family || patch.fallback_families;
  }
  void flush() {
    for (const auto &weak : contacts) {
      const auto contact = weak.lock();
      if (!contact || !contact->mounted)
        continue;
      const auto serial = contact->serial;
      const auto run = [&](unsigned bit,
                           const std::function<void()> &callback) {
        if (!contact->mounted || contact->serial != serial ||
            !(contact->pending & bit))
          return;
        auto owned = callback;
        contact->pending &= ~bit;
        if (owned)
          owned();
      };
      run(1, contact->layout);
      run(2, contact->invalidate);
      run(4, contact->availability);
    }
  }
  void sync() {
    if (syncing) {
      read_pending = true;
      return;
    }
    const bool valid = model->valid();
    if (!valid) {
      if (source_valid) {
        source_valid = false;
        for (const auto &weak : contacts)
          if (auto contact = weak.lock()) {
            cancel_contact_noexcept(contact, false);
            contact->pending |= 6;
          }
      }
      flush();
      return;
    }
    if (seen == model->revision() && !read_pending) {
      flush();
      return;
    }
    syncing = true;
    read_pending = false;
    try {
      const auto generation = memory->generation;
      const bool mounted = memory->mounted;
      const std::weak_ptr<SegmentState> weak = shared_from_this();
      auto candidate = model->snapshot([weak, generation, mounted] {
        const auto state = weak.lock();
        return state && state->memory->generation == generation &&
               state->memory->mounted == mounted;
      });
      if (!candidate) {
        read_pending = true;
        syncing = false;
        return;
      }
      if (candidate->selected.size() != selected.size())
        throw std::logic_error("SegmentedControl selection mask size mismatch");
      const bool source_changed = seen != candidate->revision;
      const bool layout = selection_changes_layout();
      for (std::size_t i = 0; i < selected.size(); ++i) {
        if (auto contact = contacts[i].lock()) {
          if (source_changed)
            cancel_contact_noexcept(contact, false);
          if (selected[i] != candidate->selected[i])
            contact->pending |= layout ? 3U : 2U;
          if (!source_valid)
            contact->pending |= 4U;
        }
      }
      selected.swap(candidate->selected);
      seen = candidate->revision;
      source_valid = true;
      syncing = false;
      flush();
    } catch (...) {
      syncing = false;
      throw;
    }
  }
  void select(std::size_t index, const std::shared_ptr<SegmentContact> &contact,
              std::function<bool()> permission,
              std::optional<std::uint64_t> press = {}) {
    if (!memory->mounted || !memory->active || !contact->mounted ||
        !enabled_at(index) || !model->valid())
      return;
    const auto revision = press.value_or(model->revision());
    const auto generation = memory->generation;
    const auto serial = contact->serial;
    const std::weak_ptr<SegmentState> weak = shared_from_this();
    const std::weak_ptr<SegmentContact> weak_contact = contact;
    auto allowed = [weak, weak_contact, generation, serial, revision,
                    permission = std::move(permission)] {
      const auto state = weak.lock();
      const auto current = weak_contact.lock();
      return state && current && state->memory->mounted &&
             state->memory->active && state->memory->generation == generation &&
             current->mounted && current->serial == serial &&
             state->model->valid() && state->model->revision() == revision &&
             (!current->allowed || current->allowed()) &&
             (!permission || permission());
    };
    if (!allowed())
      return;
    contact->pending |= selection_changes_layout() ? 3U : 2U;
    flush();
    if (!allowed())
      return;
    model->select(index, allowed);
  }
};
class Segment final : public Component,
                      public ThemeBinding,
                      public FocusGroupParticipant {
public:
  Segment(std::shared_ptr<SegmentState> state, std::size_t index)
      : state_(std::move(state)), index_(index),
        contact_(std::make_shared<SegmentContact>()) {}
  bool focusable() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return true; }
  ComponentAvailability local_availability() const noexcept override {
    return {VisibilityMode::Visible, state_->enabled_at(index_),
            !state_->model->valid()};
  }
  const void *focus_group_identity() const noexcept override {
    return state_->memory.get();
  }
  bool focus_group_selected() const override {
    const auto selected = state_->selected_enabled();
    return selected ? *selected == index_ : state_->memory->last == index_;
  }
  bool focus_group_accepts_boundary_key(Key key) const noexcept override {
    return key == Key::Home || key == Key::End;
  }
  void focus_group_did_focus() noexcept override {
    if (state_->memory->mounted && state_->memory->active)
      state_->memory->last = index_;
  }
  std::uint64_t focus_group_selection_revision() const noexcept override {
    return state_->model->revision();
  }
  void focus_group_select() override {
    auto allowed = contact_->allowed;
    focus_group_select_guarded(allowed);
  }
  void
  focus_group_select_guarded(const std::function<bool()> &allowed) override {
    auto state = state_;
    auto contact = contact_;
    const auto index = index_;
    const auto revision = state->model->revision();
    cancel_contact(contact, false);
    state->select(index, contact, allowed, revision);
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto style = resolved();
    TextStyle text;
    text.size = style.text_size;
    text.family = style.font_family;
    text.fallback_families = style.fallback_families;
    text.weight = style.text_weight;
    text.slant = style.text_slant;
    return {
        std::max(
            style.minimum_width,
            TextService::measure(state_->recipe->labels[index_], text).width +
                2 * style.horizontal_padding),
        style.control_height};
  }
  void mount(MountContext &context) override {
    contact_->mounted = true;
    contact_->allowed = InputMutationAccess::guard(context);
    contact_->invalidate = context.invalidator();
    contact_->layout = context.layout_invalidator();
    contact_->availability = context.availability_invalidator();
    state_->contacts[index_] = contact_;
  }
  void unmount(LifecycleContext &) override {
    contact_->mounted = false;
    cancel_contact_noexcept(contact_, true);
    contact_->allowed = {};
    contact_->invalidate = {};
    contact_->layout = {};
    contact_->availability = {};
    state_->contacts[index_].reset();
  }
  void deactivate(LifecycleContext &) override {
    cancel_contact_noexcept(contact_, true);
    contact_->focused = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    auto contact = contact_;
    contact->focused = focused;
    if (!focused)
      cancel_contact(contact, true);
    context.invalidate();
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    if (action != SemanticAction::Select && action != SemanticAction::Activate)
      return EventResult::Ignored;
    auto state = state_;
    auto contact = contact_;
    const auto index = index_;
    auto guard = InputMutationAccess::guard(context);
    const auto revision = state->model->revision();
    cancel_contact(contact, true);
    context.invalidate();
    state->select(index, contact, std::move(guard), revision);
    return EventResult::Handled;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    auto state = state_;
    auto contact = contact_;
    const auto index = index_;
    bool activate{};
    std::optional<std::uint64_t> revision;
    auto guard = InputMutationAccess::guard(context);
    const auto finish = [&](bool choose) {
      const auto original = revision;
      cancel_contact(contact, false);
      context.invalidate();
      if (choose)
        state->select(index, contact, guard, original);
      return EventResult::Handled;
    };
    if (event.type == InputType::KeyUp && event.key == Key::Enter) {
      contact->enter_down = false;
      return EventResult::Handled;
    }
    if (!InputMutationAccess::allowed(context) || !state->model->valid()) {
      cancel_contact(contact, false);
      if (event.type == InputType::KeyDown && event.key == Key::Enter)
        contact->enter_down = true;
      if (event.type == InputType::KeyDown && event.key == Key::Space)
        contact->space_down = true;
      if (event.type == InputType::KeyUp && event.key == Key::Space)
        contact->space_down = false;
      return (event.type == InputType::PointerDown ||
              event.type == InputType::PointerUp || event.key == Key::Enter ||
              event.key == Key::Space)
                 ? EventResult::Handled
                 : EventResult::Ignored;
    }
    switch (event.type) {
    case InputType::PointerDown:
      cancel_contact(contact, false);
      contact->release = context.pointer_releaser();
      contact->pointer = event.pointer.id;
      contact->press_revision = state->model->revision();
      contact->pointer_armed = true;
      contact->pointer_inside = true;
      contact->hovered = true;
      try {
        context.capture_pointer();
        context.invalidate();
      } catch (...) {
        cancel_contact_noexcept(contact, false);
        throw;
      }
      return EventResult::Handled;
    case InputType::PointerMove: {
      const bool inside = context.bounds().contains(event.position);
      contact->hovered = inside;
      if (contact->pointer_armed && contact->pointer == event.pointer.id)
        contact->pointer_inside = inside;
      context.invalidate();
      return EventResult::Handled;
    }
    case InputType::PointerLeave:
      contact->hovered = false;
      if (contact->pointer == event.pointer.id)
        contact->pointer_inside = false;
      context.invalidate();
      return EventResult::Handled;
    case InputType::PointerUp:
      if (!contact->pointer_armed || contact->pointer != event.pointer.id)
        return EventResult::Ignored;
      activate =
          contact->pointer_inside && context.bounds().contains(event.position);
      revision = contact->press_revision;
      return finish(activate);
    case InputType::PointerCancel:
      if (contact->pointer != event.pointer.id)
        return EventResult::Ignored;
      return finish(false);
    case InputType::KeyDown:
      if (event.key == Key::Enter) {
        if (contact->enter_down)
          return EventResult::Handled;
        contact->enter_down = true;
        revision = state->model->revision();
        return finish(true);
      }
      if (event.key == Key::Space) {
        if (!contact->space_down) {
          contact->space_down = true;
          contact->space_armed = true;
          contact->press_revision = state->model->revision();
          context.invalidate();
        }
        return EventResult::Handled;
      }
      if (event.key == Key::Escape) {
        contact->space_armed = false;
        return finish(false);
      }
      return EventResult::Ignored;
    case InputType::KeyUp:
      if (event.key == Key::Space) {
        activate = contact->space_down && contact->space_armed;
        contact->space_down = false;
        revision = contact->press_revision;
        return finish(activate);
      }
      return EventResult::Ignored;
    default:
      return EventResult::Ignored;
    }
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::RadioButton;
    info.name = state_->recipe->labels[index_];
    info.selected = state_->selected_at(index_);
    info.checked = info.selected ? SemanticCheckedState::Checked
                                 : SemanticCheckedState::Unchecked;
    info.focusable = true;
    info.focused = contact_->focused;
    info.enabled = effective_enabled() && state_->enabled_at(index_);
    info.read_only = effective_read_only() || !state_->model->valid();
    if (info.enabled) {
      info.actions = {SemanticAction::Focus};
      if (!info.read_only)
        info.actions.push_back(SemanticAction::Select);
    }
    return info;
  }
  void paint(PaintContext &context) const override {
    const auto style = resolved();
    const auto bounds = context.bounds();
    auto &painter = context.painter();
    painter.fill_rounded_rect(bounds, style.corner_radius, style.fill);
    painter.stroke_rounded_rect(bounds, style.corner_radius, style.border_width,
                                style.border);
    TextStyle text;
    text.size = style.text_size;
    text.color = style.text;
    text.family = style.font_family;
    text.fallback_families = style.fallback_families;
    text.weight = style.text_weight;
    text.slant = style.text_slant;
    text.align = TextAlign::Center;
    painter.text({bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f},
                 state_->recipe->labels[index_], text);
  }

private:
  ResolvedButtonStyle resolved() const {
    VisualState visual;
    visual.enabled = effective_enabled() && state_->enabled_at(index_);
    visual.read_only = effective_read_only() || !state_->model->valid();
    visual.focused = contact_->focused;
    visual.hovered = contact_->hovered;
    visual.pressed = (contact_->pointer_armed && contact_->pointer_inside) ||
                     contact_->space_armed;
    auto base = default_button_style(current_theme());
    base.base.corner_radius = 0;
    base.base.border_width = 0;
    auto style =
        resolve_button_style(base, state_->recipe->style.segment, visual);
    if (state_->selected_at(index_)) {
      style.fill = current_theme().palette.accent;
      style.border = current_theme().palette.accent;
      style.text = current_theme().palette.background;
      apply_button_style_patch(style, state_->recipe->style.selected);
      // Momentary pressed/disabled and focus remain above persistent selected.
      const auto interaction = resolve_interaction_state(visual);
      apply_button_interaction_patch(style, base, interaction);
      apply_button_interaction_patch(style, state_->recipe->style.segment,
                                     interaction);
      if (visual.read_only) {
        apply_button_style_patch(style, base.read_only);
        apply_button_style_patch(style,
                                 state_->recipe->style.segment.read_only);
      }
      if (visual.focused) {
        apply_button_style_patch(style, base.focused);
        apply_button_style_patch(style, state_->recipe->style.segment.focused);
      }
    }
    return style;
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    if (!after.interactive() || after.read_only)
      cancel_contact_noexcept(contact_, false);
  }
  std::shared_ptr<SegmentState> state_;
  std::size_t index_{};
  std::shared_ptr<SegmentContact> contact_;
};
class SegmentedComponent final : public ControlGroupTrack {
public:
  explicit SegmentedComponent(std::shared_ptr<const SegmentRecipe> recipe)
      : ControlGroupTrack(recipe->label, recipe->style),
        state_(std::make_shared<SegmentState>(std::move(recipe))) {}
  bool uses_retained_checkpoint() const noexcept override { return true; }
  void mount(MountContext &) override {
    state_->memory->mounted = true;
    const std::weak_ptr<SegmentState> weak = state_;
    state_->subscription = state_->model->observe([weak] {
      if (auto state = weak.lock())
        state->sync();
    });
    state_->sync();
  }
  void unmount(LifecycleContext &) override {
    state_->memory->mounted = false;
    state_->memory->active = false;
    ++state_->memory->generation;
    state_->subscription.reset();
  }
  void activate(LifecycleContext &) override {
    state_->memory->active = true;
    state_->read_pending = true;
  }
  void deactivate(LifecycleContext &) override {
    state_->memory->active = false;
    ++state_->memory->generation;
    for (const auto &weak : state_->contacts)
      if (auto contact = weak.lock())
        cancel_contact_noexcept(contact, true);
  }
  std::vector<Spec> children() const {
    std::vector<Spec> result;
    result.reserve(state_->recipe->labels.size());
    for (std::size_t i = 0; i < state_->recipe->labels.size(); ++i)
      result.emplace_back(
          [state = state_, i] { return std::make_unique<Segment>(state, i); },
          std::vector<Spec>{});
    return result;
  }

private:
  void retained_checkpoint() override {
    auto state = state_;
    state->sync();
  }
  std::shared_ptr<SegmentState> state_;
};
} // namespace
Spec make_segmented_control_spec(std::string label,
                                 std::vector<std::string> labels,
                                 std::vector<bool> enabled,
                                 SegmentedControlStyle style,
                                 SegmentedSelectionFactory selection) {
  if (labels.size() != enabled.size())
    throw std::invalid_argument(
        "SegmentedControl labels/options size mismatch");
  auto recipe = std::make_shared<const SegmentRecipe>(
      SegmentRecipe{std::move(label), std::move(labels), std::move(enabled),
                    std::move(style), std::move(selection)});
  Spec result{[recipe] { return std::make_unique<SegmentedComponent>(recipe); },
              {}};
  result.children_factory = [](Component &component) {
    return static_cast<SegmentedComponent &>(component).children();
  };
  return result;
}
} // namespace ui::detail
