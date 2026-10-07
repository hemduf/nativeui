#include <nativeui/if.hpp>

namespace ui::detail {
IfComponent::IfComponent(Binding<bool> state,std::shared_ptr<const Spec> child)
    : state_(std::move(state)),child_(std::move(child)) {}
std::vector<std::string> IfComponent::desired_keys() const {
    const bool open = !mounted_ && initial_prepared_ ? initial_open_ : state_.get();
    return open ? std::vector<std::string>{"if:true"} : std::vector<std::string>{};
}
std::vector<DynamicChildSpec> IfComponent::desired_children() const {
    return state_.get() ? std::vector<DynamicChildSpec>{{"if:true",*child_}} : std::vector<DynamicChildSpec>{};
}
void IfComponent::set_structure_invalidator(std::function<void()> invalidator) {
    invalidate_structure_ = std::move(invalidator);
}
void IfComponent::mount(MountContext&) {
    subscription_ = state_.observe([invalidate=invalidate_structure_](bool) { if (invalidate) invalidate(); });
    mounted_ = true;
    if (invalidate_structure_) invalidate_structure_();
}
void IfComponent::unmount(LifecycleContext&) {
    mounted_ = false; subscription_.reset(); invalidate_structure_ = {};
}
std::vector<Spec> IfComponent::prepare_initial_children() {
    const bool open = state_.get();
    std::vector<Spec> result;
    if (open) result.push_back(*child_);
    initial_open_ = open; initial_prepared_ = true;
    return result;
}
} // namespace ui::detail
namespace ui {
Spec If::spec() && {
    const auto source = state_;
    const auto child = std::make_shared<const Spec>(std::move(child_));
    Spec result{[source,child] { return std::make_unique<detail::IfComponent>(source,child); },{}};
    result.children_factory = [](Component& component) {
        return static_cast<detail::IfComponent&>(component).prepare_initial_children();
    };
    return result;
}
} // namespace ui
