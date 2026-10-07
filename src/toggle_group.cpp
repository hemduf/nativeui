#include "detail/layout_support.hpp"
#include "detail/widget_control_group.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <nativeui/detail/group_button_style.hpp>
#include <nativeui/toggle_group.hpp>
#include <utility>
namespace ui::detail {
std::shared_ptr<const GroupButtonStyle>
make_control_group_style(ButtonStyle buttons, ButtonStylePatch selected) {
  return std::make_shared<const GroupButtonStyle>(
      GroupButtonStyle{std::move(buttons), std::move(selected)});
}
void bind_control_group_style(
    Component &component,
    const std::shared_ptr<const GroupButtonStyle> &style) noexcept {
  if (auto *target = dynamic_cast<GroupButtonStyleTarget *>(&component))
    target->bind_group_style(style);
}

float group_extent(float value) noexcept {
  return std::isfinite(value) ? std::max(0.0f, value) : 0;
}
ControlGroupItem::ControlGroupItem(std::shared_ptr<ControlGroupMemory> memory,
                                   std::size_t index, bool vertical)
    : memory_(std::move(memory)), index_(index), vertical_(vertical) {}
bool ControlGroupItem::focusable() const noexcept { return false; }
FlexFactors ControlGroupItem::flex_factors() const noexcept {
  return measured_flex_;
}
ChildMetrics ControlGroupItem::measure_constrained(
    const Constraints &constraints,
    const std::vector<ChildMetrics> &children) const {
  auto result =
      AvailabilityWrapperComponent::measure_constrained(constraints, children);
  measured_flex_ = !children.empty() && children.front().participates_in_layout
                       ? children.front().flex
                       : FlexFactors{};
  result.flex = measured_flex_;
  return result;
}
const void *ControlGroupItem::focus_group_identity() const noexcept {
  return memory_.get();
}
bool ControlGroupItem::focus_group_selected() const {
  return memory_->last == index_;
}
void ControlGroupItem::focus_group_select() {}
void ControlGroupItem::focus_group_did_focus() noexcept {
  if (memory_->mounted && memory_->active)
    memory_->last = index_;
}
bool ControlGroupItem::focus_group_accepts_navigation_key(
    Key key) const noexcept {
  return key == Key::Left || key == Key::Right ||
         (vertical_ && (key == Key::Up || key == Key::Down));
}
bool ControlGroupItem::focus_group_accepts_boundary_key(
    Key key) const noexcept {
  return key == Key::Home || key == Key::End;
}
Spec control_group_item(const std::shared_ptr<ControlGroupMemory> &memory,
                        std::size_t index, bool vertical, Spec child) {
  return {[memory, index, vertical] {
            return std::make_unique<ControlGroupItem>(memory, index, vertical);
          },
          {std::move(child)}};
}
ControlGroupTrack::ControlGroupTrack(std::string label, ToggleGroupStyle style)
    : label_(std::move(label)), style_(std::move(style)) {
  style_.padding = group_extent(style_.padding);
  style_.gap = group_extent(style_.gap);
  style_.border_width = group_extent(style_.border_width);
  if (!style_.segment.base.corner_radius)
    style_.segment.base.corner_radius = 0.0f;
  if (!style_.segment.base.border_width)
    style_.segment.base.border_width = 0.0f;
  presentation_ = make_control_group_style(style_.segment, style_.selected);
}
bool ControlGroupTrack::focusable() const noexcept { return false; }
bool ControlGroupTrack::clips_children() const noexcept { return true; }
Size ControlGroupTrack::measure(
    const std::vector<ChildMetrics> &children) const {
  double width = 2 * style_.padding;
  float height{};
  bool previous{};
  for (const auto &child : children) {
    if (!child.participates_in_layout)
      continue;
    if (previous)
      width += style_.gap;
    width += child.preferred.w;
    height = std::max(height, child.preferred.h);
    previous = true;
  }
  return {saturating_extent(width),
          saturating_extent(height + 2 * style_.padding)};
}
Size ControlGroupTrack::minimum_size(
    const std::vector<ChildMetrics> &children) const {
  double width = 2 * style_.padding;
  float height{};
  bool previous{};
  for (const auto &child : children) {
    if (!child.participates_in_layout)
      continue;
    if (previous)
      width += style_.gap;
    width += child.minimum.w;
    height = std::max(height, child.minimum.h);
    previous = true;
  }
  return {saturating_extent(width),
          saturating_extent(height + 2 * style_.padding)};
}
Constraints ControlGroupTrack::child_constraints(const Constraints &constraints,
                                                 std::size_t,
                                                 std::size_t) const {
  return Constraints::loose(
      {kUnboundedExtent,
       std::max(0.0f, constraints.max.h - 2 * style_.padding)});
}
void ControlGroupTrack::layout_children(
    Rect bounds, const std::vector<ChildMetrics> &children,
    std::vector<ChildPlacement> &placements) const {
  const float width = std::max(0.0f, bounds.w - 2 * style_.padding),
              height = std::max(0.0f, bounds.h - 2 * style_.padding);
  const auto allocation = allocate_main_axis(children, width, style_.gap, true);
  float x = bounds.x + style_.padding;
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (!children[i].participates_in_layout) {
      placements[i].bounds = {bounds.x, bounds.y, 0, 0};
      continue;
    }
    placements[i].bounds = {x, bounds.y + style_.padding, allocation.extents[i],
                            height};
    x = saturating_coordinate(static_cast<double>(x) + allocation.extents[i] +
                              style_.gap);
  }
}
SemanticInfo ControlGroupTrack::semantics() const {
  SemanticInfo info;
  info.role = SemanticRole::Group;
  info.name = label_;
  info.enabled = effective_enabled();
  info.read_only = effective_read_only();
  return info;
}
void ControlGroupTrack::paint(PaintContext &context) const {
  const auto radius = style_.radius ? group_extent(*style_.radius)
                                    : current_theme().radii.medium;
  context.painter().fill_rounded_rect(
      context.bounds(), radius,
      style_.fill.value_or(current_theme().palette.surface));
  context.painter().stroke_rounded_rect(
      context.bounds(), radius, style_.border_width,
      style_.border.value_or(current_theme().palette.border));
}
void ControlGroupTrack::bind_descendant_context(Component &component) const {
  bind_control_group_style(component, presentation_);
}
namespace {
struct ToggleRecipe {
  std::string label;
  std::vector<Spec> controls;
  ToggleGroupStyle style;
};
class ToggleGroupComponent final : public ControlGroupTrack {
public:
  explicit ToggleGroupComponent(std::shared_ptr<const ToggleRecipe> recipe)
      : ControlGroupTrack(recipe->label, recipe->style),
        recipe_(std::move(recipe)),
        memory_(std::make_shared<ControlGroupMemory>()) {}
  void mount(MountContext &) override { memory_->mounted = true; }
  void unmount(LifecycleContext &) override {
    memory_->mounted = false;
    memory_->active = false;
    ++memory_->generation;
  }
  void activate(LifecycleContext &) override { memory_->active = true; }
  void deactivate(LifecycleContext &) override {
    memory_->active = false;
    ++memory_->generation;
  }
  std::vector<Spec> children() const {
    std::vector<Spec> result;
    result.reserve(recipe_->controls.size());
    for (std::size_t i = 0; i < recipe_->controls.size(); ++i)
      result.push_back(
          control_group_item(memory_, i, true, recipe_->controls[i]));
    return result;
  }

private:
  std::shared_ptr<const ToggleRecipe> recipe_;
  std::shared_ptr<ControlGroupMemory> memory_;
};
} // namespace
} // namespace ui::detail
namespace ui {
ToggleGroup::ToggleGroup(std::string label, std::vector<Spec> controls)
    : label_(std::move(label)), controls_(std::move(controls)) {}
ToggleGroup &&ToggleGroup::style(ToggleGroupStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}
Spec ToggleGroup::spec() && {
  auto recipe =
      std::make_shared<const detail::ToggleRecipe>(detail::ToggleRecipe{
          std::move(label_), std::move(controls_), std::move(style_)});
  Spec result{[recipe] {
                return std::make_unique<detail::ToggleGroupComponent>(recipe);
              },
              {}};
  result.children_factory = [](Component &component) {
    return static_cast<detail::ToggleGroupComponent &>(component).children();
  };
  return result;
}
} // namespace ui
