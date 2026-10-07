#pragma once

#include <nativeui/detail/tabs_kernel.hpp>

#include <stdexcept>
#include <utility>

namespace ui {
template <class T>
class Tabs {
public:
    explicit Tabs(Binding<T> selection) : selection_(std::move(selection)) {}
    explicit Tabs(State<T>& selection) : Tabs(selection.binding()) {}
    template <class Child>
    Tabs&& tab(T key,std::string label,Child&& panel,bool enabled=true) && {
        for (const auto& tab : tabs_)
            if (tab.key == key) throw std::invalid_argument("Tabs keys must be unique");
        tabs_.push_back({std::move(key),std::move(label),enabled,make_spec(std::forward<Child>(panel))});
        return std::move(*this);
    }
    Tabs&& style(TabsStyle value) && { style_ = std::move(value); return std::move(*this); }
    Spec spec() && {
        auto recipe = std::make_shared<detail::TabsRecipe>();
        auto keys = std::make_shared<std::vector<T>>();
        recipe->style = std::move(style_);
        keys->reserve(tabs_.size()); recipe->labels.reserve(tabs_.size());
        recipe->enabled.reserve(tabs_.size()); recipe->panels.reserve(tabs_.size());
        for (auto& tab : tabs_) {
            keys->push_back(std::move(tab.key));
            recipe->labels.push_back(std::move(tab.label)); recipe->enabled.push_back(tab.enabled);
            recipe->panels.push_back(std::move(tab.panel));
        }
        const auto source = selection_;
        return detail::make_tabs_spec(std::move(recipe),[source,keys] {
            return std::make_unique<detail::TypedTabsSelection<T>>(source,keys);
        });
    }
private:
    struct Tab { T key; std::string label; bool enabled{}; Spec panel; };
    Binding<T> selection_;
    std::vector<Tab> tabs_;
    TabsStyle style_;
};
} // namespace ui
