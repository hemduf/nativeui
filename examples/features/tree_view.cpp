#include "example_support.hpp"
#include <nativeui/tree_view.hpp>

namespace {
int self_test() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{
        {{1, {}, "Sources", true, true}, {2, 1, "main.cpp"}, {3, {}, "README.md"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    int activations = 0;
    ui::UI tree{ui::TreeView<int>{nodes, selection, expanded}.on_activate(
        [&](const int &value) { activations = value; })};
    example::Platform platform;
    tree.resize({240, 160});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Right), platform);
    tree.dispatch(example::key(ui::Key::Right), platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (expanded.get() != std::vector<int>{1} || chosen.get().active != 2 || activations != 2)
        return example::fail("tree disclosure/child navigation failed");
    ui::HeadlessRenderer renderer{{240, 160}, 1};
    return renderer.render(tree) ? 0 : example::fail("tree render failed");
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1, {}, "Projet", true, true},
                                                     {2, 1, "Sources", true, true},
                                                     {3, 2, "main.cpp"},
                                                     {4, 2, "widgets.cpp"},
                                                     {5, 1, "Documentation", true, true},
                                                     {6, 5, "widgets.md"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{1}};
    ui::UI tree{ui::TreeView<int>{nodes, selection, expanded}};
    return example::run_window(tree, "NativeUI TreeView", {400, 400});
}
