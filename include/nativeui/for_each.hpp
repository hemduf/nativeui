#pragma once

#include <nativeui/detail/dynamic_host.hpp>
#include <nativeui/detail/dynamic_key.hpp>

#include <algorithm>

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
    std::optional<DynamicSnapshot> cached_snapshot;
    [[nodiscard]] DynamicSnapshot snapshot() {
        return StateReadAccess::read(source,[this](const Items& items) {
            DynamicSnapshot candidate;
            candidate.keys.reserve(items.size());
            for (const auto& item : items) candidate.keys.push_back(key_function(item));
            // Equal keys preserve the existing recipes and must not replay
            // child factories just because the source's data changed.
            if (cached_snapshot && cached_snapshot->keys == candidate.keys) return *cached_snapshot;
            bool duplicate{};
            for (std::size_t i=0; i<candidate.keys.size() && !duplicate; ++i)
                duplicate = std::find(candidate.keys.begin()+static_cast<std::ptrdiff_t>(i+1),
                    candidate.keys.end(),candidate.keys[i]) != candidate.keys.end();
            auto children=std::make_shared<std::vector<Spec>>();
            if (!duplicate) {
                children->reserve(items.size());
                for (const auto& item : items) children->push_back(child_function(item));
            }
            const std::shared_ptr<const std::vector<Spec>> owned=std::move(children);
            candidate.build_children=[owned] { return *owned; };
            cached_snapshot=std::move(candidate);
            return *cached_snapshot;
        });
    }
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
            return model->snapshot();
        },dynamic_observe(model->source)) {}
};
} // namespace detail

/// Keyed retained composition over an observable std::vector<T>.
/// Keys must be unique and encode from supported string-like/integral/enum
/// values. Reorders with stable keys reuse the same retained subtrees.
/// Equal key sequences reuse cached child recipes: changes to other item
/// fields do not automatically reconstruct Specs. Bind mutable content
/// to State/Binding. Key/child callbacks may allocate, throw and be retried.
template <class T>
class ForEach {
public:
    using Items = std::vector<T>;
    template <class KeyFunction,class ChildFunction>
/// Own the source Binding and callback objects. A const T& callback argument
/// is borrowed only for that invocation. Key computation should be pure and
/// deterministic; duplicate snapshots are rejected without partial publication.
    ForEach(Binding<Items> source,KeyFunction key,ChildFunction child)
        : state_(std::move(source)),
          key_function_([function=std::move(key)](const T& item) mutable {
              return detail::encode_dynamic_key(std::invoke(function,item));
          }),child_function_([function=std::move(child)](const T& item) mutable {
              return detail::dynamic_make_spec(std::invoke(function,item));
          }) {}
    template <class KeyFunction,class ChildFunction>
/// Convert a borrowed State to its Binding; do not retain a raw reference.
    ForEach(State<Items>& source,KeyFunction key,ChildFunction child)
        : ForEach(source.binding(),std::move(key),std::move(child)) {}
/// Consume the source/callbacks into a dynamic retained Spec; reconciliation
/// is UI-thread-only and not an audio-thread synchronization facility.
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
