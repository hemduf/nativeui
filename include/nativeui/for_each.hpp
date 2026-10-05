#pragma once

#include <nativeui/detail/dynamic_host.hpp>
#include <nativeui/detail/dynamic_key.hpp>

namespace ui {
namespace detail {
template <class T>
struct ForEachModel {
    using Items = std::vector<T>;
    using KeyFunction = std::function<std::string(const T&)>;
    using ChildFunction = std::function<Spec(const T&)>;
    ForEachModel(Binding<Items> value,KeyFunction key,ChildFunction child)
        : source(std::move(value)),key_function(std::move(key)),child_function(std::move(child)) {}
    Binding<Items> source;
    KeyFunction key_function;
    ChildFunction child_function;
};
template <class T>
class ForEachComponent final : public ForEachHostComponent {
public:
    using Items = std::vector<T>;
    using KeyFunction = typename ForEachModel<T>::KeyFunction;
    using ChildFunction = typename ForEachModel<T>::ChildFunction;
    ForEachComponent(Binding<Items> source,KeyFunction key,ChildFunction child)
        : ForEachComponent(std::make_shared<ForEachModel<T>>(std::move(source),std::move(key),std::move(child))) {}
private:
    explicit ForEachComponent(std::shared_ptr<ForEachModel<T>> model)
        : ForEachHostComponent([model] {
            const auto items = std::make_shared<const Items>(model->source.get());
            DynamicSnapshot snapshot;
            snapshot.keys.reserve(items->size());
            for (const auto& item : *items) snapshot.keys.push_back(model->key_function(item));
            snapshot.build_children = [model,items] {
                std::vector<Spec> children;
                children.reserve(items->size());
                for (const auto& item : *items) children.push_back(model->child_function(item));
                return children;
            };
            return snapshot;
        },dynamic_observe(model->source)) {}
};
} // namespace detail

template <class T>
class ForEach {
public:
    using Items = std::vector<T>;
    template <class KeyFunction,class ChildFunction>
    ForEach(Binding<Items> source,KeyFunction key,ChildFunction child)
        : state_(std::move(source)),
          key_function_([function=std::move(key)](const T& item) mutable {
              return detail::encode_dynamic_key(std::invoke(function,item));
          }),child_function_([function=std::move(child)](const T& item) mutable {
              return detail::dynamic_make_spec(std::invoke(function,item));
          }) {}
    template <class KeyFunction,class ChildFunction>
    ForEach(State<Items>& source,KeyFunction key,ChildFunction child)
        : ForEach(source.binding(),std::move(key),std::move(child)) {}
    Spec spec() && {
        auto source = state_;
        auto key = std::move(key_function_);
        auto child = std::move(child_function_);
        Spec result{[source,key=std::move(key),child=std::move(child)] {
            return std::make_unique<detail::ForEachComponent<T>>(source,key,child);
        },{}};
        result.children_factory = [](Component& component) {
            return static_cast<detail::ForEachComponent<T>&>(component).prepare_initial_children();
        };
        return result;
    }
private:
    Binding<Items> state_;
    typename detail::ForEachComponent<T>::KeyFunction key_function_;
    typename detail::ForEachComponent<T>::ChildFunction child_function_;
};
} // namespace ui
