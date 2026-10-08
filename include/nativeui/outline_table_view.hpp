#pragma once

#include <nativeui/table_view.hpp>
#include <nativeui/tree_view.hpp>

namespace ui {
struct OutlineTableViewStyle {
    TableViewStyle table;
    double indentation{16.0}, chevron_width{16.0};
};
template <class Key> class OutlineTableView {
  public:
    OutlineTableView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key> &selection,
                     Binding<std::vector<Key>> expanded)
        : recipe_{std::move(nodes), selection.binding(), std::move(expanded)} {}
    OutlineTableView(State<std::vector<TreeNode<Key>>> &nodes, Selection<Key> &selection,
                     State<std::vector<Key>> &expanded)
        : OutlineTableView(nodes.binding(), selection, expanded.binding()) {}
    OutlineTableView &&columns(std::vector<TableColumn> value) && {
        options_.columns = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&tree_column(ColumnId value) && {
        tree_column_ = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&cell(std::function<Spec(const TreeNode<Key> &, const ColumnId &)> value) && {
        recipe_.cell = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&layout(Binding<TableLayout> value) && {
        options_.layout = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&layout(State<TableLayout> &value) && {
        return std::move(*this).layout(value.binding());
    }
    OutlineTableView &&sort(Binding<std::optional<SortOrder>> value) && {
        options_.sort = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&sort(State<std::optional<SortOrder>> &value) && {
        return std::move(*this).sort(value.binding());
    }
    OutlineTableView &&row_heights(ListRowHeights value) && {
        options_.rows.row_heights = value;
        return std::move(*this);
    }
    OutlineTableView &&selection_mode(SelectionMode value) && {
        options_.rows.selection_mode = value;
        return std::move(*this);
    }
    OutlineTableView &&on_sort_request(std::function<void(const SortOrder &)> value) && {
        options_.sort_request = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&on_layout_change(std::function<void(const TableLayout &)> value) && {
        options_.layout_change = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&on_expansion_change(std::function<void(const std::vector<Key> &)> value) && {
        recipe_.expansion_change = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&autofit_width(std::function<double(const ColumnId &)> value) && {
        options_.autofit_width = std::move(value);
        return std::move(*this);
    }
    OutlineTableView &&style(OutlineTableViewStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }
    [[nodiscard]] Spec spec() && {
        recipe_.table = true;
        options_.style = style_.table;
        options_.rows.style = options_.style.rows;
        return detail::make_outline_table_view(
            detail::collection_source_factory(std::move(recipe_)), std::move(options_),
            std::move(tree_column_), style_.indentation, style_.chevron_width);
    }

  private:
    detail::CollectionSourceRecipe<Key, TreeNode<Key>> recipe_;
    detail::TableViewOptions options_;
    OutlineTableViewStyle style_;
    ColumnId tree_column_;
};
} // namespace ui
