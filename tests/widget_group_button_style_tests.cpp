#include "test_support.hpp"
#include <nativeui/button.hpp>
#include <nativeui/toggle_button.hpp>
#if __has_include(<nativeui/detail/group_button_style.hpp>)
#include <nativeui/detail/group_button_style.hpp>
#else
// The old native leaf widgets are kept intact. Only the future private binding
// protocol is declared for this oracle; old components cannot implement it.
namespace ui::detail {
struct GroupButtonStyle {
  ButtonStyle buttons;
  ButtonStylePatch selected;
};
class GroupButtonStyleTarget {
public:
  virtual ~GroupButtonStyleTarget() = default;
  virtual void
      bind_group_style(std::shared_ptr<const GroupButtonStyle>) noexcept = 0;
};
} // namespace ui::detail
#endif
namespace {
class Scope final : public ui::Component {
public:
  explicit Scope(std::shared_ptr<const ui::detail::GroupButtonStyle> recipe)
      : recipe_(std::move(recipe)) {}
  ui::Size
  measure(const std::vector<ui::ChildMetrics> &children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  ui::Size
  minimum_size(const std::vector<ui::ChildMetrics> &children) const override {
    return children.empty() ? ui::Size{} : children.front().minimum;
  }
  void
  layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics> &,
                  std::vector<ui::ChildPlacement> &children) const override {
    if (!children.empty())
      children.front().bounds = bounds;
  }
  void paint(ui::PaintContext &) const override {}

private:
  void bind_descendant_context(ui::Component &component) const override {
    if (auto *target =
            dynamic_cast<ui::detail::GroupButtonStyleTarget *>(&component))
      target->bind_group_style(recipe_);
  }
  std::shared_ptr<const ui::detail::GroupButtonStyle> recipe_;
};
ui::Spec scope(float width, ui::Spec child, float selected_width = 0) {
  auto style = std::make_shared<ui::detail::GroupButtonStyle>();
  style->buttons.base.minimum_width = width;
  style->buttons.base.horizontal_padding = 0.0f;
  style->buttons.base.text_size = 1.0f;
  if (selected_width > 0)
    style->selected.minimum_width = selected_width;
  return {[style] { return std::make_unique<Scope>(style); },
          {std::move(child)}};
}
void inherited_recipe_changes_actual_leaf_measurement() {
  ui::UI ordinary{ui::Button{"x", {}}};
  ui::UI scoped{scope(203, ui::Button{"x", {}}.spec())};
  NUI_CHECK_NEAR(scoped.measure().preferred.w, 203, 0.01f);
  NUI_CHECK(ordinary.measure().preferred.w < 203);
}
void nearest_scope_and_own_patch_have_priority() {
  ui::UI nested{scope(203, scope(117, ui::Button{"x", {}}.spec()))};
  NUI_CHECK_NEAR(nested.measure().preferred.w, 117, 0.01f);
  ui::ButtonStyle own;
  own.base.minimum_width = 71.0f;
  ui::UI explicit_style{scope(203, ui::Button{"x", {}}.style(own).spec())};
  NUI_CHECK_NEAR(explicit_style.measure().preferred.w, 71, 0.01f);
  ui::State<bool> value{true};
  ui::ToggleButtonStyle own_toggle;
  own_toggle.selected.minimum_width = 79.0f;
  ui::UI toggle{
      scope(203, ui::ToggleButton{"x", value}.style(own_toggle).spec(), 229)};
  NUI_CHECK_NEAR(toggle.measure().preferred.w, 79, 0.01f);
}
void selected_metrics_reconcile_without_remount() {
  ui::State<bool> value{false};
  ui::UI tree{scope(113, ui::ToggleButton{"x", value}.spec(), 227)};
  test::MockPlatform platform;
  tree.resize({300, 60});
  tree.activate(platform);
  NUI_CHECK_NEAR(tree.measure().preferred.w, 113, 0.01f);
  value.set(true);
  NUI_CHECK(tree.layout_dirty());
  NUI_CHECK_NEAR(tree.measure().preferred.w, 227, 0.01f);
  value.set(false);
  NUI_CHECK_NEAR(tree.measure().preferred.w, 113, 0.01f);
}
void suite() {
  inherited_recipe_changes_actual_leaf_measurement();
  nearest_scope_and_own_patch_have_priority();
  selected_metrics_reconcile_without_remount();
}
} // namespace
int main(int argc, char **argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "inherit")
    return test::run("group_style_inherit",
                     &inherited_recipe_changes_actual_leaf_measurement);
  if (mode == "nearest")
    return test::run("group_style_nearest",
                     &nearest_scope_and_own_patch_have_priority);
  if (mode == "selected")
    return test::run("group_style_selected",
                     &selected_metrics_reconcile_without_remount);
  if (mode == "all")
    return test::run("group_button_style", &suite);
  return 2;
}
