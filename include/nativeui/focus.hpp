#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ui {

/// Focus-only subtree boundary. Rendering and visibility are intentionally
/// independent: `active=false` only removes descendants from focus targeting.
class FocusScopeComponent final : public Component {
public:
    FocusScopeComponent(State<bool>& active, bool trap, std::size_t default_index)
        : active_(&active), trap_(trap), default_index_(default_index) {}

    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return active_->get(); }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return trap_; }
    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override {
        return default_index_;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext& context) override {
        subscription_ = active_->observe(
            [invalidate_focus = context.focus_invalidator(),
             invalidate = context.invalidator()](const bool&) {
                invalidate_focus();
                invalidate();
            });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }
    void paint(PaintContext&) const override {}

private:
    State<bool>* active_{};
    bool trap_{true};
    std::size_t default_index_{};
    State<bool>::Subscription subscription_;
};

class FocusScope {
public:
    template <class Child>
    FocusScope(State<bool>& active, Child&& child) : active_(&active) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    FocusScope&& trap(bool value = true) && {
        trap_ = value;
        return std::move(*this);
    }

    FocusScope&& default_focus(std::size_t focusable_descendant_index) && {
        default_index_ = focusable_descendant_index;
        return std::move(*this);
    }

    Spec spec() && {
        auto* active = active_;
        const bool trap = trap_;
        const auto default_index = default_index_;
        return Spec{
            [active, trap, default_index] {
                return std::make_unique<FocusScopeComponent>(*active, trap, default_index);
            },
            std::move(children_)};
    }

private:
    State<bool>* active_{};
    bool trap_{true};
    std::size_t default_index_{};
    std::vector<Spec> children_;
};

} // namespace ui
