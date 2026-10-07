#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/detail/collection_source_adapter.hpp>

namespace ui {
struct GridViewStyle {
    ListViewStyle cells;
};
template <class Key> class GridViewState : public detail::CollectionControllerHandle<Key> {};
template <class Key> class GridView {
  public:
    GridView(Binding<std::vector<CollectionItem<Key>>> items, Selection<Key> &selection)
        : recipe_{std::move(items), selection.binding(), {}} {}
    GridView(State<std::vector<CollectionItem<Key>>> &items, Selection<Key> &selection)
        : GridView(items.binding(), selection) {}
    GridView &&state(GridViewState<Key> &value) && {
        options_.controller = value.controller();
        return std::move(*this);
    }
    GridView &&minimum_cell_width(double value) && {
        minimum_width_ = value;
        return std::move(*this);
    }
    GridView &&cell_height(double value) && {
        height_ = value;
        return std::move(*this);
    }
    GridView &&gap(double value) && {
        gap_ = value;
        return std::move(*this);
    }
    GridView &&cell(std::function<Spec(const CollectionItem<Key> &)> value) && {
        recipe_.row = std::move(value);
        return std::move(*this);
    }
    GridView &&selection_mode(SelectionMode value) && {
        options_.selection_mode = value;
        return std::move(*this);
    }
    GridView &&on_activate(std::function<void(const Key &)> value) && {
        recipe_.activation = std::move(value);
        return std::move(*this);
    }
    GridView &&
    on_reorder(std::function<void(const std::vector<Key> &, std::optional<Key>)> value) && {
        reorder_ = static_cast<bool>(value);
        recipe_.reorder = std::move(value);
        return std::move(*this);
    }
    GridView &&style(GridViewStyle value) && {
        options_.style = std::move(value.cells);
        return std::move(*this);
    }
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
