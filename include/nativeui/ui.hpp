#pragma once

#include <nativeui/component.hpp>
#include <nativeui/overlay.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

/// One retained NativeUI component tree.
///
/// UI is intentionally confined to the host/platform UI thread. Its retained
/// tree, focus/input routing, invalidation, layout and painting APIs are not
/// synchronized and must not be called directly from a VST3/CLAP audio or
/// worker thread. Plug-in adapters must transfer cross-thread state through an
/// explicitly reviewed thread-safe bridge and apply it from the UI domain.
class UI {
public:
    template <class Root>
    explicit UI(Root&& root)
        : overlay_state_(std::make_shared<detail::OverlayState>()),
          tree_(compile(detail::make_overlay_host_spec(
              make_spec(std::forward<Root>(root)), overlay_state_))) {
        tree_.mount();
    }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints = Constraints::unbounded()) const {
        return tree_.measure(constraints);
    }
    void resize(Size viewport) { tree_.layout(viewport); }
    void activate(PlatformServices& platform) { tree_.activate_focus(platform); }
    void deactivate(PlatformServices& platform) { tree_.deactivate_focus(platform); }
    void refresh_focus(PlatformServices& platform) { tree_.refresh_focus(platform); }
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        return tree_.dispatch(event, platform);
    }
    EventResult cancel_pointer(PlatformServices& platform) {
        return tree_.cancel_pointer(platform);
    }
    void set_command_handler(std::function<EventResult(Command)> handler) {
        tree_.set_global_command_handler(std::move(handler));
    }
    [[nodiscard]] bool dirty() const noexcept { return tree_.dirty(); }
    [[nodiscard]] bool layout_dirty() const noexcept { return tree_.layout_dirty(); }
    [[nodiscard]] bool paint_dirty() const noexcept { return tree_.paint_dirty(); }
    [[nodiscard]] const std::vector<Rect>& dirty_regions() const noexcept {
        return tree_.dirty_regions();
    }
    [[nodiscard]] const std::string& structural_diagnostic() const noexcept {
        return tree_.structural_diagnostic();
    }
    [[nodiscard]] std::optional<ComponentAvailability> component_availability(
        NodeId id) const noexcept {
        return tree_.component_availability(id);
    }
    void set_invalidation_callback(std::function<void(Rect)> callback) {
        tree_.set_invalidation_callback(std::move(callback));
    }
    void set_invalidation_callback(std::function<void()> callback) {
        tree_.set_invalidation_callback(std::move(callback));
    }
    void clear_invalidation_callback() {
        tree_.set_invalidation_callback(std::function<void(Rect)>{});
    }
    void invalidate() { tree_.invalidate(); }
    void invalidate(Rect rect) { tree_.invalidate(rect); }
    void invalidate_layout() { tree_.invalidate_layout(); }
    void paint(SkCanvas& canvas, PlatformServices& platform) { tree_.paint(canvas, platform); }

    /// Queue one in-view overlay through the same T058 structural checkpoint
    /// used by explicit dynamic containers. show/close never splice retained
    /// nodes synchronously on the caller's callback stack.
    [[nodiscard]] OverlayHandle show_overlay(OverlaySpec overlay) {
        return overlay_state_->show(std::move(overlay));
    }

    /// Close an overlay handle owned by this UI. Stale, cross-UI and already
    /// closed handles are deterministic no-ops; retained teardown is deferred
    /// through T058 even though the handle becomes stale immediately.
    bool close_overlay(OverlayHandle handle) {
        return overlay_state_->close(std::move(handle));
    }

private:
    // State must outlive Tree because the retained OverlayHost clears its T058
    // structural invalidator during tree teardown.
    std::shared_ptr<detail::OverlayState> overlay_state_;
    Tree tree_;
};

using PluginUI = UI; // compatibility alias for the original POC

} // namespace ui
