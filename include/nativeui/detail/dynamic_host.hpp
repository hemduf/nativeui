#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui::detail {

template <class Built>
[[nodiscard]] Spec dynamic_make_spec(Built&& built) {
    if constexpr (std::is_same_v<std::remove_cvref_t<Built>, Spec>) return std::forward<Built>(built);
    else return make_spec(std::forward<Built>(built));
}

class DynamicHostComponent : public Component {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override;
    void paint(PaintContext&) const override;
};

class DynamicSubscription {
public:
    virtual ~DynamicSubscription() = default;
};
template <class Subscription>
class OwnedDynamicSubscription final : public DynamicSubscription {
public:
    explicit OwnedDynamicSubscription(Subscription subscription) : subscription_(std::move(subscription)) {}
private:
    Subscription subscription_;
};
using DynamicObserve = std::function<std::unique_ptr<DynamicSubscription>(std::function<void()>)>;

template <class T>
DynamicObserve dynamic_observe(Binding<T> source) {
    return [source](std::function<void()> invalidate) mutable {
        auto subscription = source.observe([invalidate = std::move(invalidate)](const T&) {
            if (invalidate) invalidate();
        });
        return std::make_unique<OwnedDynamicSubscription<typename Binding<T>::Subscription>>(std::move(subscription));
    };
}

class SwitchHostComponent : public DynamicHostComponent, public DynamicChildrenSource {
public:
    using Selection = std::function<std::optional<DynamicChildSpec>()>;
    SwitchHostComponent(Selection selection,DynamicObserve observe);
    [[nodiscard]] std::vector<std::string> desired_keys() const override;
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override;
    void set_structure_invalidator(std::function<void()> invalidator) override;
    void mount(MountContext&) override;
    void unmount(LifecycleContext&) override;
    [[nodiscard]] std::vector<Spec> prepare_initial_children();
private:
    Selection selection_;
    DynamicObserve observe_;
    std::unique_ptr<DynamicSubscription> subscription_;
    std::function<void()> invalidate_structure_;
    std::optional<DynamicChildSpec> initial_;
    mutable std::optional<DynamicChildSpec> prepared_;
    mutable bool selection_prepared_{};
    bool initial_prepared_{};
    bool mounted_{};
};

struct DynamicSnapshot {
    std::vector<std::string> keys;
    std::function<std::vector<Spec>()> build_children;
};
class ForEachHostComponent : public DynamicHostComponent, public DynamicChildrenSource {
public:
    using Snapshot = std::function<DynamicSnapshot()>;
    ForEachHostComponent(Snapshot snapshot,DynamicObserve observe);
    [[nodiscard]] std::vector<std::string> desired_keys() const override;
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override;
    void set_structure_invalidator(std::function<void()> invalidator) override;
    void mount(MountContext&) override;
    void unmount(LifecycleContext&) override;
    [[nodiscard]] std::vector<Spec> prepare_initial_children();
private:
    Snapshot snapshot_;
    DynamicObserve observe_;
    std::unique_ptr<DynamicSubscription> subscription_;
    std::function<void()> invalidate_structure_;
    mutable std::optional<DynamicSnapshot> prepared_;
    std::vector<std::string> initial_keys_;
    bool initial_prepared_{};
    bool mounted_{};
};
} // namespace ui::detail
