#pragma once

/// \file
/// Public retained dynamic-composition builders: If, Switch and ForEach.
///
/// These builders observe UI State/Binding values and request structural
/// reconciliation at NativeUI's retained safe checkpoint. They do not mutate
/// the component tree synchronously from a State observer. Construction,
/// reconciliation and callback execution are UI/main-thread work and may
/// allocate; none of this API is an audio-real-time synchronization boundary.

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_key.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

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
    IfComponent(Binding<bool> state, std::shared_ptr<const Spec> child)
        : state_(std::move(state)), child_(std::move(child)) {}

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        if (!state_.get()) return {};
        return {"if:true"};
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        if (!state_.get()) return {};
        return {DynamicChildSpec{"if:true", *child_}};
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        structure_invalidator_ = std::move(invalidator);
    }

    void mount(MountContext&) override {
        subscription_ = state_.observe([this](const bool&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    Binding<bool> state_;
    std::shared_ptr<const Spec> child_;
    Binding<bool>::Subscription subscription_;
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
    SwitchComponent(Binding<T> state,
                    std::vector<SwitchBranch<T>> branches,
                    std::shared_ptr<const Spec> fallback)
        : state_(std::move(state)),
          branches_(std::move(branches)),
          fallback_(std::move(fallback)) {}

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
        subscription_ = state_.observe([this](const T&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    [[nodiscard]] const SwitchBranch<T>* selected_branch() const noexcept {
        const auto& selected = state_.get();
        const auto it = std::find_if(branches_.begin(), branches_.end(), [&](const auto& branch) {
            return branch.value == selected;
        });
        return it == branches_.end() ? nullptr : &*it;
    }

    Binding<T> state_;
    std::vector<SwitchBranch<T>> branches_;
    std::shared_ptr<const Spec> fallback_;
    typename Binding<T>::Subscription subscription_;
    std::function<void()> structure_invalidator_;
};

template <class T>
class ForEachComponent final : public DynamicHostComponent, public DynamicChildrenSource {
public:
    using Items = std::vector<T>;
    using KeyFunction = std::function<std::string(const T&)>;
    using ChildFunction = std::function<Spec(const T&)>;

    ForEachComponent(Binding<Items> state, KeyFunction key_function, ChildFunction child_function)
        : state_(std::move(state)),
          key_function_(std::move(key_function)),
          child_function_(std::move(child_function)) {}

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        std::vector<std::string> result;
        result.reserve(state_.get().size());
        for (const auto& item : state_.get()) result.push_back(key_function_(item));
        return result;
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        std::vector<DynamicChildSpec> result;
        result.reserve(state_.get().size());
        for (const auto& item : state_.get()) {
            result.push_back(DynamicChildSpec{key_function_(item), child_function_(item)});
        }
        return result;
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        structure_invalidator_ = std::move(invalidator);
    }

    void mount(MountContext&) override {
        subscription_ = state_.observe([this](const Items&) {
            if (structure_invalidator_) structure_invalidator_();
        });
    }

    void unmount(LifecycleContext&) override {
        subscription_.reset();
        structure_invalidator_ = {};
    }

private:
    Binding<Items> state_;
    KeyFunction key_function_;
    ChildFunction child_function_;
    typename Binding<Items>::Subscription subscription_;
    std::function<void()> structure_invalidator_;
};

} // namespace detail

/// Conditionally retain one child while a boolean Binding is true.
///
/// The child is converted to an owned `Spec` when the builder is constructed.
/// State changes only request structural reconciliation; actual mount/unmount
/// happens later at the retained tree's safe structural checkpoint.
///
/// The Binding follows NativeUI's normal UI-thread/lifetime contract. Removing
/// the child runs normal retained focus/capture/lifecycle reconciliation; keeping
/// an `If` builder or its produced Spec does not create process-global state.
class If {
public:
    /// Build from a Binding. `child` is converted to Spec immediately.
    template <class Child>
    If(Binding<bool> state, Child&& child)
        : state_(std::move(state)), child_(make_spec(std::forward<Child>(child))) {}

    /// Convenience overload borrowing observable state through its Binding.
    template <class Child>
    If(State<bool>& state, Child&& child)
        : If(state.binding(), std::forward<Child>(child)) {}

    /// Consume this builder and produce a reusable retained specification.
    ///
    /// If the current value is true the initial compiled structure contains the
    /// child; later changes are reconciled by retained identity `"if:true"`.
    Spec spec() && {
        auto child = std::make_shared<const Spec>(std::move(child_));
        std::vector<Spec> initial_children;
        if (state_.get()) initial_children.push_back(*child);
        auto state = state_;
        return Spec{
            [state, child] { return std::make_unique<detail::IfComponent>(state, child); },
            std::move(initial_children)};
    }

private:
    Binding<bool> state_;
    Spec child_;
};

/// Select exactly one retained branch from an observable value.
///
/// `T` must support equality comparison with the values passed to `when()`.
/// Branches are tested in declaration order; when several branches compare
/// equal, the first matching branch wins. `otherwise()` supplies an optional
/// fallback when no branch matches.
///
/// Each declared branch owns a reusable Spec. Returning to a previously removed
/// branch reconstructs its retained subtree from that Spec; state changes are
/// applied only at the retained structural checkpoint. All operations are
/// UI/main-thread work and may allocate/call component factories.
template <class T>
class Switch {
public:
    /// Build from a Binding whose current value selects the active branch.
    explicit Switch(Binding<T> state) : state_(std::move(state)) {}
    /// Convenience overload using `state.binding()`.
    explicit Switch(State<T>& state) : Switch(state.binding()) {}

    /// Append one branch and return this lvalue builder.
    ///
    /// `child` is converted to an owned Spec immediately. Branch retained keys
    /// are stable by declaration position, not derived from `value`.
    template <class Child>
    Switch& when(T value, Child&& child) & {
        const auto index = branches_.size();
        branches_.push_back(detail::SwitchBranch<T>{
            std::move(value),
            "switch:" + std::to_string(index),
            std::make_shared<const Spec>(make_spec(std::forward<Child>(child)))});
        return *this;
    }

    /// Rvalue-qualified fluent overload of `when()`.
    template <class Child>
    Switch&& when(T value, Child&& child) && {
        when(std::move(value), std::forward<Child>(child));
        return std::move(*this);
    }

    /// Set/replace the fallback branch used when no `when()` value matches.
    template <class Child>
    Switch& otherwise(Child&& child) & {
        fallback_ = std::make_shared<const Spec>(make_spec(std::forward<Child>(child)));
        return *this;
    }

    /// Rvalue-qualified fluent overload of `otherwise()`.
    template <class Child>
    Switch&& otherwise(Child&& child) && {
        otherwise(std::forward<Child>(child));
        return std::move(*this);
    }

    /// Consume the builder and produce the retained dynamic specification.
    ///
    /// The current selection seeds the initial compiled child. Subsequent State
    /// notifications only enqueue reconciliation; they do not synchronously
    /// splice retained nodes on the observer callback stack.
    Spec spec() && {
        std::vector<Spec> initial_children;
        const auto& selected = state_.get();
        const auto selected_it = std::find_if(
            branches_.begin(), branches_.end(), [&](const auto& branch) {
                return branch.value == selected;
            });
        if (selected_it != branches_.end()) {
            initial_children.push_back(*selected_it->spec);
        } else if (fallback_) {
            initial_children.push_back(*fallback_);
        }

        auto state = state_;
        auto branches = std::move(branches_);
        auto fallback = std::move(fallback_);
        return Spec{
            [state, branches = std::move(branches), fallback = std::move(fallback)]() mutable {
                return std::make_unique<detail::SwitchComponent<T>>(
                    state, std::move(branches), std::move(fallback));
            },
            std::move(initial_children)};
    }

private:
    Binding<T> state_;
    std::vector<detail::SwitchBranch<T>> branches_;
    std::shared_ptr<const Spec> fallback_;
};

/// Reconcile a vector of items by stable application-provided keys.
///
/// `Items` is `std::vector<T>`. The key callback must return a string-like,
/// integral or enum value; NativeUI type-encodes it into an owned string key so
/// string/signed/unsigned domains do not collide accidentally. Keys must be
/// unique within every observed snapshot.
///
/// A stable key preserves the existing retained child across reordering. A
/// removed or changed key tears down the old retained child; a new key creates a
/// new child from the child callback. Duplicate-key snapshots are rejected
/// atomically rather than partially replacing the last valid retained structure.
///
/// The key callback can be evaluated more than once during one reconciliation
/// and should therefore be deterministic and free of externally visible side
/// effects. The child callback may be called for items while preparing a changed
/// structural snapshot, not only for keys that ultimately become new retained
/// nodes. Both callbacks execute on the UI/main thread, may allocate/throw, and
/// may be invoked again after a recoverable failed reconciliation.
template <class T>
class ForEach {
public:
    /// Observable collection type reconciled by this builder.
    using Items = std::vector<T>;

    /// Build from a Binding plus key and child callbacks.
    ///
    /// `key_function(const T&)` must return a supported key type:
    /// string/string-view-like, integral, or enum. `child_function(const T&)`
    /// may return either a Spec or any public builder accepted by `make_spec`.
    template <class KeyFunction, class ChildFunction>
    ForEach(Binding<Items> state, KeyFunction key_function, ChildFunction child_function)
        : state_(std::move(state)),
          key_function_([function = std::move(key_function)](const T& item) mutable {
              return detail::encode_dynamic_key(std::invoke(function, item));
          }),
          child_function_([function = std::move(child_function)](const T& item) mutable {
              return detail::dynamic_make_spec(std::invoke(function, item));
          }) {}

    /// Convenience overload using `state.binding()`.
    template <class KeyFunction, class ChildFunction>
    ForEach(State<Items>& state, KeyFunction key_function, ChildFunction child_function)
        : ForEach(state.binding(), std::move(key_function), std::move(child_function)) {}

    /// Consume this builder and create its retained dynamic specification.
    ///
    /// The current item snapshot is inspected immediately to seed the initial
    /// children. If initial keys are duplicated, no partial initial child list
    /// is published; runtime reconciliation continues to reject duplicate
    /// snapshots until a valid unique-key snapshot is observed.
    Spec spec() && {
        std::vector<std::string> initial_keys;
        initial_keys.reserve(state_.get().size());
        for (const auto& item : state_.get()) initial_keys.push_back(key_function_(item));

        bool duplicate_keys = false;
        for (std::size_t i = 0; i < initial_keys.size() && !duplicate_keys; ++i) {
            duplicate_keys = std::find(
                initial_keys.begin() + static_cast<std::ptrdiff_t>(i + 1),
                initial_keys.end(),
                initial_keys[i]) != initial_keys.end();
        }

        std::vector<Spec> initial_children;
        if (!duplicate_keys) {
            initial_children.reserve(state_.get().size());
            for (const auto& item : state_.get()) {
                initial_children.push_back(child_function_(item));
            }
        }

        auto state = state_;
        auto key_function = std::move(key_function_);
        auto child_function = std::move(child_function_);
        return Spec{
            [state,
             key_function = std::move(key_function),
             child_function = std::move(child_function)]() mutable {
                return std::make_unique<detail::ForEachComponent<T>>(
                    state, std::move(key_function), std::move(child_function));
            },
            std::move(initial_children)};
    }

private:
    Binding<Items> state_;
    typename detail::ForEachComponent<T>::KeyFunction key_function_;
    typename detail::ForEachComponent<T>::ChildFunction child_function_;
};

} // namespace ui
