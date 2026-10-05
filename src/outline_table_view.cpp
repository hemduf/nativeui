#include <nativeui/outline_table_view.hpp>

#include "detail/table_view_kernel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui::detail {
namespace {
class OutlineTableLayout final : public CollectionLayoutPolicy {
  public:
    [[nodiscard]] CollectionLayoutSnapshot prepare(CollectionHeightSnapshot heights,
                                                   std::size_t count, Rect viewport, Point offset,
                                                   bool) const override {
        if (count != heights.size())
            throw std::logic_error("OutlineTableView visible geometry mismatch");
        const double total = heights.total_height();
        if (!std::isfinite(total))
            throw std::overflow_error("OutlineTableView height overflow");
        CollectionLayoutSnapshot result;
        result.content = {viewport.w,
                          static_cast<float>(std::min(
                              total, static_cast<double>(std::numeric_limits<float>::max())))};
        const auto range = heights.range(offset.y, viewport.h, 2);
        result.window.reserve(range.last - range.first);
        for (std::size_t index = range.first; index < range.last; ++index)
            result.window.push_back(index);
        result.geometry = std::make_shared<const CollectionVerticalGeometry>(std::move(heights),
                                                                             viewport, offset);
        return result;
    }
};
} // namespace
Spec make_outline_table_view(CollectionSourceFactory source, TableViewOptions options,
                             ColumnId tree_column, double indentation, double chevron_width) {
    validate_table_columns(options.columns);
    if (tree_column.empty() && !options.columns.empty())
        tree_column = options.columns.front().id;
    if (!tree_column.empty() &&
        std::none_of(options.columns.begin(), options.columns.end(),
                     [&](const auto &column) { return column.id == tree_column; }))
        throw std::invalid_argument("OutlineTableView tree_column is not declared");
    if (!std::isfinite(indentation) || indentation < 0 || !std::isfinite(chevron_width) ||
        chevron_width < 0)
        throw std::invalid_argument("OutlineTableView indentation must be finite and nonnegative");
    return make_retained_table(
        std::move(source), std::move(options),
        CollectionHierarchyOptions{true, true, false, indentation, chevron_width},
        std::move(tree_column), std::make_shared<OutlineTableLayout>());
}
} // namespace ui::detail
