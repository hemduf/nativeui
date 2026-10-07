#include <nativeui/detail/virtual_list_row.hpp>

#include <exception>
#include <utility>

namespace ui::detail {
bool VirtualListRowContentBarrierComponent::is_focus_scope() const noexcept { return true; }
bool VirtualListRowContentBarrierComponent::focus_scope_active() const noexcept { return false; }
bool VirtualListRowContentBarrierComponent::focus_scope_traps() const noexcept { return false; }
Size VirtualListRowContentBarrierComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty()?Size{}:children.front().preferred;
}
Size VirtualListRowContentBarrierComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty()?Size{}:children.front().minimum;
}
void VirtualListRowContentBarrierComponent::layout_children(Rect bounds,const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const { if (!placements.empty()) placements.front().bounds=bounds; }
void VirtualListRowContentBarrierComponent::paint(PaintContext&) const {}

struct VirtualListRowInteractionComponent::State {
    State(BeginCapture begin,EndCapture end,Activate action,PresentationChanged presentation,Allowed permitted)
        : begin(std::move(begin)),end(std::move(end)),action(std::move(action)),presentation(std::move(presentation)),
          permitted(std::move(permitted)) {}
    BeginCapture begin;
    EndCapture end;
    Activate action;
    PresentationChanged presentation;
    Allowed permitted;
    std::function<void()> release;
    std::uint64_t serial{};
    bool mounted{};
    bool interactive{true};
    bool armed{};
};
VirtualListRowInteractionComponent::VirtualListRowInteractionComponent(BeginCapture begin,EndCapture end,
    Activate action,PresentationChanged presentation,Allowed permitted)
    : state_(std::make_shared<State>(std::move(begin),std::move(end),std::move(action),std::move(presentation),std::move(permitted))) {}
VirtualListRowInteractionComponent::~VirtualListRowInteractionComponent()=default;
bool VirtualListRowInteractionComponent::pointer_targetable() const noexcept { return true; }
Size VirtualListRowInteractionComponent::measure(const std::vector<ChildMetrics>& children) const {
    return children.empty()?Size{}:children.front().preferred;
}
Size VirtualListRowInteractionComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    return children.empty()?Size{}:children.front().minimum;
}
void VirtualListRowInteractionComponent::layout_children(Rect bounds,const std::vector<ChildMetrics>&,
    std::vector<ChildPlacement>& placements) const { if (!placements.empty()) placements.front().bounds=bounds; }
void VirtualListRowInteractionComponent::mount(MountContext&) { state_->mounted=true; }
void VirtualListRowInteractionComponent::unmount(LifecycleContext&) {
    const auto state=state_; state->mounted=false; clear_noexcept(state);
}
void VirtualListRowInteractionComponent::clear(const std::shared_ptr<State>& state) {
    ++state->serial; const auto armed=std::exchange(state->armed,false);
    auto release=std::exchange(state->release,{});
    std::exception_ptr failure;
    try { if (armed && state->end) state->end(); } catch (...) { failure=std::current_exception(); }
    try { if (release) release(); } catch (...) { if (!failure) failure=std::current_exception(); }
    if (failure) std::rethrow_exception(failure);
}
void VirtualListRowInteractionComponent::clear_noexcept(const std::shared_ptr<State>& state) noexcept {
    try { clear(state); } catch (...) {}
}
bool VirtualListRowInteractionComponent::allowed(const std::shared_ptr<State>& state,std::uint64_t serial) {
    if (!state->mounted || !state->interactive || state->serial!=serial) return false;
    const auto permitted=state->permitted;
    if (!state->mounted || !state->interactive || state->serial!=serial) return false;
    const bool result=!permitted || permitted();
    return result && state->mounted && state->interactive && state->serial==serial;
}
EventResult VirtualListRowInteractionComponent::input(const InputEvent& event,InputContext& context) {
    const auto state=state_;
    if (event.type==InputType::PointerCancel) {
        const auto armed=state->armed; const auto serial=state->serial+1;
        clear(state);
        if (!armed) return EventResult::Ignored;
        if (allowed(state,serial)) {
            const auto presentation=state->presentation;
            if (allowed(state,serial) && presentation && presentation(true,false) && allowed(state,serial)) context.invalidate();
        }
        return EventResult::Handled;
    }
    if (event.type==InputType::PointerDown) {
        auto release=context.pointer_releaser(); const auto cleared=state->serial+1;
        clear(state);
        if (!allowed(state,cleared)) return EventResult::Handled;
        const auto begin=state->begin;
        if (!allowed(state,cleared) || !begin) return EventResult::Handled;
        state->armed=true; state->release=std::move(release); const auto serial=++state->serial;
        try {
            if (!begin() || !allowed(state,serial)) {
                if (state->serial==serial) clear(state);
                return EventResult::Handled;
            }
            context.capture_pointer();
            if (!allowed(state,serial)) { if (state->serial==serial) clear(state); return EventResult::Handled; }
            const auto presentation=state->presentation;
            if (allowed(state,serial) && presentation && presentation(false,true) && allowed(state,serial)) context.invalidate();
        } catch (...) { if (state->serial==serial) clear_noexcept(state); throw; }
        return EventResult::Handled;
    }
    if (event.type==InputType::PointerUp) {
        const auto armed=state->armed; const auto inside=context.bounds().contains(event.position);
        const auto serial=state->serial+1; clear(state);
        if (!armed) return EventResult::Ignored;
        if (!allowed(state,serial)) return EventResult::Handled;
        const auto action=state->action;
        if (!allowed(state,serial)) return EventResult::Handled;
        const auto presentation=state->presentation;
        if (!allowed(state,serial)) return EventResult::Handled;
        if (presentation && presentation(true,false) && allowed(state,serial)) context.invalidate();
        if (inside && allowed(state,serial) && action) (void)action();
        return EventResult::Handled;
    }
    return EventResult::Ignored;
}
void VirtualListRowInteractionComponent::deactivate(LifecycleContext&) { const auto state=state_; clear(state); }
void VirtualListRowInteractionComponent::paint(PaintContext&) const {}
void VirtualListRowInteractionComponent::effective_availability_changed(const ComponentAvailability&,
    const ComponentAvailability& next) noexcept {
    const auto state=state_; state->interactive=next.interactive();
    if (!state->interactive) clear_noexcept(state);
}
} // namespace ui::detail
