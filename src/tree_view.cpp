#include <nativeui/tree_view.hpp>
#include "detail/collection_tree_graph.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <cmath>
#include <numeric>

namespace ui::detail {
namespace {
class TreeLayout final : public CollectionLayoutPolicy {
  public:
    [[nodiscard]] CollectionLayoutSnapshot prepare(CollectionHeightSnapshot heights,
                                                   std::size_t count, Rect viewport, Point offset,
                                                   bool) const override {
        if (count != heights.size())
            throw std::logic_error("TreeView height/row mismatch");
        const auto content_height = heights.total_height();
        if (!std::isfinite(content_height))
            throw std::overflow_error("TreeView extent overflow");
        CollectionLayoutSnapshot result;
        result.content = {viewport.w, static_cast<float>(std::min(
                                          content_height,
                                          static_cast<double>(std::numeric_limits<float>::max())))};
        result.geometry = std::make_shared<const CollectionVerticalGeometry>(std::move(heights),
                                                                             viewport, offset);
        result.window.resize(count);
        std::iota(result.window.begin(), result.window.end(), 0);
        return result;
    }
};
} // namespace
std::shared_ptr<CollectionLayoutPolicy> make_tree_view_layout() {
    return std::make_shared<TreeLayout>();
}
Spec make_tree_view(CollectionSourceFactory source, CollectionViewOptions options,
                    double indentation, double chevron_width) {
    return make_collection_view(
        std::move(source), std::move(options),
        CollectionHierarchyOptions{true, false, false, indentation, chevron_width},
        make_tree_view_layout());
}

CollectionTreeGraph prepare_collection_tree_graph(std::vector<std::optional<std::size_t>> parents,
                                                  std::vector<bool> branches) {
    if (parents.size() != branches.size())
        throw CollectionModelError("Tree branch/parent count mismatch");
    const auto count = parents.size();
    CollectionTreeGraph result{
        std::move(parents), std::vector<std::vector<std::size_t>>(count), {}, std::move(branches)};
    result.roots.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (!result.parents[index])
            result.roots.push_back(index);
        else {
            const auto parent = *result.parents[index];
            if (parent >= count)
                throw CollectionModelError("Tree parent is absent");
            if (parent == index)
                throw CollectionModelError("Tree node cannot parent itself");
            result.children[parent].push_back(index);
            result.branches[parent] = true;
        }
    }
    // Each parent chain is visited once. A grey node in this chain is a cycle;
    // black nodes are already checked, including disconnected rootless cycles.
    std::vector<unsigned char> colors(count, 0);
    std::vector<std::size_t> chain;
    for (std::size_t index = 0; index < count; ++index) {
        if (colors[index] != 0)
            continue;
        chain.clear();
        auto current = std::optional<std::size_t>{index};
        while (current && colors[*current] == 0) {
            colors[*current] = 1;
            chain.push_back(*current);
            current = result.parents[*current];
        }
        if (current && colors[*current] == 1)
            throw CollectionModelError("Tree parent graph contains a cycle");
        for (const auto visited : chain)
            colors[visited] = 2;
    }
    return result;
}
std::vector<CollectionTreeRow> flatten_collection_tree(const CollectionTreeGraph &graph,
                                                       const std::vector<bool> &expanded) {
    if (expanded.size() != graph.parents.size())
        throw CollectionModelError("Tree expansion count mismatch");
    struct Pending {
        std::size_t index, depth;
    };
    std::vector<Pending> pending;
    pending.reserve(graph.roots.size());
    for (auto root = graph.roots.rbegin(); root != graph.roots.rend(); ++root)
        pending.push_back({*root, 0});
    std::vector<CollectionTreeRow> result;
    result.reserve(std::min<std::size_t>(graph.parents.size(), 64));
    while (!pending.empty()) {
        const auto row = pending.back();
        pending.pop_back();
        result.push_back({row.index, row.depth, graph.branches[row.index],
                          graph.branches[row.index] && expanded[row.index]});
        if (!graph.branches[row.index] || !expanded[row.index])
            continue;
        if (row.depth == std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("Tree depth is not representable");
        const auto &children = graph.children[row.index];
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            pending.push_back({*child, row.depth + 1});
    }
    return result;
}
std::optional<std::size_t>
visible_collection_tree_ancestor(const CollectionTreeGraph &graph,
                                 const std::vector<CollectionTreeRow> &visible, std::size_t index) {
    if (index >= graph.parents.size())
        return {};
    std::unordered_set<std::size_t> included;
    included.reserve(visible.size());
    for (const auto &row : visible)
        if (row.index < graph.parents.size())
            included.insert(row.index);
    auto current = std::optional<std::size_t>{index};
    while (current) {
        if (included.contains(*current))
            return current;
        current = graph.parents[*current];
    }
    return {};
}
} // namespace ui::detail
