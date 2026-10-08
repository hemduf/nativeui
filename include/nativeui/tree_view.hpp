#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/detail/collection_source_adapter.hpp>

namespace ui {
/// Typed row styling and logical indentation/chevron geometry for TreeView.
struct TreeViewStyle {
    ListViewStyle rows;
    double indentation{16.0};
    double chevron_width{16.0};
};
/// Keyed hierarchical collection built from TreeNode<Key>, a Selection<Key> controller,
/// and a separate expanded-key State/Binding. UI-thread use and owner lifetime follow State.
/// Parent-key references are model data; the detached struct itself cannot validate a tree.
template <class Key> class TreeView {
  public:
/// Keep Binding handles to nodes, selection and expansion state; no State owner is transferred.
    TreeView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key> &selection,
             Binding<std::vector<Key>> expanded)
        : recipe_{std::move(nodes), selection.binding(), std::move(expanded)} {}
    TreeView(State<std::vector<TreeNode<Key>>> &nodes, Selection<Key> &selection,
             State<std::vector<Key>> &expanded)
        : TreeView(nodes.binding(), selection, expanded.binding()) {}
/// Set the per-row Spec factory; the passed TreeNode reference is callback-scoped.
    TreeView &&row(std::function<Spec(const TreeNode<Key> &)> value) && {
        recipe_.row = std::move(value);
        return std::move(*this);
    }
/// Choose Single or Multiple selection in the view's interaction policy.
    TreeView &&selection_mode(SelectionMode value) && {
        options_.selection_mode = value;
        return std::move(*this);
    }
/// Register activation callback receiving one Key.
    TreeView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
/// Register expansion-change callback with the updated expanded-key vector.
    TreeView &&on_expansion_change(std::function<void(const std::vector<Key> &)> value) && {
        recipe_.expansion_change = std::move(value);
        return std::move(*this);
    }
/// Set row style and indentation/chevron geometry for this builder.
    TreeView &&style(TreeViewStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }
/// Consume the bindings, callbacks and style into a retained Spec.
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
