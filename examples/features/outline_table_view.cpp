#include "example_support.hpp"
#include <nativeui/outline_table_view.hpp>

namespace {
int self_test() {
    ui::State<std::vector<ui::TreeNode<int>>> nodes{
        {{1, {}, "Dossier", true, true}, {2, 1, "Fichier"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{{1}, 1, 1}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{}};
    ui::State<ui::TableLayout> layout{{{"detail", "name"}, {}}};
    ui::UI tree{ui::OutlineTableView<int>{nodes, selection, expanded}
                    .columns({{"name", "Nom", 100}, {"detail", "Détails", 100}})
                    .tree_column("name")
                    .layout(layout)};
    example::Platform platform;
    tree.resize({200, 120});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{200, 120}, 1};
    if (!renderer.render(tree))
        return example::fail("outline table render failed");
    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {105, 44};
    tree.dispatch(down, platform);
    down.type = ui::InputType::PointerUp;
    tree.dispatch(down, platform);
    if (expanded.get() != std::vector<int>{1})
        return example::fail("tree column ID moved disclosure incorrectly");
    return renderer.render(tree) ? 0 : example::fail("expanded outline table render failed");
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
    ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1, {}, "Sources", true, true},
                                                     {2, 1, "main.cpp"},
                                                     {3, 1, "widgets.cpp"},
                                                     {4, {}, "Documentation", true, true},
                                                     {5, 4, "widgets.md"}}};
    ui::State<ui::SelectionSnapshot<int>> chosen{{}};
    ui::Selection<int> selection{chosen};
    ui::State<std::vector<int>> expanded{{1}};
    ui::State<ui::TableLayout> layout{{}};
    ui::UI tree{ui::OutlineTableView<int>{nodes, selection, expanded}
                    .columns({{"name", "Nom", 280}, {"id", "ID", 100}})
                    .tree_column("name")
                    .layout(layout)
                    .cell([](const auto &node, const auto &column) {
                        return ui::make_spec(
                            ui::Label{column == "id" ? std::to_string(node.key) : node.label});
                    })};
    return example::run_window(tree, "NativeUI OutlineTableView", {550, 400});
}
