#pragma once

#include <nativeui/detail/list_view_kernel.hpp>
#include <nativeui/virtual_list.hpp>

#include <stdexcept>
#include <utility>

namespace ui {
template <class Key>
class ListView {
public:
    using ActivationCallback = std::function<void(const Key&)>;
    explicit ListView(Binding<std::optional<Key>> selection) : selection_(std::move(selection)) {}
    explicit ListView(State<std::optional<Key>>& selection) : ListView(selection.binding()) {}
    explicit ListView(VirtualListState<Key>& state)
        : selection_(state.selection_binding()) {
        const auto runtime = state.runtime();
        virtual_factory_ = [runtime](ActivationCallback activation,ListViewStyle style) {
            return detail::make_virtual_list_retained_spec(runtime,std::move(style),std::move(activation));
        };
    }
    template <class Child>
    ListView&& item(Key key,Child&& content,bool enabled=true) && {
        if (virtual_factory_) throw std::logic_error("ListView::item is unavailable for a virtualized ListView");
        for (const auto& row : rows_)
            if (row.key == key) throw std::invalid_argument("ListView item keys must be unique");
        rows_.push_back({std::move(key),enabled,make_spec(std::forward<Child>(content))});
        return std::move(*this);
    }
    ListView&& on_activate(ActivationCallback callback) && { activation_ = std::move(callback); return std::move(*this); }
    ListView&& style(ListViewStyle value) && { style_ = std::move(value); return std::move(*this); }
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
