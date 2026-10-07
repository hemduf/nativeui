#pragma once

#include <nativeui/component_base.hpp>

#include <functional>
#include <memory>

namespace ui::detail {
class VirtualListRowContentBarrierComponent final : public Component {
public:
    [[nodiscard]] bool is_focus_scope() const noexcept override;
    [[nodiscard]] bool focus_scope_active() const noexcept override;
    [[nodiscard]] bool focus_scope_traps() const noexcept override;
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>&) const override;
    void layout_children(Rect,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>&) const override;
    void paint(PaintContext&) const override;
};
class VirtualListRowInteractionComponent final : public Component {
public:
    using BeginCapture=std::function<bool()>;
    using EndCapture=std::function<void()>;
    using Activate=std::function<bool()>;
    using PresentationChanged=std::function<bool(bool,bool)>;
    using Allowed=std::function<bool()>;
    VirtualListRowInteractionComponent(BeginCapture,EndCapture,Activate,PresentationChanged={},Allowed={});
    ~VirtualListRowInteractionComponent() override;
    [[nodiscard]] bool pointer_targetable() const noexcept override;
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override;
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>&) const override;
    void layout_children(Rect,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>&) const override;
    void mount(MountContext&) override;
    void unmount(LifecycleContext&) override;
    EventResult input(const InputEvent&,InputContext&) override;
    void deactivate(LifecycleContext&) override;
    void paint(PaintContext&) const override;
private:
    void effective_availability_changed(const ComponentAvailability&,const ComponentAvailability&) noexcept override;
    struct State;
    static void clear(const std::shared_ptr<State>&);
    static void clear_noexcept(const std::shared_ptr<State>&) noexcept;
    [[nodiscard]] static bool allowed(const std::shared_ptr<State>&,std::uint64_t);
    std::shared_ptr<State> state_;
};
} // namespace ui::detail
