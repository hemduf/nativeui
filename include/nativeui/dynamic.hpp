#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

template <class>
inline constexpr bool kUnsupportedDynamicKey = false;

template <class Key>
[[nodiscard]] std::string encode_dynamic_key(Key&& key) {
    using Value = std::remove_cvref_t<Key>;
    if constexpr (std::is_same_v<Value, std::string>) {
        return "s:" + std::forward<Key>(key);
    } else if constexpr (std::is_convertible_v<Key, std::string_view>) {
        return "s:" + std::string{std::string_view{std::forward<Key>(key)}};
    } else if constexpr (std::is_enum_v<Value>) {
        return encode_dynamic_key(static_cast<std::underlying_type_t<Value>>(key));
    } else if constexpr (std::is_integral_v<Value> && std::is_signed_v<Value>) {
        return "i:" + std::to_string(static_cast<long long>(key));
    } else if constexpr (std::is_integral_v<Value>) {
        return "u:" + std::to_string(static_cast<unsigned long long>(key));
    } else {
        static_assert(kUnsupportedDynamicKey<Value>,
                      "NativeUI dynamic keys must be string-like, integral, or enum values");
    }
}

template <class Built>
[[nodiscard]] Spec dynamic_make_spec(Built&& built) {
    if constexpr (std::is_same_v<std::remove_cvref_t<Built>, Spec>) {
        return std::forward<Built>(built);
    } else {
        return make_spec(std::forward<Built>(built));
    }
}

class DynamicHostComponent : public Component {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.preferred.w);
            result.h = std::max(result.h, child.preferred.h);
        }
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        const auto count = std::min(children.size(), placements.size());
        for (std::size_t i = 0; i < count; ++i) placements[i].bounds = bounds;
    }

    void paint(PaintContext&) const override {}
};

