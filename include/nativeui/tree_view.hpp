#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/detail/collection_source_adapter.hpp>

namespace ui {
struct TreeViewStyle {
    ListViewStyle rows;
    double indentation{16.0};
    double chevron_width{16.0};
};
template <class Key> class TreeView {
  public:
    TreeView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key> &selection,
             Binding<std::vector<Key>> expanded)
        : recipe_{std::move(nodes), selection.binding(), std::move(expanded)} {}
    TreeView(State<std::vector<TreeNode<Key>>> &nodes, Selection<Key> &selection,
             State<std::vector<Key>> &expanded)
        : TreeView(nodes.binding(), selection, expanded.binding()) {}
    TreeView &&row(std::function<Spec(const TreeNode<Key> &)> value) && {
        recipe_.row = std::move(value);
        return std::move(*this);
    }
    TreeView &&selection_mode(SelectionMode value) && {
        options_.selection_mode = value;
        return std::move(*this);
    }
    TreeView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
    TreeView &&on_expansion_change(std::function<void(const std::vector<Key> &)> value) && {
        recipe_.expansion_change = std::move(value);
        return std::move(*this);
    }
    TreeView &&style(TreeViewStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }
    [[nodiscard]] Spec spec() && {
        options_.style = style_.rows;
        return detail::make_tree_view(detail::collection_source_factory(std::move(recipe_)),
                                      std::move(options_), style_.indentation,
                                      style_.chevron_width);
    }

  private:
    detail::CollectionSourceRecipe<Key, TreeNode<Key>> recipe_;
    detail::CollectionViewOptions options_;
    TreeViewStyle style_;
};
} // namespace ui
