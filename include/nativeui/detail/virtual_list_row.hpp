#pragma once

#include <nativeui/component_base.hpp>

#include <functional>
#include <utility>
#include <vector>

namespace ui::detail {

// Keeps arbitrary row content out of global keyboard focus traversal while
// allowing the sibling/parent row interaction component to remain pointer
// targetable. Inactive focus scopes are intentionally excluded from pointer
// hit testing by Tree, so the barrier must live *inside* the row owner rather
// than around the complete virtual-list content subtree.
class VirtualListRowContentBarrierComponent final : public Component {
public:
    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return false; }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return false; }

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

    void paint(PaintContext&) const override {}
};

class VirtualListRowInteractionComponent final : public Component {
public:
    using BeginCapture = std::function<bool()>;
    using EndCapture = std::function<void()>;
    using Activate = std::function<bool()>;
    using PresentationChanged = std::function<bool(bool, bool)>;

    VirtualListRowInteractionComponent(
        BeginCapture begin_capture,
        EndCapture end_capture,
        Activate activate,
        PresentationChanged presentation_changed = {})
        : begin_capture_(std::move(begin_capture)),
          end_capture_(std::move(end_capture)),
          activate_(std::move(activate)),
          presentation_changed_(std::move(presentation_changed)) {}

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
            const bool presentation_changed =
                presentation_changed_ && presentation_changed_(false, true);
            armed_ = true;
            if (presentation_changed) context.invalidate();
            context.capture_pointer();
            return EventResult::Handled;
        }
        case InputType::PointerUp: {
            if (!armed_) return EventResult::Ignored;
            const bool activate = context.bounds().contains(event.position);
            const bool presentation_changed =
                presentation_changed_ && presentation_changed_(true, false);
            armed_ = false;
            auto end_capture = end_capture_;
            auto activation = activate_;
            context.release_pointer();
            if (end_capture) end_capture();
            if (presentation_changed) context.invalidate();
            if (activate && activation) (void)activation();
            return EventResult::Handled;
        }
        case InputType::PointerCancel: {
            if (!armed_) return EventResult::Ignored;
            const bool presentation_changed =
                presentation_changed_ && presentation_changed_(true, false);
            armed_ = false;
            auto end_capture = end_capture_;
            context.release_pointer();
            if (end_capture) end_capture();
            if (presentation_changed) context.invalidate();
            return EventResult::Handled;
        }
        default:
            return EventResult::Ignored;
        }
    }

    void deactivate(LifecycleContext& context) override {
        if (!armed_) return;
        const bool presentation_changed =
            presentation_changed_ && presentation_changed_(true, false);
        armed_ = false;
        auto end_capture = end_capture_;
        if (end_capture) end_capture();
        if (presentation_changed) context.invalidate();
    }

    void paint(PaintContext&) const override {}

private:
    BeginCapture begin_capture_;
    EndCapture end_capture_;
    Activate activate_;
    PresentationChanged presentation_changed_;
    bool armed_{};
};

} // namespace ui::detail
