#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {
namespace detail {

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

} // namespace ui
