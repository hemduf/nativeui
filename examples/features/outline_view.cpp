#include "example_support.hpp"
#include <nativeui/outline_view.hpp>

namespace {
std::vector<ui::TreeNode<int>> rows() {
    std::vector<ui::TreeNode<int>> result;
    for (int i = 0; i < 10000; ++i)
        result.push_back({i, {}, "File " + std::to_string(i)});
    return result;
}
int self_test() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{rows()};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    ui::OutlineState<int> state;
    int factories = 0;
    ui::UI tree{
        ui::OutlineView<int>{nodes, selection, expanded}.state(state).row([&](const auto &node) {
            ++factories;
            return ui::make_spec(ui::Label{node.label});
        })};
    ui::HeadlessRenderer renderer{{240, 120}, 1};
    if (!renderer.render(tree))
        return example::fail("outline render failed");
    const auto old = state.semantic_children();
    const auto count = factories;
    if (!state.scroll_to_key(9000, ui::ScrollAlignment::Start) || !renderer.render(tree))
        return example::fail("outline scroll failed");
    const auto current = state.semantic_children();
    if (factories > count + 30 || current.size() != 10000 ||
        old.metadata_snapshot().get() != current.metadata_snapshot().get())
        return example::fail("outline materialization/snapshot isolation failed");
    if (current.item_at(9000)->logical_bounds.y != 0 || old.item_at(9000)->logical_bounds.y <= 0)
        return example::fail("outline geometry snapshot failed");
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::vector<ui::TreeNode<int>>> nodes{rows()};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    ui::UI tree{ui::OutlineView<int>{nodes, selection, expanded}};
    return example::run_window(tree, "NativeUI OutlineView", {420, 500});
}
