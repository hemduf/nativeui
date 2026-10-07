#include "detail/layout_support.hpp"
#include "detail/widget_control_group.hpp"
#include "detail/widget_menu_popup.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <nativeui/detail/interaction_observer.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/toolbar.hpp>
#include <stdexcept>
#include <unordered_set>
#include <utility>
namespace ui::detail {
namespace {
struct ToolbarRecipe {
  std::string label;
  std::vector<ToolbarItem> items;
  ToolbarStyle style;
};
struct ToolbarPlan {
  std::vector<bool> visible;
  std::vector<std::size_t> overflow;
  std::vector<Rect> bounds;
  bool more{};
};
struct ToolbarState final {
  explicit ToolbarState(std::shared_ptr<const ToolbarRecipe> value)
      : recipe(std::move(value)),
        memory(std::make_shared<ControlGroupMemory>()),
        menu(std::make_shared<MenuAnchorRuntime>()) {
    auto style = recipe->style.controls;
    if (!style.base.fill)
      style.base.fill = Color{0, 0, 0, 0};
    if (!style.base.border)
      style.base.border = Color{0, 0, 0, 0};
    presentation = make_control_group_style(std::move(style));
  }
  std::shared_ptr<const ToolbarRecipe> recipe;
  std::shared_ptr<ControlGroupMemory> memory;
  std::shared_ptr<MenuAnchorRuntime> menu;
  std::shared_ptr<const GroupButtonStyle> presentation;
  std::shared_ptr<const ToolbarPlan> plan;
  std::function<void()> invalidate_availability, request_more;
  std::function<bool()> allowed;
  unsigned pending{};
  bool focus_within{};
  bool visible(std::size_t index) const noexcept {
    return plan ? index < plan->visible.size() && plan->visible[index]
                : index < recipe->items.size();
  }
  bool more_visible() const noexcept { return plan && plan->more; }
};
class ToolbarSlot final : public ControlGroupItem {
public:
  ToolbarSlot(std::shared_ptr<ToolbarState> state, std::size_t index)
      : ControlGroupItem(state->memory, index, false),
        state_(std::move(state)) {}
  ComponentAvailability local_availability() const noexcept override {
    return {state_->visible(index_) ? VisibilityMode::Visible
                                    : VisibilityMode::Hidden,
            true, false};
  }

private:
  std::shared_ptr<ToolbarState> state_;
};
TextStyle toolbar_text(const ResolvedComboBoxStyle &style) {
  TextStyle text;
  text.size = style.text_size;
  text.color = style.text;
  text.weight = style.text_weight;
  text.slant = style.text_slant;
  text.family = style.font_family;
  text.fallback_families = style.fallback_families;
  text.align = TextAlign::Center;
  return text;
}
class ToolbarMore final : public Component,
                          public ThemeBinding,
                          public OverlayCommandSource,
                          public OverlayAnchorPolicy {
public:
  explicit ToolbarMore(std::shared_ptr<ToolbarState> state)
      : state_(std::move(state)) {}
  bool focusable() const noexcept override { return true; }
  bool roving_focus_target() const noexcept override { return true; }
  bool cancel_capture_on_read_only() const noexcept override { return false; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  bool dismiss_overlay_on_tab() const noexcept override { return true; }
  bool overlay_handles_escape() const noexcept override { return true; }
  bool overlay_session_valid() const noexcept override {
    return menu_anchor_live(state_->menu);
  }
  Size measure(const std::vector<ChildMetrics> &) const override {
    const auto style = resolved();
    return {std::max(style.minimum_width,
                     TextService::measure(state_->recipe->style.overflow_label,
                                          toolbar_text(style))
                             .width +
                         2 * style.horizontal_padding),
            style.control_height};
  }
  void mount(MountContext &context) override {
    state_->menu->node_id = context.node_id();
    state_->menu->mounted = true;
    state_->menu->permission = InputMutationAccess::action_guard(context);
    state_->menu->invalidate = context.invalidator();
    state_->request_more = context.focus_requester();
  }
  void unmount(LifecycleContext &context) override {
    state_->menu->mounted = false;
    retire_menu_anchor(state_->menu);
    state_->menu->permission = {};
    state_->menu->invalidate = {};
    state_->request_more = {};
    press_.deactivate(context, false);
  }
  void activate(LifecycleContext &) override { state_->menu->active = true; }
  void deactivate(LifecycleContext &context) override {
    state_->menu->active = false;
    retire_menu_anchor(state_->menu);
    press_.deactivate(context, false);
    focused_ = false;
  }
  void focus_changed(bool focused, FocusContext &context) override {
    focused_ = focused;
    press_.focus_changed(focused, context, false);
    context.invalidate();
  }
  EventResult semantic_action(SemanticAction action,
                              InputContext &context) override {
    return action == SemanticAction::Activate
               ? open_owned(state_, context, Key::None)
               : EventResult::Ignored;
  }
  EventResult input(const InputEvent &event, InputContext &context) override {
    const auto state = state_;
    const auto menu = state->menu;
    if (menu->suppress_until_key_up != Key::None &&
        event.key == menu->suppress_until_key_up) {
      if (event.type == InputType::KeyUp)
        menu->suppress_until_key_up = Key::None;
      return EventResult::Handled;
    }
    if (event.type == InputType::KeyDown && event.key == Key::Down)
      return open_owned(state, context, event.key);
    const auto outcome = press_.input(event, context, true, false);
    context.invalidate();
    if (outcome.activate)
      return open_owned(state, context,
                        event.type == InputType::KeyDown ? event.key
                                                         : Key::None);
    return outcome.result;
  }
  std::optional<OverlayComponentCommand> take_overlay_command() override {
    return take_menu_command(state_->menu);
  }
  bool has_pending_overlay_command() const noexcept override {
    return !state_->menu->commands.empty();
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Button;
    info.name = state_->recipe->style.overflow_label;
    info.focusable = true;
    info.focused = focused_;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
    const auto session = state_->menu->current.lock();
    info.expanded = session && session->live ? SemanticExpandedState::Expanded
                                             : SemanticExpandedState::Collapsed;
    if (info.enabled)
      info.actions = {SemanticAction::Activate, SemanticAction::Focus};
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
    painter.text({bounds.x + bounds.w * .5f, bounds.y + bounds.h * .5f},
                 state_->recipe->style.overflow_label, toolbar_text(style));
  }

private:
  static EventResult open_owned(std::shared_ptr<ToolbarState> state,
                                InputContext &context, Key key) {
    const std::weak_ptr<ToolbarState> weak = state;
    const auto generation = state->memory->generation;
    PopupMenu::ItemsProvider provider = [weak, generation] {
      const auto current = weak.lock();
      std::vector<PopupMenuItem> result;
      if (!current || !current->memory->mounted || !current->memory->active ||
          current->memory->generation != generation || !current->plan)
        return result;
      const auto plan = current->plan;
      result.reserve(plan->overflow.size());
      for (const auto index : plan->overflow) {
        if (!current->memory->mounted || !current->memory->active ||
            current->memory->generation != generation ||
            !current->menu->mounted || !current->menu->active ||
            (current->menu->permission && !current->menu->permission()))
          return std::vector<PopupMenuItem>{};
        const auto &metadata = current->recipe->items[index].overflow_item;
        if (metadata)
          result.push_back(*metadata);
      }
      return result;
    };
    return open_menu(state->menu, provider, state->recipe->style.overflow_items,
                     context, key);
  }
  ResolvedComboBoxStyle resolved() const {
    VisualState visual;
    visual.enabled = effective_enabled();
    visual.read_only = effective_read_only();
    visual.focused = focused_;
    visual.hovered = press_.hovered();
    visual.pressed = press_.pressed();
    return resolve_combo_box_style(default_combo_box_style(current_theme()),
                                   state_->recipe->style.overflow_button,
                                   visual);
  }
  void effective_availability_changed(
      const ComponentAvailability &,
      const ComponentAvailability &after) noexcept override {
    state_->menu->interactive = after.interactive();
    if (!after.interactive())
      retire_menu_anchor(state_->menu);
  }
  void retained_checkpoint() override { reconcile_menu_anchor(state_->menu); }
  std::shared_ptr<ToolbarState> state_;
  PressActivationState press_;
  bool focused_{};
};
class ToolbarComponent final : public Component,
                               public ThemeBinding,
                               public RetainedInteractionObserver {
public:
  explicit ToolbarComponent(std::shared_ptr<const ToolbarRecipe> recipe)
      : state_(std::make_shared<ToolbarState>(std::move(recipe))) {}
  bool focusable() const noexcept override { return false; }
  bool clips_children() const noexcept override { return true; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  Size measure(const std::vector<ChildMetrics> &children) const override {
    return metrics(children, false);
  }
  Size minimum_size(const std::vector<ChildMetrics> &children) const override {
    return metrics(children, true);
  }
  Constraints child_constraints(const Constraints &constraints, std::size_t,
                                std::size_t) const override {
    return Constraints::loose(
        {kUnboundedExtent,
         std::max(0.0f,
                  constraints.max.h - 2 * state_->recipe->style.padding)});
  }
  void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                       std::vector<ChildPlacement> &placements) const override {
    const auto count = state_->recipe->items.size();
    if (children.size() != count + 1)
      throw std::logic_error("Toolbar child count mismatch");
    const auto &style = state_->recipe->style;
    const float available = std::max(0.0f, bounds.w - 2 * style.padding),
                height = std::max(0.0f, bounds.h - 2 * style.padding);
    auto plan = std::make_shared<ToolbarPlan>();
    plan->visible.resize(count + 1);
    plan->bounds.resize(count + 1);
    for (std::size_t i = 0; i < count; ++i)
      plan->visible[i] = children[i].participates_in_layout;
    const auto total = [&](bool more) {
      double width{};
      std::size_t visible{};
      for (std::size_t i = 0; i < count; ++i)
        if (plan->visible[i]) {
          width += children[i].preferred.w;
          ++visible;
        }
      if (more) {
        width += children[count].preferred.w;
        ++visible;
      }
      if (visible > 1)
        width += style.gap * static_cast<double>(visible - 1);
      return width;
    };
    if (total(false) > available) {
      for (std::size_t distance = 0; distance < count; ++distance) {
        const auto i = count - 1 - distance;
        if (!plan->visible[i] || !state_->recipe->items[i].overflow_item)
          continue;
        plan->visible[i] = false;
        plan->overflow.push_back(i);
        if (total(true) <= available)
          break;
      }
      std::reverse(plan->overflow.begin(), plan->overflow.end());
      plan->more = !plan->overflow.empty();
    }
    plan->visible[count] = plan->more;
    auto arranged = children;
    for (std::size_t i = 0; i < arranged.size(); ++i)
      arranged[i].participates_in_layout = plan->visible[i];
    const auto allocation =
        allocate_main_axis(arranged, available, style.gap, true);
    float x = bounds.x + style.padding;
    for (std::size_t i = 0; i < children.size(); ++i) {
      if (plan->visible[i]) {
        plan->bounds[i] = {x, bounds.y + style.padding, allocation.extents[i],
                           height};
        x = saturating_coordinate(static_cast<double>(x) +
                                  allocation.extents[i] + style.gap);
      } else
        plan->bounds[i] = {bounds.x + style.padding, bounds.y + style.padding,
                           0, height};
      placements[i].bounds = plan->bounds[i];
    }
    pending_plan_ = std::move(plan);
  }
  void mount(MountContext &context) override {
    state_->memory->mounted = true;
    state_->allowed = InputMutationAccess::action_guard(context);
    state_->invalidate_availability = context.availability_invalidator();
  }
  void unmount(LifecycleContext &) override {
    state_->memory->mounted = false;
    state_->memory->active = false;
    ++state_->memory->generation;
    state_->focus_within = false;
    state_->pending = 0;
    state_->allowed = {};
    state_->invalidate_availability = {};
  }
  void activate(LifecycleContext &) override { state_->memory->active = true; }
  void deactivate(LifecycleContext &) override {
    state_->memory->active = false;
    ++state_->memory->generation;
    state_->focus_within = false;
    state_->pending &= ~2U;
  }
  void retained_focus_within_changed(bool focused, bool, Dispatcher) override {
    state_->focus_within = focused;
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;
    info.role = SemanticRole::Group;
    info.name = state_->recipe->label;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only();
    return info;
  }
  void paint(PaintContext &context) const override {
    context.painter().fill_rounded_rect(context.bounds(), 0,
                                        current_theme().palette.surface);
  }
  std::vector<Spec> children() const {
    std::vector<Spec> result;
    result.reserve(state_->recipe->items.size() + 1);
    for (std::size_t i = 0; i < state_->recipe->items.size(); ++i)
      result.push_back(Spec{[state = state_, i] {
                              return std::make_unique<ToolbarSlot>(state, i);
                            },
                            {state_->recipe->items[i].content}});
    Spec more{[state = state_] { return std::make_unique<ToolbarMore>(state); },
              {}};
    const auto index = state_->recipe->items.size();
    result.push_back(Spec{[state = state_, index] {
                            return std::make_unique<ToolbarSlot>(state, index);
                          },
                          {std::move(more)}});
    return result;
  }

private:
  Size metrics(const std::vector<ChildMetrics> &children, bool minimum) const {
    const auto &style = state_->recipe->style;
    double width = 2 * style.padding;
    float height = style.minimum_height;
    std::size_t count{};
    for (std::size_t i = 0;
         i < state_->recipe->items.size() && i < children.size(); ++i) {
      if (!children[i].participates_in_layout)
        continue;
      const auto size = minimum ? children[i].minimum : children[i].preferred;
      width += size.w;
      height = std::max(height, size.h);
      ++count;
    }
    if (count > 1)
      width += style.gap * static_cast<double>(count - 1);
    return {saturating_extent(width),
            saturating_extent(height + 2 * style.padding)};
  }
  void bind_descendant_context(Component &component) const override {
    bind_control_group_style(component, state_->presentation);
  }
  void layout_committed(Rect, Rect) noexcept override {
    if (!pending_plan_)
      return;
    const bool changed =
        !state_->plan || state_->plan->visible != pending_plan_->visible;
    if (changed) {
      state_->pending |= 1U;
      const auto last = state_->memory->last;
      if (state_->focus_within && last &&
          *last < pending_plan_->visible.size() &&
          !pending_plan_->visible[*last] && pending_plan_->more &&
          !state_->menu->handle.valid())
        state_->pending |= 2U;
    }
    state_->plan = std::move(pending_plan_);
  }
  void retained_checkpoint() override {
    const auto state = state_;
    if (!state->memory->mounted)
      return;
    if (state->pending & 1U) {
      auto callback = state->invalidate_availability;
      state->pending &= ~1U;
      if (callback)
        callback();
    }
    if (!state->memory->mounted || !state->memory->active)
      return;
    if ((state->pending & 2U) && state->more_visible() &&
        !state->menu->handle.valid()) {
      auto callback = state->request_more;
      if (callback) {
        state->pending &= ~2U;
        callback();
      }
    }
  }
  std::shared_ptr<ToolbarState> state_;
  mutable std::shared_ptr<const ToolbarPlan> pending_plan_;
};
} // namespace
} // namespace ui::detail
namespace ui {
Toolbar::Toolbar(std::string label, std::vector<ToolbarItem> items)
    : label_(std::move(label)), items_(std::move(items)) {}
Toolbar &&Toolbar::style(ToolbarStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec Toolbar::spec() && {
  std::unordered_set<std::string> keys;
  for (const auto &item : items_)
    if (item.key.empty() || !keys.insert(item.key).second)
      throw std::invalid_argument("Toolbar keys must be nonempty and unique");
  style_.padding = detail::group_extent(style_.padding);
  style_.gap = detail::group_extent(style_.gap);
  style_.minimum_height = detail::group_extent(style_.minimum_height);
  auto recipe =
      std::make_shared<const detail::ToolbarRecipe>(detail::ToolbarRecipe{
          std::move(label_), std::move(items_), std::move(style_)});
  Spec result{
      [recipe] { return std::make_unique<detail::ToolbarComponent>(recipe); },
      {}};
  result.children_factory = [](Component &component) {
    return static_cast<detail::ToolbarComponent &>(component).children();
  };
  return result;
}
} // namespace ui
