#pragma once
#include <nativeui/table_view.hpp>

namespace ui::detail {
struct ResolvedTableColumn {
    std::size_t declared_index{};
    double x{}, width{};
    bool operator==(const ResolvedTableColumn &) const = default;
};
struct ResolvedTableGeometry {
    std::vector<ResolvedTableColumn> columns;
    double width{};
};
void validate_table_columns(const std::vector<TableColumn> &columns);
[[nodiscard]] ResolvedTableGeometry resolve_table_columns(const std::vector<TableColumn> &columns,
                                                          const TableLayout &layout,
                                                          double available_width);
[[nodiscard]] Spec make_retained_table(CollectionSourceFactory source, TableViewOptions options,
                                       CollectionHierarchyOptions hierarchy, ColumnId tree_column,
                                       std::shared_ptr<CollectionLayoutPolicy> body_layout);
} // namespace ui::detail
