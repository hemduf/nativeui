#pragma once

#include <nativeui/detail/dynamic_host.hpp>

namespace ui {
namespace detail {
template <class T>
struct SwitchBranch {
    T value;
    std::string key;
    std::shared_ptr<const Spec> spec;
};
template <class T>
struct SwitchModel {
    SwitchModel(Binding<T> value,std::vector<SwitchBranch<T>> choices,std::shared_ptr<const Spec> otherwise)
        : source(std::move(value)),branches(std::move(choices)),fallback(std::move(otherwise)) {}
    Binding<T> source;
    std::vector<SwitchBranch<T>> branches;
    std::shared_ptr<const Spec> fallback;
    [[nodiscard]] std::optional<DynamicChildSpec> selected() const {
        // Copy the source so equality callbacks cannot invalidate a borrowed T.
        const auto selected = source.get();
        for (const auto& branch : branches)
            if (branch.value == selected) return DynamicChildSpec{branch.key,*branch.spec};
        if (fallback) return DynamicChildSpec{"switch:fallback",*fallback};
        return {};
    }
};
template <class T>
class SwitchComponent final : public SwitchHostComponent {
public:
    SwitchComponent(Binding<T> source,std::vector<SwitchBranch<T>> branches,std::shared_ptr<const Spec> fallback)
        : SwitchComponent(std::make_shared<SwitchModel<T>>(std::move(source),std::move(branches),std::move(fallback))) {}
private:
    explicit SwitchComponent(std::shared_ptr<SwitchModel<T>> model)
        : SwitchHostComponent([model] { return model->selected(); },dynamic_observe(model->source)) {}
};
} // namespace detail

template <class T>
class Switch {
public:
    explicit Switch(Binding<T> state) : state_(std::move(state)) {}
    explicit Switch(State<T>& state) : Switch(state.binding()) {}
    template <class Child>
    Switch& when(T value,Child&& child) & {
        const auto index = branches_.size();
        branches_.push_back({std::move(value),"switch:"+std::to_string(index),
            std::make_shared<const Spec>(make_spec(std::forward<Child>(child)))});
        return *this;
    }
    template <class Child>
    Switch&& when(T value,Child&& child) && {
        when(std::move(value),std::forward<Child>(child)); return std::move(*this);
    }
    template <class Child>
    Switch& otherwise(Child&& child) & {
        fallback_ = std::make_shared<const Spec>(make_spec(std::forward<Child>(child))); return *this;
    }
    template <class Child>
    Switch&& otherwise(Child&& child) && {
        otherwise(std::forward<Child>(child)); return std::move(*this);
    }
    Spec spec() && {
        auto source = state_;
        auto branches = std::move(branches_);
        auto fallback = std::move(fallback_);
        Spec result{[source,branches=std::move(branches),fallback=std::move(fallback)] {
            return std::make_unique<detail::SwitchComponent<T>>(source,branches,fallback);
        },{}};
        result.children_factory = [](Component& component) {
            return static_cast<detail::SwitchComponent<T>&>(component).prepare_initial_children();
        };
        return result;
    }
private:
    Binding<T> state_;
    std::vector<detail::SwitchBranch<T>> branches_;
    std::shared_ptr<const Spec> fallback_;
};
} // namespace ui
