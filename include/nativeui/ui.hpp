#pragma once

#include <nativeui/component.hpp>
#include <nativeui/overlay.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
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
        : tree_(compile(make_spec(std::forward<Root>(root)))) {
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

    /// Queue one in-view overlay description for this UI. The retained-tree
    /// integration is completed by T061; this owner-scoped handle seam rejects
    /// the one structurally invalid v1 policy combination up front.
    [[nodiscard]] OverlayHandle show_overlay(OverlaySpec overlay) {
        if (overlay.mode == OverlayMode::Modal &&
            overlay.pointer_policy == OverlayPointerPolicy::Ignore) {
            return {};
        }
        if (next_overlay_id_ == 0) return {};

        const auto id = next_overlay_id_;
        if (next_overlay_id_ == std::numeric_limits<std::uint64_t>::max()) {
            next_overlay_id_ = 0;
        } else {
            ++next_overlay_id_;
        }
        overlays_.push_back(PendingOverlay{id, std::move(overlay)});
        return OverlayHandle{overlay_owner_, id};
    }

    /// Close an overlay handle owned by this UI. Stale, cross-UI and already
    /// closed handles are deterministic no-ops.
    bool close_overlay(OverlayHandle handle) {
        const auto owner = handle.owner_.lock();
        if (!owner || owner != overlay_owner_ || handle.id_ == 0) return false;
        const auto before = overlays_.size();
        std::erase_if(overlays_, [id = handle.id_](const PendingOverlay& entry) {
            return entry.id == id;
        });
        return overlays_.size() != before;
    }

private:
    struct PendingOverlay {
        std::uint64_t id{};
        OverlaySpec spec;
    };

    Tree tree_;
    std::shared_ptr<const detail::OverlayOwnerToken> overlay_owner_{
        std::make_shared<const detail::OverlayOwnerToken>()};
    std::uint64_t next_overlay_id_{1};
    std::vector<PendingOverlay> overlays_;
};

using PluginUI = UI; // compatibility alias for the original POC

} // namespace ui
