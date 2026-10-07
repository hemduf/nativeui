#pragma once

#include <nativeui/tree_view.hpp>

namespace ui {
struct OutlineViewStyle {
    ListViewStyle rows;
    double indentation{16.0};
    double chevron_width{16.0};
};
template <class Key> class OutlineState : public detail::CollectionControllerHandle<Key> {};
template <class Key> class OutlineView {
  public:
    OutlineView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key> &selection,
                Binding<std::vector<Key>> expanded)
        : recipe_{std::move(nodes), selection.binding(), std::move(expanded)} {}
    OutlineView(State<std::vector<TreeNode<Key>>> &nodes, Selection<Key> &selection,
                State<std::vector<Key>> &expanded)
        : OutlineView(nodes.binding(), selection, expanded.binding()) {}
    OutlineView &&state(OutlineState<Key> &value) && {
        options_.controller = value.controller();
        return std::move(*this);
    }
    OutlineView &&row(std::function<Spec(const TreeNode<Key> &)> value) && {
        recipe_.row = std::move(value);
        return std::move(*this);
    }
    OutlineView &&row_heights(ListRowHeights value) && {
        options_.row_heights = value;
        return std::move(*this);
    }
    OutlineView &&selection_mode(SelectionMode value) && {
        options_.selection_mode = value;
        return std::move(*this);
    }
    OutlineView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
    OutlineView &&on_expansion_change(std::function<void(const std::vector<Key> &)> value) && {
        recipe_.expansion_change = std::move(value);
        return std::move(*this);
    }
    OutlineView &&style(OutlineViewStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }
    [[nodiscard]] Spec spec() && {
        options_.style = style_.rows;
        return detail::make_outline_view(detail::collection_source_factory(std::move(recipe_)),
                                         std::move(options_), style_.indentation,
                                         style_.chevron_width);
    }

  private:
    detail::CollectionSourceRecipe<Key, TreeNode<Key>> recipe_;
    detail::CollectionViewOptions options_;
    OutlineViewStyle style_;
};
} // namespace ui
