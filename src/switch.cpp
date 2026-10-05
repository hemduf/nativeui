#include <nativeui/switch.hpp>

namespace ui::detail {
SwitchHostComponent::SwitchHostComponent(Selection selection,DynamicObserve observe)
    : selection_(std::move(selection)),observe_(std::move(observe)) {}
std::vector<std::string> SwitchHostComponent::desired_keys() const {
    if (!mounted_ && initial_prepared_) return initial_ ? std::vector<std::string>{initial_->key} : std::vector<std::string>{};
    auto choice = selection_();
    const auto result = choice ? std::vector<std::string>{choice->key} : std::vector<std::string>{};
    prepared_ = std::move(choice); selection_prepared_ = true;
    return result;
}
std::vector<DynamicChildSpec> SwitchHostComponent::desired_children() const {
    const auto choice = selection_prepared_ ? prepared_ : selection_();
    return choice ? std::vector<DynamicChildSpec>{*choice} : std::vector<DynamicChildSpec>{};
}
void SwitchHostComponent::set_structure_invalidator(std::function<void()> invalidator) {
    invalidate_structure_ = std::move(invalidator);
}
void SwitchHostComponent::mount(MountContext&) {
    subscription_ = observe_(invalidate_structure_);
    mounted_ = true;
    // A recipe may have changed its source before subscription during initial
    // compilation. Reconcile once at the ordinary safe checkpoint, preserving
    // the exact recipe/key pairing registered for the initial subtree.
    if (invalidate_structure_) invalidate_structure_();
}
void SwitchHostComponent::unmount(LifecycleContext&) {
    mounted_ = false; subscription_.reset(); invalidate_structure_ = {};
}
std::vector<Spec> SwitchHostComponent::prepare_initial_children() {
    auto choice = selection_();
    std::vector<Spec> result;
    if (choice) result.push_back(choice->spec);
    prepared_ = choice; selection_prepared_ = true;
    initial_ = std::move(choice); initial_prepared_ = true;
    return result;
}
} // namespace ui::detail
