#include "test_support.hpp"
#include "../src/detail/collection_tree_graph.hpp"

#include <stdexcept>

namespace {
void preorder_retains_dataset_sibling_order_and_branch_flags() {
    const auto graph = ui::detail::prepare_collection_tree_graph(
        {std::nullopt, 0, 0, 1, std::nullopt}, {false, false, false, false, true});
    const auto rows = ui::detail::flatten_collection_tree(graph, {true, true, false, false, true});
    NUI_CHECK(rows == std::vector<ui::detail::CollectionTreeRow>({{0, 0, true, true},
                                                                  {1, 1, true, true},
                                                                  {3, 2, false, false},
                                                                  {2, 1, false, false},
                                                                  {4, 0, true, true}}));
    const auto folded =
        ui::detail::flatten_collection_tree(graph, {false, true, false, false, true});
    NUI_CHECK(folded.size() == 2 && folded[0].index == 0 && folded[1].index == 4);
    NUI_CHECK(ui::detail::visible_collection_tree_ancestor(graph, folded, 3) ==
              std::optional<std::size_t>{0});
    NUI_CHECK(!ui::detail::visible_collection_tree_ancestor(graph, folded, 100));
}
void invalid_parent_graphs_fail_before_publication() {
    for (const auto &parents : std::vector<std::vector<std::optional<std::size_t>>>{
             {0}, {1, 0}, {std::nullopt, 2, 1}, {10}}) {
        bool threw = false;
        try {
            (void)ui::detail::prepare_collection_tree_graph(
                parents, std::vector<bool>(parents.size(), false));
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        NUI_CHECK(threw);
    }
    const auto recovered =
        ui::detail::prepare_collection_tree_graph({std::nullopt, 0}, {false, false});
    NUI_CHECK(ui::detail::flatten_collection_tree(recovered, {true, false}).size() == 2);
}
void deep_graphs_traverse_without_recursive_call_stack() {
    constexpr std::size_t count = 50000;
    std::vector<std::optional<std::size_t>> parents(count);
    for (std::size_t index = 1; index < count; ++index)
        parents[index] = index - 1;
    const auto graph = ui::detail::prepare_collection_tree_graph(std::move(parents),
                                                                 std::vector<bool>(count, false));
    const auto rows = ui::detail::flatten_collection_tree(graph, std::vector<bool>(count, true));
    NUI_CHECK(rows.size() == count && rows.back().index == count - 1 &&
              rows.back().depth == count - 1);
    auto expanded = std::vector<bool>(count, true);
    expanded[0] = false;
    const auto folded = ui::detail::flatten_collection_tree(graph, expanded);
    NUI_CHECK(folded.size() == 1 && ui::detail::visible_collection_tree_ancestor(
                                        graph, folded, count - 1) == std::optional<std::size_t>{0});
}
void suite() {
    preorder_retains_dataset_sibling_order_and_branch_flags();
    invalid_parent_graphs_fail_before_publication();
    deep_graphs_traverse_without_recursive_call_stack();
}
} // namespace
int main() { return test::run("collection_tree_graph", &suite); }
