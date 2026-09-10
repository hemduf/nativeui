#pragma once

#include <nativeui/component_base.hpp>

#include <functional>
#include <utility>
#include <vector>

namespace ui::detail {

class VirtualListRowInteractionComponent final : public Component {
public:
    using BeginCapture = std::function<bool()>;
    using EndCapture = std::function<void()>;
    using Activate = std::function<bool()>;

    VirtualListRowInteractionComponent(
        BeginCapture begin_capture,
        EndCapture end_capture,
        Activate activate)
        : begin_capture_(std::move(begin_capture)),
          end_capture_(std::move(end_capture)),
          activate_(std::move(activate)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        switch (event.type) {
        case InputType::PointerDown: {
            if (armed_) return EventResult::Handled;
            auto begin_capture = begin_capture_;
            if (!begin_capture || !begin_capture()) return EventResult::Handled;
            armed_ = true;
            context.capture_pointer();
            return EventResult::Handled;
        }
        case InputType::PointerUp: {
            if (!armed_) return EventResult::Ignored;
            const bool activate = context.bounds().contains(event.position);
            armed_ = false;
            auto end_capture = end_capture_;
            auto activation = activate_;
            context.release_pointer();
            if (end_capture) end_capture();
            if (activate && activation) (void)activation();
            return EventResult::Handled;
        }
        case InputType::PointerCancel: {
            if (!armed_) return EventResult::Ignored;
            armed_ = false;
            auto end_capture = end_capture_;
            context.release_pointer();
            if (end_capture) end_capture();
            return EventResult::Handled;
        }
        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext&) override {
        if (!armed_) return;
        armed_ = false;
        auto end_capture = end_capture_;
        if (end_capture) end_capture();
    }

    void paint(PaintContext&) const override {}

private:
    BeginCapture begin_capture_;
    EndCapture end_capture_;
    Activate activate_;
    bool armed_{};
};

} // namespace ui::detail
