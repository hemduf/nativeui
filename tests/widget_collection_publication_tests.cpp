#include "test_support.hpp"

#include <nativeui/enabled.hpp>
#include <nativeui/grid_view.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/table_view.hpp>
#include <nativeui/tree_view.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {
enum class Policy { ReadOnly, Enabled };

// Let committed geometry/window publication settle before arming the exposure
// callback. No source is rewritten during these normal preparatory checkpoints.
void settle(ui::UI& tree, ui::HeadlessRenderer& renderer) {
  for (int pass = 0; pass < 4; ++pass) NUI_CHECK(renderer.render(tree));
}

template <class Value>
void publication_boundary(std::string_view family, Policy policy, ui::State<Value>& value,
                          ui::Spec control, const ui::InputEvent& publication,
                          const Value& accepted) {
  const Value initial = value.snapshot();
  int notifications{};
  auto subscription = value.observe([&](const Value&) { ++notifications; });
  ui::State<bool> enabled{true};
  ui::State<bool> read_only{false};
  ui::UI tree{ui::Enabled{enabled, ui::ReadOnly{read_only, std::move(control)}}};
  test::MockPlatform platform;
  const ui::Size size{100.0f, 120.0f};
  tree.resize(size);
  tree.activate(platform);
  ui::HeadlessRenderer renderer{size, 1.0f};
  settle(tree, renderer);
  NUI_CHECK(value.get() == initial && notifications == 0);

  bool armed{};
  bool before_publication{};
  int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    ++boundaries;
    before_publication = value.get() == initial && notifications == 0;
    if (policy == Policy::ReadOnly) read_only.set(true);
    else enabled.set(false);
  });
  std::cout << "ORACLE " << family << ' '
            << (policy == Policy::ReadOnly ? "read_only" : "enabled") << '\n';
  armed = true;
  tree.dispatch(publication, platform);
  NUI_CHECK(boundaries == 1);
  NUI_CHECK(before_publication);
  NUI_CHECK(value.get() == initial);
  NUI_CHECK(notifications == 0);

  tree.clear_invalidation_callback();
  // Recover normal permission without reconstructing the UI or source model.
  enabled.set(true);
  read_only.set(false);
  settle(tree, renderer);
  tree.dispatch(publication, platform);
  NUI_CHECK(value.get() == accepted);
  NUI_CHECK(notifications == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void grid(Policy policy) {
  ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "one"}, {2, "two"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  auto control = ui::make_spec(ui::GridView<int>{rows, selection}
                                  .minimum_cell_width(80)
                                  .cell_height(32)
                                  .cell([](const auto&) {
                                    return ui::make_spec(ui::Spacer{20, 20});
                                  }));
  publication_boundary("grid_selection", policy, chosen, std::move(control),
                       test::key(ui::Key::Down), ui::SelectionSnapshot<int>{{2}, 2, 2});
}
void tree_expansion(Policy policy) {
  ui::State<std::vector<ui::TreeNode<int>>> rows{
      {{1, {}, "root", true, true}, {2, 1, "child"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  ui::State<std::vector<int>> expanded{{}};
  auto control = ui::make_spec(ui::TreeView<int>{rows, selection, expanded});
  publication_boundary("tree_expansion", policy, expanded, std::move(control),
                       test::key(ui::Key::Right), std::vector<int>{1});
  NUI_CHECK(chosen.get() == (ui::SelectionSnapshot<int>{{1}, 1, 1}));
}
void table_sort(Policy policy) {
  ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "one"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
  auto control = ui::make_spec(ui::TableView<int>{rows, selection}
                                  .columns({{"name", "Name", 100, 60, {}, ui::Align::Start,
                                             true, false}})
                                  .sort(sort));
  publication_boundary("table_sort", policy, sort, std::move(control),
                       test::key(ui::Key::Enter),
                       std::optional<ui::SortOrder>{ui::SortOrder{
                           "name", ui::SortDirection::Ascending}});
}
void grid_read_only() { grid(Policy::ReadOnly); }
void grid_enabled() { grid(Policy::Enabled); }
void tree_read_only() { tree_expansion(Policy::ReadOnly); }
void tree_enabled() { tree_expansion(Policy::Enabled); }
void table_read_only() { table_sort(Policy::ReadOnly); }
void table_enabled() { table_sort(Policy::Enabled); }
void suite() {
  grid_read_only(); grid_enabled();
  tree_read_only(); tree_enabled();
  table_read_only(); table_enabled();
}
} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "grid_read_only") return test::run("collection_grid_read_only", &grid_read_only);
  if (mode == "grid_enabled") return test::run("collection_grid_enabled", &grid_enabled);
  if (mode == "tree_read_only") return test::run("collection_tree_read_only", &tree_read_only);
  if (mode == "tree_enabled") return test::run("collection_tree_enabled", &tree_enabled);
  if (mode == "table_read_only") return test::run("collection_table_read_only", &table_read_only);
  if (mode == "table_enabled") return test::run("collection_table_enabled", &table_enabled);
  if (mode == "all") return test::run("collection_publication", &suite);
  return 2;
}
