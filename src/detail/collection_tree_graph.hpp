#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace ui::detail {
struct CollectionTreeRow {
    std::size_t index{};
    std::size_t depth{};
    bool branch{};
    bool expanded{};
    bool operator==(const CollectionTreeRow &) const = default;
};
struct CollectionTreeGraph {
    std::vector<std::optional<std::size_t>> parents;
    std::vector<std::vector<std::size_t>> children;
    std::vector<std::size_t> roots;
    std::vector<bool> branches;
};
// Typed adapters resolve parent keys to indices against an owned token snapshot.
// These routines then validate and traverse without user equality or recursion.
[[nodiscard]] CollectionTreeGraph
prepare_collection_tree_graph(std::vector<std::optional<std::size_t>> parents,
                              std::vector<bool> declared_branches);
[[nodiscard]] std::vector<CollectionTreeRow>
flatten_collection_tree(const CollectionTreeGraph &graph, const std::vector<bool> &expanded);
[[nodiscard]] std::optional<std::size_t>
visible_collection_tree_ancestor(const CollectionTreeGraph &graph,
                                 const std::vector<CollectionTreeRow> &visible, std::size_t index);
} // namespace ui::detail
