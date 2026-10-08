#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/detail/collection_source_adapter.hpp>

namespace ui {
/// Typed cell-style override for GridView. Dimensions are logical UI units.
struct GridViewStyle {
    ListViewStyle cells;
};
/// Optional external per-view collection controller handle; not a process-global registry.
template <class Key> class GridViewState : public detail::CollectionControllerHandle<Key> {};
/// Keyed grid composition recipe over CollectionItem<Key> and Selection<Key>.
/// It retains State bindings, not the source-owner objects. Run on the UI thread,
/// and keep the owning States valid while the mounted view consumes their data.
template <class Key> class GridView {
  public:
/// Borrow the selection controller to obtain its Binding; own the item Binding handle.
    GridView(Binding<std::vector<CollectionItem<Key>>> items, Selection<Key> &selection)
        : recipe_{std::move(items), selection.binding(), {}} {}
    GridView(State<std::vector<CollectionItem<Key>>> &items, Selection<Key> &selection)
        : GridView(items.binding(), selection) {}
/// Use an external GridViewState controller for imperative integration.
    GridView &&state(GridViewState<Key> &value) && {
        options_.controller = value.controller();
        return std::move(*this);
    }
/// Minimum cell extent in logical pixels; consumed during grid layout.
    GridView &&minimum_cell_width(double value) && {
        minimum_width_ = value;
        return std::move(*this);
    }
/// Preferred cell height in logical pixels.
    GridView &&cell_height(double value) && {
        height_ = value;
        return std::move(*this);
    }
/// Gap between cells in logical pixels.
    GridView &&gap(double value) && {
        gap_ = value;
        return std::move(*this);
    }
/// Provide a row/cell Spec factory; the item reference is borrowed for the callback.
    GridView &&cell(std::function<Spec(const CollectionItem<Key> &)> value) && {
        recipe_.row = std::move(value);
        return std::move(*this);
    }
/// Choose view interaction policy; Selection::set does not itself enforce this mode.
    GridView &&selection_mode(SelectionMode value) && {
        options_.selection_mode = value;
        return std::move(*this);
    }
/// Register key activation notification. Callback may throw; never run on the audio thread.
    GridView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
/// Install a reorder notification with current key order and an optional before-key.
/// Providing a callback enables the view's reorder handling; it does not mutate the input State here.
    GridView &&
    on_reorder(std::function<void(const std::vector<Key> &, std::optional<Key>)> value) && {
        reorder_ = static_cast<bool>(value);
        recipe_.reorder = std::move(value);
        return std::move(*this);
    }
/// Apply a local GridViewStyle; uses its cell recipe for retained rendering.
    GridView &&style(GridViewStyle value) && {
        options_.style = std::move(value.cells);
        return std::move(*this);
    }
/// Consume this builder and its factories/Bindings into a retained Spec.
    [[nodiscard]] Spec spec() && {
        return detail::make_grid_view(detail::collection_source_factory(std::move(recipe_)),
                                      std::move(options_), minimum_width_, height_, gap_, reorder_);
    }

  private:
    detail::CollectionSourceRecipe<Key, CollectionItem<Key>> recipe_;
    detail::CollectionViewOptions options_;
    double minimum_width_{140.0}, height_{120.0}, gap_{8.0};
    bool reorder_{};
};
} // namespace ui
