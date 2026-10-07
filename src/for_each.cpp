#include <nativeui/for_each.hpp>

#include <algorithm>
#include <stdexcept>

namespace ui::detail {
namespace {
bool duplicates(const std::vector<std::string>& keys) {
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (std::find(keys.begin()+static_cast<std::ptrdiff_t>(i+1),keys.end(),keys[i]) != keys.end()) return true;
    return false;
}
}
ForEachHostComponent::ForEachHostComponent(Snapshot snapshot,DynamicObserve observe)
    : snapshot_(std::move(snapshot)),observe_(std::move(observe)) {}
std::vector<std::string> ForEachHostComponent::desired_keys() const {
    if (!mounted_ && initial_prepared_) return initial_keys_;
    auto candidate = snapshot_();
    auto result = candidate.keys;
    prepared_ = std::move(candidate);
    return result;
}
std::vector<DynamicChildSpec> ForEachHostComponent::desired_children() const {
    if (!prepared_) prepared_ = snapshot_();
    // Keep this snapshot alive across user factories that trigger a nested
    // desired_keys() request; no borrowed T or prepared_ address crosses code.
    const auto snapshot = *prepared_;
    auto children = snapshot.build_children();
    if (children.size() != snapshot.keys.size()) throw std::logic_error("ForEach snapshot child/key mismatch");
    std::vector<DynamicChildSpec> result;
    result.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i)
        result.push_back({snapshot.keys[i],std::move(children[i])});
    return result;
}
void ForEachHostComponent::set_structure_invalidator(std::function<void()> invalidator) {
    invalidate_structure_ = std::move(invalidator);
}
void ForEachHostComponent::mount(MountContext&) {
    subscription_ = observe_(invalidate_structure_);
    mounted_ = true;
    if (invalidate_structure_) invalidate_structure_();
}
void ForEachHostComponent::unmount(LifecycleContext&) {
    mounted_ = false; subscription_.reset(); invalidate_structure_ = {}; prepared_.reset();
}
std::vector<Spec> ForEachHostComponent::prepare_initial_children() {
    auto candidate = snapshot_();
    std::vector<Spec> result;
    // Preserve historical duplicate diagnostics/quarantine: initially publish
    // no ambiguous children, leaving Tree to report duplicate logical keys.
    if (!duplicates(candidate.keys)) result = candidate.build_children();
    initial_keys_ = candidate.keys;
    prepared_ = std::move(candidate); initial_prepared_ = true;
    return result;
}
} // namespace ui::detail
