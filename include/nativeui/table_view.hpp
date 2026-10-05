#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/detail/collection_source_adapter.hpp>
#include <nativeui/detail/layout_types.hpp>

#include <map>

namespace ui {
using ColumnId = std::string;
enum class SortDirection { Ascending, Descending };
struct SortOrder {
    ColumnId column;
    SortDirection direction{SortDirection::Ascending};
    bool operator==(const SortOrder &) const = default;
};
struct TableColumn {
    ColumnId id;
    std::string title;
    double width{};
    double minimum_width{60.0};
    std::optional<double> maximum_width{};
    Align align{Align::Start};
    bool sortable{}, fixed{};
    bool operator==(const TableColumn &) const = default;
};
struct TableLayout {
    std::vector<ColumnId> order;
    std::map<ColumnId, double> widths;
    bool operator==(const TableLayout &) const = default;
};
struct TableViewStyle {
    ListViewStyle rows;
    std::optional<Color> header_fill, header_text, separator, guide;
    double header_height{32.0};
};
namespace detail {
struct TableViewOptions {
    CollectionViewOptions rows;
    std::vector<TableColumn> columns;
    std::optional<Binding<TableLayout>> layout;
    std::optional<Binding<std::optional<SortOrder>>> sort;
    std::function<double(const ColumnId &)> autofit_width;
    std::function<void(const SortOrder &)> sort_request;
    std::function<void(const TableLayout &)> layout_change;
    TableViewStyle style;
};
[[nodiscard]] Spec make_table_view(CollectionSourceFactory source, TableViewOptions options);
[[nodiscard]] Spec make_outline_table_view(CollectionSourceFactory source, TableViewOptions options,
                                           ColumnId tree_column, double indentation,
                                           double chevron_width);
} // namespace detail
template <class Key> class TableView {
  public:
    TableView(Binding<std::vector<CollectionItem<Key>>> rows, Selection<Key> &selection)
        : recipe_{std::move(rows), selection.binding(), {}} {}
    TableView(State<std::vector<CollectionItem<Key>>> &rows, Selection<Key> &selection)
        : TableView(rows.binding(), selection) {}
    TableView &&columns(std::vector<TableColumn> value) && {
        options_.columns = std::move(value);
        return std::move(*this);
    }
    TableView &&cell(std::function<Spec(const CollectionItem<Key> &, const ColumnId &)> value) && {
        recipe_.cell = std::move(value);
        return std::move(*this);
    }
    TableView &&layout(Binding<TableLayout> value) && {
        options_.layout = std::move(value);
        return std::move(*this);
    }
    TableView &&layout(State<TableLayout> &value) && {
        return std::move(*this).layout(value.binding());
    }
    TableView &&sort(Binding<std::optional<SortOrder>> value) && {
        options_.sort = std::move(value);
        return std::move(*this);
    }
    TableView &&sort(State<std::optional<SortOrder>> &value) && {
        return std::move(*this).sort(value.binding());
    }
    TableView &&row_heights(ListRowHeights value) && {
        options_.rows.row_heights = value;
        return std::move(*this);
    }
    TableView &&selection_mode(SelectionMode value) && {
        options_.rows.selection_mode = value;
        return std::move(*this);
    }
    TableView &&autofit_width(std::function<double(const ColumnId &)> value) && {
        options_.autofit_width = std::move(value);
        return std::move(*this);
    }
    TableView &&on_sort_request(std::function<void(const SortOrder &)> value) && {
        options_.sort_request = std::move(value);
        return std::move(*this);
    }
    TableView &&on_layout_change(std::function<void(const TableLayout &)> value) && {
        options_.layout_change = std::move(value);
        return std::move(*this);
    }
    TableView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
    TableView &&style(TableViewStyle value) && {
        options_.style = std::move(value);
        return std::move(*this);
    }
    [[nodiscard]] Spec spec() && {
        recipe_.table = true;
        options_.rows.style = options_.style.rows;
        return detail::make_table_view(detail::collection_source_factory(std::move(recipe_)),
                                       std::move(options_));
    }

  private:
    detail::CollectionSourceRecipe<Key, CollectionItem<Key>> recipe_;
    detail::TableViewOptions options_;
};
} // namespace ui
