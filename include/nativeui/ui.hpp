#pragma once

#include <nativeui/component.hpp>
#include <nativeui/overlay.hpp>
#include <nativeui/theme.hpp>

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
        : UI(std::forward<Root>(root), default_theme()) {}

    template <class Root>
    UI(Root&& root, Theme theme)
        : overlay_state_(std::make_shared<detail::OverlayState>()),
          tree_(compile(detail::make_overlay_host_spec(
              make_spec(std::forward<Root>(root)), overlay_state_))) {
        tree_.set_theme(std::move(theme));
        tree_.mount();
    }

    [[nodiscard]] const Theme& theme() const noexcept { return tree_.theme(); }
    void set_theme(Theme theme) { tree_.set_theme(std::move(theme)); }

    [[nodiscard]] ChildMetrics measure(const Constraints& constraints = Constraints::unbounded()) const {
        return tree_.measure_overlay_content(constraints);
    }
    void resize(Size viewport) {
        viewport_ = viewport;
        prepare_overlay_layout();
    }
    void activate(PlatformServices& platform) {
        tree_.activate_focus(platform);
        prepare_overlay_layout();
        enforce_new_modal_capture_barrier(platform);
    }
    void deactivate(PlatformServices& platform) {
        close_anchored_overlays();
        tree_.deactivate_focus(platform);
    }
    void refresh_focus(PlatformServices& platform) { tree_.refresh_focus(platform); }
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        // Resolve dynamic/availability/layout changes before using retained
        // overlay bounds for pointer containment or dismissal. A newly-created
        // modal also terminates any capture established by lower content before
        // this event can be routed through the modal barrier.
        prepare_overlay_layout();
        enforce_new_modal_capture_barrier(platform);

        // Overlay dismissal is policy that must run before normal retained-tree
        // delivery: an outside-dismiss PointerDown is consumed and must never
        // click through to lower content in the same event, while Escape is
        // owned by the topmost eligible overlay before a focused root control
        // can consume it. Logical close invalidates the handle immediately;
        // retained destruction remains deferred through the T058 queue.
        if (event.type == InputType::PointerDown) {
            for (auto it = overlay_state_->entries.rbegin();
                 it != overlay_state_->entries.rend(); ++it) {
                if (it->spec.pointer_policy == OverlayPointerPolicy::Ignore) {
                    continue;
                }

                if (it->resolved_bounds.contains(event.position)) {
                    break;
                }

                if (it->spec.dismiss_on_outside_pointer_down) {
                    const auto id = it->id;
                    (void)overlay_state_->close_id(id);
                    return EventResult::Handled;
                }

                if (it->spec.mode == OverlayMode::Modal) {
                    return EventResult::Handled;
                }
            }
        } else if (event.type == InputType::KeyDown && event.key == Key::Escape) {
            for (auto it = overlay_state_->entries.rbegin();
                 it != overlay_state_->entries.rend(); ++it) {
                if (it->spec.dismiss_on_escape) {
                    const auto id = it->id;
                    (void)overlay_state_->close_id(id);
                    return EventResult::Handled;
                }

                // The topmost modal owns keyboard activation below it. If it
                // is not itself Escape-dismissable, lower overlays/root must
                // not observe this Escape; overlays created above it have
                // already had their eligibility checked by this reverse scan.
                if (it->spec.mode == OverlayMode::Modal) {
                    return EventResult::Handled;
                }
            }
        }

        const auto result = tree_.dispatch(event, platform);

        // A callback may have captured the pointer and shown a modal in the
        // same dispatch. T058 mounts that modal only after the callback stack
        // unwinds; cancel the now-lower capture here, after Tree::dispatch has
        // reached that safe checkpoint, never reentrantly inside PointerDown.
        enforce_new_modal_capture_barrier(platform);
        return result;
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
    void paint(SkCanvas& canvas, PlatformServices& platform) {
        prepare_overlay_layout();
        enforce_new_modal_capture_barrier(platform);
        tree_.paint(canvas, platform);
    }

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
    [[nodiscard]] static bool same_rect(Rect a, Rect b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] std::uint64_t newest_modal_id() const noexcept {
        for (auto it = overlay_state_->entries.rbegin();
             it != overlay_state_->entries.rend(); ++it) {
            if (it->spec.mode == OverlayMode::Modal) return it->id;
        }
        return 0;
    }

    void enforce_new_modal_capture_barrier(PlatformServices& platform) {
        const auto modal_id = newest_modal_id();
        if (modal_id == 0 || modal_id <= last_modal_capture_barrier_id_) return;

        // IDs are monotonic for the UI lifetime. A newly observed modal was
        // created after every currently possible capture owner, so that owner
        // is necessarily below the new modal in creation-order stacking. Mark
        // the modal observed before invoking PointerCancel because that callback
        // is allowed to show/close overlays reentrantly.
        last_modal_capture_barrier_id_ = modal_id;
        (void)tree_.cancel_pointer(platform);
    }

    [[nodiscard]] bool synchronize_overlay_anchors() {
        bool changed = false;
        std::vector<std::uint64_t> stale;

        for (auto& entry : overlay_state_->entries) {
            if (!entry.spec.anchor) continue;

            const auto availability = tree_.component_availability(*entry.spec.anchor);
            const auto bounds = tree_.overlay_anchor_bounds(*entry.spec.anchor);
            if (!availability || !bounds ||
                availability->visibility != VisibilityMode::Visible) {
                stale.push_back(entry.id);
                continue;
            }

            if (!entry.anchor_bounds || !same_rect(*entry.anchor_bounds, *bounds)) {
                entry.anchor_bounds = *bounds;
                changed = true;
            }
        }

        for (const auto id : stale) {
            changed = overlay_state_->close_id(id) || changed;
        }

        if (changed) tree_.invalidate_layout();
        return changed;
    }

    void prepare_overlay_layout() {
        // First pass makes root/anchor geometry authoritative for this viewport
        // and flushes pending T058 structural mutations. The overlay-specific
        // layout path deliberately skips a redundant recursive measurement of
        // child 0, whose full-viewport bounds are independent of its preferred
        // size. A changed/missing anchor then invalidates overlay placement or
        // closes the overlay; the second pass consumes that update before input
        // or paint observes it.
        tree_.layout_overlay_viewport(viewport_);
        if (synchronize_overlay_anchors()) tree_.layout_overlay_viewport(viewport_);
    }

    void close_anchored_overlays() {
        std::vector<std::uint64_t> anchored;
        anchored.reserve(overlay_state_->entries.size());
        for (const auto& entry : overlay_state_->entries) {
            if (entry.spec.anchor) anchored.push_back(entry.id);
        }
        for (const auto id : anchored) (void)overlay_state_->close_id(id);
    }

    // State must outlive Tree because the retained OverlayHost clears its T058
    // structural invalidator during tree teardown.
    std::shared_ptr<detail::OverlayState> overlay_state_;
    Tree tree_;
    Size viewport_{};
    std::uint64_t last_modal_capture_barrier_id_{};
};

using PluginUI = UI; // compatibility alias for the original POC

} // namespace ui
