#pragma once

#include <nativeui/detail/dynamic_host.hpp>

namespace ui {
namespace detail {
class IfComponent final : public DynamicHostComponent, public DynamicChildrenSource {
public:
    IfComponent(Binding<bool> state,std::shared_ptr<const Spec> child);
    [[nodiscard]] std::vector<std::string> desired_keys() const override;
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override;
    void set_structure_invalidator(std::function<void()> invalidator) override;
    void mount(MountContext&) override;
    void unmount(LifecycleContext&) override;
    [[nodiscard]] std::vector<Spec> prepare_initial_children();
private:
    Binding<bool> state_;
    std::shared_ptr<const Spec> child_;
    Binding<bool>::Subscription subscription_;
    std::function<void()> invalidate_structure_;
    bool initial_open_{};
    bool initial_prepared_{};
    bool mounted_{};
};
}
/// Conditional one-child retained composition controlled by an observable bool.
/// The child Spec is owned at construction; changes reconcile at a safe UI
/// checkpoint rather than on the observer stack. Removing the subtree uses
/// normal retained lifecycle, focus and pointer-capture teardown.
class If {
public:
    template <class Child>
/// Keep a Binding handle and convert child to an owned Spec; may throw.
    If(Binding<bool> source,Child&& child) : state_(std::move(source)),child_(make_spec(std::forward<Child>(child))) {}
    template <class Child>
/// State convenience overload; the resulting recipe stores its Binding.
    If(State<bool>& source,Child&& child) : If(source.binding(),std::forward<Child>(child)) {}
/// Consume this builder into a retained dynamic composition recipe.
    Spec spec() &&;
private:
    Binding<bool> state_;
    Spec child_;
};
} // namespace ui
