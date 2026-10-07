#include "example_support.hpp"
#include <nativeui/grid_view.hpp>

namespace {
std::vector<ui::CollectionItem<int>> items() {
    std::vector<ui::CollectionItem<int>> result;
    for (int i = 0; i < 200; ++i)
        result.push_back({i, "Photo " + std::to_string(i + 1)});
    return result;
}
int self_test() {
    ui::State<std::vector<ui::CollectionItem<int>>> source{items()};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{0}, 0, 0}};
    ui::Selection<int> selection{chosen};
    ui::GridViewState<int> state;
    ui::UI tree{ui::GridView<int>{source, selection}
                    .state(state)
                    .minimum_cell_width(60)
                    .cell_height(40)
                    .gap(10)};
    example::Platform platform;
    tree.resize({130, 100});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{130, 100}, 1};
    if (!renderer.render(tree))
        return example::fail("grid render failed");
    const auto first = state.semantic_children();
    tree.dispatch(example::key(ui::Key::Down), platform);
    if (chosen.get().active != 2 || first.item_at(1)->logical_bounds.x != 70)
        return example::fail("grid columns/keyboard failed");
    ui::HeadlessRenderer narrow{{60, 100}, 1};
    if (!narrow.render(tree) || state.semantic_children().item_at(1)->logical_bounds.x != 0 ||
        first.item_at(1)->logical_bounds.x != 70)
        return example::fail("grid reflow/old snapshot failed");
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::vector<ui::CollectionItem<int>>> source{items()};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::UI tree{ui::GridView<int>{source, selection}.selection_mode(ui::SelectionMode::Multiple)};
    return example::run_window(tree, "NativeUI GridView", {620, 500});
}
