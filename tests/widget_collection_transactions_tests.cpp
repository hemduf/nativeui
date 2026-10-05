#include "test_support.hpp"

#include <nativeui/grid_view.hpp>
#include <nativeui/table_view.hpp>
#include <nativeui/tree_view.hpp>

#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void settle(ui::UI& tree, ui::HeadlessRenderer& renderer) {
  for (int pass = 0; pass < 4; ++pass) NUI_CHECK(renderer.render(tree));
}
void click(ui::UI& tree, test::MockPlatform& platform, float x, float y, int clicks = 1) {
  auto down = test::pointer(ui::InputType::PointerDown, x, y);
  down.clicks = clicks;
  tree.dispatch(down, platform);
  auto up = test::pointer(ui::InputType::PointerUp, x, y);
  up.clicks = clicks;
  tree.dispatch(up, platform);
}

// The candidate was resolved from {1,2}. Replacing the source at exposure must
// not let old tokens publish key2 into {9,10}, even though preparation of the
// adapter itself begins only after that callback has returned.
void selection_rejects_rows_replaced_at_exposure() {
  ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "one"}, {2, "two"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  int notifications{};
  auto subscription = chosen.observe([&](const auto&) { ++notifications; });
  ui::UI tree{ui::GridView<int>{rows, selection}
                  .minimum_cell_width(80).cell_height(32).gap(0)
                  .cell([](const auto&) { return ui::make_spec(ui::Spacer{20, 20}); })};
  test::MockPlatform platform;
  tree.resize({100, 120}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{100, 120}, 1};
  settle(tree, renderer);
  bool armed{}, before_publication{};
  int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    ++boundaries;
    before_publication = chosen.get() == (ui::SelectionSnapshot<int>{{1}, 1, 1}) &&
                         notifications == 0;
    rows.set({{9, "nine"}, {10, "ten"}});
  });
  armed = true;
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(boundaries == 1 && before_publication);
  NUI_CHECK(rows.get() == (std::vector<ui::CollectionItem<int>>{{9, "nine"}, {10, "ten"}}));
  NUI_CHECK(chosen.get() == (ui::SelectionSnapshot<int>{{1}, 1, 1}));
  NUI_CHECK(notifications == 0);
  tree.clear_invalidation_callback();
  settle(tree, renderer);
  tree.dispatch(test::key(ui::Key::Home), platform);
  NUI_CHECK(chosen.get() == (ui::SelectionSnapshot<int>{{9}, 9, 9}));
  NUI_CHECK(notifications == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

// An external expansion of a different valid branch supersedes the prepared
// toggle. A fresh Right may subsequently open root1 while preserving root3.
void toggle_preserves_expansion_replaced_at_exposure() {
  ui::State<std::vector<ui::TreeNode<int>>> rows{
      {{1, {}, "root1", true, true}, {2, 1, "child"}, {3, {}, "root3", true, true}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  ui::State<std::vector<int>> expanded{{}};
  int notifications{}, callbacks{};
  auto subscription = expanded.observe([&](const auto&) { ++notifications; });
  ui::UI tree{ui::TreeView<int>{rows, selection, expanded}
                  .on_expansion_change([&](const auto&) { ++callbacks; })};
  test::MockPlatform platform;
  tree.resize({160, 120}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{160, 120}, 1};
  settle(tree, renderer);
  bool armed{}, before_publication{};
  int boundaries{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    ++boundaries;
    before_publication = expanded.get().empty() && notifications == 0 && callbacks == 0;
    expanded.set({3});
  });
  armed = true;
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(boundaries == 1 && before_publication);
  NUI_CHECK(expanded.get() == std::vector<int>{3});
  NUI_CHECK(notifications == 1 && callbacks == 0);
  NUI_CHECK(chosen.get() == (ui::SelectionSnapshot<int>{{1}, 1, 1}));
  tree.clear_invalidation_callback();
  settle(tree, renderer);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(expanded.get() == (std::vector<int>{1, 3}));
  NUI_CHECK(notifications == 2 && callbacks == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void autofit_preserves_provider_external_layout() {
  ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "one"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  ui::State<ui::TableLayout> layout{{}};
  int notifications{}, callbacks{}, provider_calls{};
  auto subscription = layout.observe([&](const auto&) { ++notifications; });
  bool armed{}, before_publication{};
  ui::UI tree{ui::TableView<int>{rows, selection}
                  .columns({{"name", "Name", 100, 60, 160, ui::Align::Start, false, false}})
                  .layout(layout)
                  .on_layout_change([&](const auto&) { ++callbacks; })
                  .autofit_width([&](const ui::ColumnId& id) {
                    NUI_CHECK(id == "name");
                    ++provider_calls;
                    if (std::exchange(armed, false)) {
                      before_publication = layout.get().widths.empty() && notifications == 0 &&
                                           callbacks == 0;
                      layout.set({{}, {{"name", 140}}});
                    }
                    return 80.0;
                  })};
  test::MockPlatform platform;
  tree.resize({200, 120}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{200, 120}, 1};
  settle(tree, renderer);
  armed = true;
  click(tree, platform, 99, 10, 2);
  NUI_CHECK(provider_calls == 1 && before_publication);
  NUI_CHECK(layout.get().widths.at("name") == 140);
  NUI_CHECK(notifications == 1 && callbacks == 0);
  settle(tree, renderer);
  click(tree, platform, 139, 10, 2);
  NUI_CHECK(provider_calls == 2);
  NUI_CHECK(layout.get().widths.at("name") == 80);
  NUI_CHECK(notifications == 2 && callbacks == 1);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

struct CellFault {
  bool armed{};
  int layout_faults{};
};
class FaultCell final : public ui::Component {
public:
  explicit FaultCell(std::shared_ptr<CellFault> fault) : fault_(std::move(fault)) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {20, 20}; }
  ui::Size minimum_size(const std::vector<ui::ChildMetrics>&) const override { return {}; }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    if (fault_->armed) {
      ++fault_->layout_faults;
      throw std::runtime_error("table cell layout fault");
    }
    for (auto& placement : placements) placement.bounds = bounds;
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::shared_ptr<CellFault> fault_;
};
ui::Spec fault_cell(const std::shared_ptr<CellFault>& fault) {
  return {[fault] { return std::make_unique<FaultCell>(fault); },
          {ui::make_spec(ui::Spacer{20, 20})}};
}
void header_uses_published_columns_after_failed_resize() {
  ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1, "one"}}};
  ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
  ui::Selection<int> selection{chosen};
  ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
  auto fault = std::make_shared<CellFault>();
  ui::UI tree{ui::TableView<int>{rows, selection}
                  .columns({{"name1", "One", 0, 1, {}, ui::Align::Start, true, false},
                            {"name2", "Two", 0, 1, {}, ui::Align::Start, true, false}})
                  .sort(sort)
                  .cell([fault](const auto&, const auto&) { return fault_cell(fault); })};
  test::MockPlatform platform;
  tree.resize({200, 120}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{200, 120}, 1};
  settle(tree, renderer);
  fault->armed = true;
  bool caught{};
  try {
    tree.resize({100, 120});
  } catch (const std::runtime_error& error) {
    caught = std::string_view{error.what()} == "table cell layout fault";
  }
  NUI_CHECK(caught && fault->layout_faults >= 1);
  const int previous_faults = fault->layout_faults;
  NUI_CHECK(!sort.get());
  // Leave the cell fault armed: pointer routing must use the last successful
  // geometry and must not retry the failed resize. At width200 x75 is name1.
  click(tree, platform, 75, 10);
  NUI_CHECK(fault->layout_faults == previous_faults);
  NUI_CHECK(sort.get() && sort.get()->column == "name1");
  NUI_CHECK(sort.get()->direction == ui::SortDirection::Ascending);
  fault->armed = false;
  tree.resize({100, 120});
  ui::HeadlessRenderer narrow{{100, 120}, 1};
  settle(tree, narrow);
  // A fresh successful layout makes x75 belong to name2.
  click(tree, platform, 75, 10);
  NUI_CHECK(sort.get() && sort.get()->column == "name2");
  NUI_CHECK(sort.get()->direction == ui::SortDirection::Ascending);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void tiny_grid_height_materializes_every_visible_item() {
  std::vector<ui::CollectionItem<int>> source;
  for (int key = 0; key < 10; ++key) source.push_back({key, "tiny"});
  ui::State<std::vector<ui::CollectionItem<int>>> rows{source};
  ui::State<ui::SelectionSnapshot<int>> chosen{{}};
  ui::Selection<int> selection{chosen};
  std::set<int> built;
  ui::GridViewState<int> state;
  auto spec = ui::make_spec(ui::GridView<int>{rows, selection}.state(state)
                               .minimum_cell_width(80).cell_height(1e-30).gap(0)
                               .cell([&](const auto& item) {
                                 built.insert(item.key);
                                 return ui::make_spec(ui::Spacer{0, 0});
                               }));
  ui::UI tree{ui::Spec{spec}};
  test::MockPlatform platform;
  tree.resize({100, 120}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{100, 120}, 1};
  settle(tree, renderer);
  NUI_CHECK(state.semantic_children().size() == 10);
  for (int key = 0; key < 10; ++key) NUI_CHECK(built.contains(key));
  NUI_CHECK(built.size() == 10 && chosen.get().selected.empty());
  tree.deactivate(platform);
  // Also prove normal sizing works after the tiny-domain scenario.
  built.clear();
  ui::UI normal{ui::GridView<int>{rows, selection}.minimum_cell_width(80).cell_height(12).gap(0)
                    .cell([&](const auto& item) {
                      built.insert(item.key);
                      return ui::make_spec(ui::Spacer{0, 0});
                    })};
  normal.resize({100, 120}); normal.activate(platform);
  settle(normal, renderer);
  for (int key = 0; key < 10; ++key) NUI_CHECK(built.contains(key));
  normal.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
  // Some native conversion instructions saturate an out-of-range float cast;
  // this functional oracle can therefore pass before the fix. UBSan-enabled
  // Core qualification is required to prove the cast itself is defined.
}
void suite() {
  selection_rejects_rows_replaced_at_exposure();
  toggle_preserves_expansion_replaced_at_exposure();
  autofit_preserves_provider_external_layout();
  header_uses_published_columns_after_failed_resize();
  tiny_grid_height_materializes_every_visible_item();
}
} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "rows_replaced") return test::run("collection_rows_replaced", &selection_rejects_rows_replaced_at_exposure);
  if (mode == "expanded_replaced") return test::run("collection_expanded_replaced", &toggle_preserves_expansion_replaced_at_exposure);
  if (mode == "autofit_external") return test::run("collection_autofit_external", &autofit_preserves_provider_external_layout);
  if (mode == "table_rollback") return test::run("collection_table_rollback", &header_uses_published_columns_after_failed_resize);
  if (mode == "tiny_grid") return test::run("collection_tiny_grid", &tiny_grid_height_materializes_every_visible_item);
  if (mode == "all") return test::run("collection_transactions", &suite);
  return 2;
}
