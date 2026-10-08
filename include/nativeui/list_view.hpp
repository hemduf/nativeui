#pragma once

#include <nativeui/detail/list_view_kernel.hpp>
#include <nativeui/virtual_list.hpp>

#include <stdexcept>
#include <utility>

namespace ui {
/// Retained ListView recipe with one optional selected Key.
/// Construct from State/Binding for explicit rows, or VirtualListState for the fixed-height
/// virtualized path. It captures per-instance state; build and mutate on the UI thread.
/// The backing selection State and virtualized runtime must remain valid for the view lifetime.
template <class Key>
class ListView {
public:
    using ActivationCallback = std::function<void(const Key&)>;
/// Construct from a reference-like selection Binding. It does not own the State.
    explicit ListView(Binding<std::optional<Key>> selection) : selection_(std::move(selection)) {}
/// Convenience form borrowing a live State; equivalent to its binding().
    explicit ListView(State<std::optional<Key>>& selection) : ListView(selection.binding()) {}
/// Construct a virtualized view sharing the controller's retained runtime.
/// Rows come from VirtualListState::replace, not item().
    explicit ListView(VirtualListState<Key>& state)
        : selection_(state.selection_binding()) {
        const auto runtime = state.runtime();
        virtual_factory_ = [runtime](ActivationCallback activation,ListViewStyle style) {
            return detail::make_virtual_list_retained_spec(runtime,std::move(style),std::move(activation));
        };
    }
/// Add a keyed explicit row; keys must be unique within this recipe.
/// Throws invalid_argument for duplicate keys and logic_error in virtualized mode.
/// May allocate and consume the provided child specification.
    template <class Child>
    ListView&& item(Key key,Child&& content,bool enabled=true) && {
        if (virtual_factory_) throw std::logic_error("ListView::item is unavailable for a virtualized ListView");
        for (const auto& row : rows_)
            if (row.key == key) throw std::invalid_argument("ListView item keys must be unique");
        rows_.push_back({std::move(key),enabled,make_spec(std::forward<Child>(content))});
        return std::move(*this);
    }
/// Register a per-view activation callback; callback executes in the retained UI domain.
    ListView&& on_activate(ActivationCallback callback) && { activation_ = std::move(callback); return std::move(*this); }
/// Set typed ListViewStyle for this builder.
    ListView&& style(ListViewStyle value) && { style_ = std::move(value); return std::move(*this); }
/// Consume the recipe into an owned Spec; do not reuse the moved-from builder.
/// A virtualized Spec retains its runtime separately from the temporary builder.
    Spec spec() && {
        if (virtual_factory_) return virtual_factory_(std::move(activation_),std::move(style_));
        auto recipe = std::make_shared<detail::ListRecipe>();
        auto keys = std::make_shared<std::vector<Key>>();
        recipe->style = std::move(style_);
        keys->reserve(rows_.size()); recipe->enabled.reserve(rows_.size()); recipe->rows.reserve(rows_.size());
        for (auto& row : rows_) {
            keys->push_back(std::move(row.key)); recipe->enabled.push_back(row.enabled); recipe->rows.push_back(std::move(row.content));
        }
        const auto source = selection_; const auto activation = std::move(activation_);
        return detail::make_list_view_spec(std::move(recipe),[source,keys,activation] {
            return std::make_unique<detail::TypedListSelection<Key>>(source,keys,activation);
        });
    }
private:
    struct Row { Key key; bool enabled{}; Spec content; };
    Binding<std::optional<Key>> selection_;
    std::function<Spec(ActivationCallback,ListViewStyle)> virtual_factory_;
    std::vector<Row> rows_;
    ActivationCallback activation_;
    ListViewStyle style_;
};
} // namespace ui
