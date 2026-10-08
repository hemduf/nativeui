#include "test_support.hpp"
#include <nativeui/tree_view.hpp>
#include <nativeui/outline_view.hpp>
#include <nativeui/grid_view.hpp>
#include <nativeui/table_view.hpp>
#include <nativeui/outline_table_view.hpp>

#include <map>
#include <memory>
#include <stdexcept>

namespace {
void render(ui::UI &tree, ui::Size size) {
    ui::HeadlessRenderer renderer{size, 1};
    NUI_CHECK(renderer.render(tree));
}
void key(ui::UI &tree, test::MockPlatform &platform, ui::Key value) {
    tree.dispatch(test::key(value), platform);
}
void click(ui::UI &tree, test::MockPlatform &platform, float x, float y, int clicks = 1) {
    auto down = test::pointer(ui::InputType::PointerDown, x, y);
    down.clicks = clicks;
    tree.dispatch(down, platform);
    auto up = test::pointer(ui::InputType::PointerUp, x, y);
    up.clicks = clicks;
    tree.dispatch(up, platform);
}
void tree_keyboard_graph_validation_and_collapse_selection() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1, {}, "root", true, true},
                                                     {2, 1, "child"},
                                                     {3, {}, "disabled", false},
                                                     {4, {}, "last"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    int activations = 0, expansions = 0;
    ui::UI tree{ui::TreeView<int>{nodes, selection, expanded}
                    .on_activate([&](const int &value) { activations = value; })
                    .on_expansion_change([&](const auto &) { ++expansions; })};
    test::MockPlatform platform;
    tree.resize({200, 120});
    tree.activate(platform);
    render(tree, {200, 120});
    key(tree, platform, ui::Key::Right);
    NUI_CHECK(expanded.get() == std::vector<int>{1} && expansions == 1);
    key(tree, platform, ui::Key::Right);
    NUI_CHECK(chosen.get().active == 2 && chosen.get().selected == std::vector<int>{2});
    key(tree, platform, ui::Key::Enter);
    NUI_CHECK(activations == 2);
    key(tree, platform, ui::Key::Left);
    NUI_CHECK(chosen.get().active == 1);
    key(tree, platform, ui::Key::Down);
    NUI_CHECK(chosen.get().active == 2);
    click(tree, platform, 5, 12);
    NUI_CHECK(expanded.get().empty() && chosen.get().active == 1 &&
              chosen.get().selected == std::vector<int>{1});
    key(tree, platform, ui::Key::Down);
    NUI_CHECK(chosen.get().active == 4);
    nodes.set({{1, 2, "cycle"}, {2, 1, "cycle"}});
    render(tree, {200, 120});
    key(tree, platform, ui::Key::Home);
    NUI_CHECK(chosen.get().active == 1); // Last accepted forest remains usable.
    nodes.set({{9, {}, "recovered"}});
    render(tree, {200, 120});
    key(tree, platform, ui::Key::Home);
    NUI_CHECK(chosen.get().active == 9);
    bool rejected = false;
    try {
        ui::UI bad{ui::TreeView<int>{nodes, selection, expanded}};
        nodes.set({{7, 99, "orphan"}});
        ui::UI invalid{ui::TreeView<int>{nodes, selection, expanded}};
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    NUI_CHECK(rejected &&
              platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void outline_bounds_factories_identity_and_immutable_semantics() {
    std::vector<ui::TreeNode<int>> rows;
    rows.reserve(10000);
    for (int i = 0; i < 10000; ++i)
        rows.push_back({i, {}, "row"});
    ui::State<std::vector<ui::TreeNode<int>>> nodes{rows};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{0}, 0, 0}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    ui::OutlineState<int> state;
    int factories = 0;
    auto spec = ui::make_spec(ui::OutlineView<int>{nodes, selection, expanded}
                                  .state(state)
                                  .row_heights({24, true})
                                  .row([&](const auto &) {
                                      ++factories;
                                      return ui::make_spec(ui::Spacer{80, 20});
                                  }));
    ui::UI tree{ui::Spec{spec}};
    test::MockPlatform platform;
    tree.resize({100, 72});
    tree.activate(platform);
    render(tree, {100, 72});
    NUI_CHECK(factories < 40 && state.visible_rows().size() == 10000);
    const auto first = state.semantic_children();
    const auto metadata = first.metadata_snapshot();
    const auto count = factories;
    NUI_CHECK(first.size() == 10000 && first.item_at(9999)->info.name == "row" &&
              factories == count);
    NUI_CHECK(state.scroll_to_key(5000, ui::ScrollAlignment::Start));
    render(tree, {100, 72});
    const auto second = state.semantic_children();
    NUI_CHECK(second.metadata_snapshot().get() == metadata.get());
    NUI_CHECK_NEAR(second.item_at(5000)->logical_bounds.y, 0, 0.01f);
    NUI_CHECK_NEAR(first.item_at(5000)->logical_bounds.y, 120000, 0.01f);
    NUI_CHECK(factories < count + 30);
    const auto old_token = first.item_at(5000)->token;
    std::swap(rows[5000], rows[5001]);
    nodes.set(rows);
    render(tree, {100, 72});
    NUI_CHECK(state.semantic_children().item_at(5001)->token == old_token);
    NUI_CHECK(!state.scroll_to_key(-1) && state.depth(5000) == std::optional<std::size_t>{0});
}
void grid_reflow_navigation_gap_and_two_ui_contacts() {
    ui::State<std::vector<ui::CollectionItem<int>>> items{
        {{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::GridViewState<int> state;
    int activated = 0;
    auto spec =
        ui::make_spec(ui::GridView<int>{items, selection}
                          .state(state)
                          .minimum_cell_width(60)
                          .cell_height(40)
                          .gap(10)
                          .cell([](const auto &) { return ui::make_spec(ui::Spacer{1, 1}); })
                          .on_activate([&](const int &value) { activated = value; }));
    ui::UI first{ui::Spec{spec}}, second{ui::Spec{spec}};
    test::MockPlatform a, b;
    first.resize({130, 100});
    second.resize({200, 100});
    first.activate(a);
    second.activate(b);
    render(first, {130, 100});
    render(second, {200, 100});
    const auto geometry = state.semantic_children();
    NUI_CHECK_NEAR(geometry.item_at(1)->logical_bounds.x, 70, 0.01f);
    NUI_CHECK_NEAR(geometry.item_at(1)->logical_bounds.w, 60, 0.01f);
    key(first, a, ui::Key::Down);
    NUI_CHECK(chosen.get().active == 3);
    key(first, a, ui::Key::Right);
    NUI_CHECK(chosen.get().active == 4);
    first.dispatch(test::pointer(ui::InputType::PointerDown, 5, 5), a);
    second.dispatch(test::pointer(ui::InputType::PointerUp, 5, 5), b);
    NUI_CHECK(chosen.get().active == 4 && activated == 0);
    first.dispatch(test::pointer(ui::InputType::PointerUp, 5, 5), a);
    NUI_CHECK(chosen.get().active == 1);
    click(first, a, 65, 5, 2);
    NUI_CHECK(activated == 0); // Ten-DIP gap is inert.
    click(first, a, 5, 5, 2);
    NUI_CHECK(activated == 1);
    render(first, {60, 100});
    NUI_CHECK_NEAR(state.semantic_children().item_at(1)->logical_bounds.x, 0, 0.01f);
    NUI_CHECK_NEAR(geometry.item_at(1)->logical_bounds.x, 70, 0.01f);
    NUI_CHECK(a.pointer_capture_begin_count == a.pointer_capture_end_count &&
              b.pointer_capture_begin_count == b.pointer_capture_end_count);
}
void table_resize_sort_cancel_external_update_and_empty_models() {
    ui::State<std::vector<ui::CollectionItem<int>>> items{{{1, "Ada"}, {2, "Lin"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<ui::TableLayout> layout{{}};
    ui::State<std::optional<ui::SortOrder>> sort{std::nullopt};
    int layouts = 0, sorts = 0;
    ui::UI tree{ui::TableView<int>{items, selection}
                    .columns({{"name", "Name", 100, 60, 160, ui::Align::Start, true, false},
                              {"fixed", "Fixed", 80, 60, {}, ui::Align::Start, false, true}})
                    .layout(layout)
                    .sort(sort)
                    .on_layout_change([&](const auto &) { ++layouts; })
                    .on_sort_request([&](const auto &) { ++sorts; })};
    test::MockPlatform platform;
    tree.resize({180, 120});
    tree.activate(platform);
    render(tree, {180, 120});
    click(tree, platform, 20, 10);
    NUI_CHECK((sort.get() == std::optional<ui::SortOrder>{ui::SortOrder{
                                 "name", ui::SortDirection::Ascending}} &&
               sorts == 1));
    click(tree, platform, 20, 10);
    NUI_CHECK(sort.get()->direction == ui::SortDirection::Descending && sorts == 2);
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 99, 10), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 140, 10), platform);
    NUI_CHECK(layout.get().widths.empty());
    key(tree, platform, ui::Key::Escape);
    NUI_CHECK(layouts == 0 && layout.get().widths.empty());
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 99, 10), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 140, 10), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 140, 10), platform);
    NUI_CHECK(layout.get().widths.at("name") == 141 && layouts == 1 && sorts == 2);
    render(tree, {180, 120});
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 140, 10), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10), platform);
    layout.set({{"fixed", "name"}, {{"name", 70}}});
    render(tree, {180, 120});
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10), platform);
    NUI_CHECK(layout.get().widths.at("name") == 70 && layouts == 1);
    int cells = 0;
    ui::UI empty{
        ui::TableView<int>{items, selection}.columns({}).cell([&](const auto &, const auto &) {
            ++cells;
            return ui::make_spec(ui::Spacer{0});
        })};
    render(empty, {180, 100});
    NUI_CHECK(cells == 0);
    bool invalid = false;
    try {
        (void)ui::make_spec(
            ui::TableView<int>{items, selection}.columns({{"x", "X"}, {"x", "duplicate"}}));
    } catch (const std::invalid_argument &) {
        invalid = true;
    }
    NUI_CHECK(invalid);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void outline_table_disclosure_tracks_column_id_after_reorder() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1, {}, "root", true, true}, {2, 1, "child"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    ui::State<ui::TableLayout> layout{{{"detail", "name"}, {}}};
    ui::UI tree{ui::OutlineTableView<int>{nodes, selection, expanded}
                    .columns({{"name", "Name", 100}, {"detail", "Detail", 100}})
                    .tree_column("name")
                    .layout(layout)};
    test::MockPlatform platform;
    tree.resize({200, 120});
    tree.activate(platform);
    render(tree, {200, 120});
    click(tree, platform, 105, 44);
    NUI_CHECK(expanded.get() == std::vector<int>{1});
    click(tree, platform, 5, 44);
    NUI_CHECK(expanded.get() == std::vector<int>{1});
    layout.set({{"name", "detail"}, {}});
    render(tree, {200, 120});
    click(tree, platform, 5, 44);
    NUI_CHECK(expanded.get().empty());
}
void availability_semantics_and_expired_selection_preserve_expansion_domain() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1, {}, "root", true, true}, {2, 1, "child"}}};
    auto chosen = std::make_unique<ui::State<ui::SelectionSnapshot<int>>>(
        ui::SelectionSnapshot<int>{{1}, 1, 1});
    ui::Selection<int> selection{chosen->binding()};
    ui::State<std::vector<int>> expanded{{}};
    ui::State<bool> read_only{false};
    ui::OutlineState<int> state;
    ui::UI tree{
        ui::ReadOnly{read_only, ui::OutlineView<int>{nodes, selection, expanded}.state(state)}};
    test::MockPlatform platform;
    tree.resize({150, 100});
    tree.activate(platform);
    render(tree, {150, 100});
    const auto old = state.semantic_children();
    NUI_CHECK(!old.item_at(0)->info.read_only);
    read_only.set(true);
    render(tree, {150, 100});
    const auto blocked = state.semantic_children();
    NUI_CHECK(blocked.item_at(0)->info.read_only && !old.item_at(0)->info.read_only);
    key(tree, platform, ui::Key::Right);
    NUI_CHECK(expanded.get().empty());
    read_only.set(false);
    chosen.reset();
    render(tree, {150, 100});
    const auto expired = state.semantic_children();
    const auto expired_item = expired.item_at(0);
    const auto &actions = expired_item->info.actions;
    NUI_CHECK(std::find(actions.begin(), actions.end(), ui::SemanticAction::Select) ==
              actions.end());
    NUI_CHECK(std::find(actions.begin(), actions.end(), ui::SemanticAction::Expand) !=
              actions.end());
    key(tree, platform, ui::Key::Right);
    NUI_CHECK(expanded.get() == std::vector<int>{1});
    NUI_CHECK(selection.snapshot().selected == std::vector<int>{1});
}
void equal_presentation_hover_press_and_release_are_noops() {
    ui::State<std::vector<ui::CollectionItem<int>>> items{{{1, "one"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::GridViewStyle style;
    style.cells.base.row_fill = ui::Color{1, 0, 0, 1};
    style.cells.hovered.row_fill = ui::Color{1, 0, 0, 1};
    style.cells.pressed.row_fill = ui::Color{1, 0, 0, 1};
    style.cells.selected.row_fill = ui::Color{1, 0, 0, 1};
    ui::UI tree{ui::GridView<int>{items, selection}.cell_height(40).style(style)};
    test::MockPlatform platform;
    tree.resize({160, 80});
    tree.activate(platform);
    key(tree, platform, ui::Key::Tab);
    render(tree, {160, 80});
    render(tree, {160, 80});
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 20, 16), platform);
    NUI_CHECK(!tree.dirty());
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20, 16), platform);
    NUI_CHECK(!tree.dirty());
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20, 16), platform);
    NUI_CHECK(!tree.dirty());
    tree.dispatch(test::pointer(ui::InputType::PointerLeave, 200, 16), platform);
    NUI_CHECK(!tree.dirty());
}
void suite() {
    equal_presentation_hover_press_and_release_are_noops();
    availability_semantics_and_expired_selection_preserve_expansion_domain();
    tree_keyboard_graph_validation_and_collapse_selection();
    outline_bounds_factories_identity_and_immutable_semantics();
    grid_reflow_navigation_gap_and_two_ui_contacts();
    table_resize_sort_cancel_external_update_and_empty_models();
    outline_table_disclosure_tracks_column_id_after_reorder();
}
} // namespace
int main() { return test::run("widget_collection_views", &suite); }