class IfComponent final : public DynamicHostComponent, public DynamicChildrenSource {
public:
    IfComponent(State<bool>& state, std::shared_ptr<const Spec> child)
        : state_(&state), child_(std::move(child)) {}

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        if (!state_->get()) return {};
        return {"if:true"};
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        if (!state_->get()) return {};
        return {DynamicChildSpec{"if:true", *child_}};
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        structure_invalidator_ = std::move(invalidator);
    }

    void mount(MountContext&) override {
        subscription_ = state_->observe([this](const bool&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    State<bool>* state_{};
    std::shared_ptr<const Spec> child_;
    State<bool>::Subscription subscription_;
    std::function<void()> structure_invalidator_;
};

template <class T>
struct SwitchBranch {
    T value;
    std::string key;
    std::shared_ptr<const Spec> spec;
};

template <class T>
class SwitchComponent final : public DynamicHostComponent, public DynamicChildrenSource {
public:
    SwitchComponent(State<T>& state,
                    std::vector<SwitchBranch<T>> branches,
                    std::shared_ptr<const Spec> fallback)
        : state_(&state), branches_(std::move(branches)), fallback_(std::move(fallback)) {}

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        if (const auto* branch = selected_branch()) return {branch->key};
        if (fallback_) return {"switch:fallback"};
        return {};
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        if (const auto* branch = selected_branch()) {
            return {DynamicChildSpec{branch->key, *branch->spec}};
        }
        if (fallback_) return {DynamicChildSpec{"switch:fallback", *fallback_}};
        return {};
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        structure_invalidator_ = std::move(invalidator);
    }

    void mount(MountContext&) override {
        subscription_ = state_->observe([this](const T&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    [[nodiscard]] const SwitchBranch<T>* selected_branch() const noexcept {
        const auto& selected = state_->get();
        const auto it = std::find_if(branches_.begin(), branches_.end(), [&](const auto& branch) {
            return branch.value == selected;
        });
        return it == branches_.end() ? nullptr : &*it;
    }

    State<T>* state_{};
    std::vector<SwitchBranch<T>> branches_;
    std::shared_ptr<const Spec> fallback_;
    typename State<T>::Subscription subscription_;
    std::function<void()> structure_invalidator_;
};

template <class T>
class ForEachComponent final : public DynamicHostComponent, public DynamicChildrenSource {
public:
    using Items = std::vector<T>;
    using KeyFunction = std::function<std::string(const T&)>;
    using ChildFunction = std::function<Spec(const T&)>;

    ForEachComponent(State<Items>& state, KeyFunction key_function, ChildFunction child_function)
        : state_(&state),
          key_function_(std::move(key_function)),
          child_function_(std::move(child_function)) {}

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        std::vector<std::string> result;
        result.reserve(state_->get().size());
        for (const auto& item : state_->get()) result.push_back(key_function_(item));
        return result;
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        std::vector<DynamicChildSpec> result;
        result.reserve(state_->get().size());
        for (const auto& item : state_->get()) {
            result.push_back(DynamicChildSpec{key_function_(item), child_function_(item)});
        }
        return result;
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        structure_invalidator_ = std::move(invalidator);
    }

    void mount(MountContext&) override {
        subscription_ = state_->observe([this](const Items&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    State<Items>* state_{};
    KeyFunction key_function_;
    ChildFunction child_function_;
    typename State<Items>::Subscription subscription_;
    std::function<void()> structure_invalidator_;
};

} // namespace detail

class If {
public:
    template <class Child>
    If(State<bool>& state, Child&& child)
        : state_(&state), child_(make_spec(std::forward<Child>(child))) {}

    Spec spec() && {
        auto child = std::make_shared<const Spec>(std::move(child_));
        std::vector<Spec> initial_children;
        if (state_->get()) initial_children.push_back(*child);
        auto* state = state_;
        return Spec{
            [state, child] { return std::make_unique<detail::IfComponent>(*state, child); },
            std::move(initial_children)};
    }

private:
    State<bool>* state_{};
    Spec child_;
};

template <class T>
class Switch {
public:
    explicit Switch(State<T>& state) : state_(&state) {}

    template <class Child>
    Switch& when(T value, Child&& child) & {
        const auto index = branches_.size();
        branches_.push_back(detail::SwitchBranch<T>{
            std::move(value),
            "switch:" + std::to_string(index),
            std::make_shared<const Spec>(make_spec(std::forward<Child>(child)))});
        return *this;
    }

    template <class Child>
    Switch&& when(T value, Child&& child) && {
        when(std::move(value), std::forward<Child>(child));
        return std::move(*this);
    }

    template <class Child>
    Switch& otherwise(Child&& child) & {
        fallback_ = std::make_shared<const Spec>(make_spec(std::forward<Child>(child)));
        return *this;
    }

    template <class Child>
    Switch&& otherwise(Child&& child) && {
        otherwise(std::forward<Child>(child));
        return std::move(*this);
    }

    Spec spec() && {
        std::vector<Spec> initial_children;
        const auto& selected = state_->get();
        const auto selected_it = std::find_if(
            branches_.begin(), branches_.end(), [&](const auto& branch) {
                return branch.value == selected;
            });
        if (selected_it != branches_.end()) {
            initial_children.push_back(*selected_it->spec);
        } else if (fallback_) {
            initial_children.push_back(*fallback_);
        }

        auto* state = state_;
        auto branches = std::move(branches_);
        auto fallback = std::move(fallback_);
        return Spec{
            [state, branches = std::move(branches), fallback = std::move(fallback)]() mutable {
                return std::make_unique<detail::SwitchComponent<T>>(
                    *state, std::move(branches), std::move(fallback));
            },
            std::move(initial_children)};
    }

private:
    State<T>* state_{};
    std::vector<detail::SwitchBranch<T>> branches_;
    std::shared_ptr<const Spec> fallback_;
};

template <class T>
class ForEach {
public:
    using Items = std::vector<T>;

    template <class KeyFunction, class ChildFunction>
    ForEach(State<Items>& state, KeyFunction key_function, ChildFunction child_function)
        : state_(&state),
          key_function_([function = std::move(key_function)](const T& item) mutable {
              return detail::encode_dynamic_key(std::invoke(function, item));
          }),
          child_function_([function = std::move(child_function)](const T& item) mutable {
              return detail::dynamic_make_spec(std::invoke(function, item));
          }) {}

    Spec spec() && {
        std::vector<Spec> initial_children;
        initial_children.reserve(state_->get().size());
        for (const auto& item : state_->get()) initial_children.push_back(child_function_(item));

        auto* state = state_;
        auto key_function = std::move(key_function_);
        auto child_function = std::move(child_function_);
        return Spec{
            [state,
             key_function = std::move(key_function),
             child_function = std::move(child_function)]() mutable {
                return std::make_unique<detail::ForEachComponent<T>>(
                    *state, std::move(key_function), std::move(child_function));
            },
            std::move(initial_children)};
    }

private:
    State<Items>* state_{};
    typename detail::ForEachComponent<T>::KeyFunction key_function_;
    typename detail::ForEachComponent<T>::ChildFunction child_function_;
};

} // namespace ui
