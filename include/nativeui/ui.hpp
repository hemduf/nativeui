#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/dialog_state.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/detail/overlay_service.hpp>
#include <nativeui/overlay.hpp>
#include <nativeui/theme.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

class Dialog;

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
        : dialog_state_(std::make_shared<detail::DialogState>()),
          overlay_state_(std::make_shared<detail::OverlayState>()),
          overlay_presenter_(overlay_state_),
          tree_(compile(detail::make_overlay_host_spec(
              make_spec(std::forward<Root>(root)), overlay_state_))) {
        tree_.set_overlay_service(&overlay_presenter_);
        tree_.set_theme(std::move(theme));
        tree_.mount();
    }

    ~UI() {
        // T063 distinguishes whole-UI teardown from explicit Dialog controller
        // destruction. Publish the terminal state before Tree/overlay members
        // begin reverse-order destruction so an outliving controller is inert
        // and never invokes an application callback after the UI lifetime.
        if (dialog_state_) dialog_state_->begin_ui_teardown();
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
        // Transient presentations such as a pending/visible Tooltip are
        // cancelled at the view lifecycle boundary before focus/hover teardown
        // so their T065 timers cannot fire into an inactive UI.
        tree_.dismiss_transient_presentations();
        // T063 deactivation suppresses the application callback and must not
        // restore focus into a UI whose platform focus is already leaving. A
        // live Dialog is therefore the one case where focus deactivation must
        // precede overlay removal. Keep the historical T061/T035 order when no
        // Dialog is active so unrelated anchored-overlay behavior is unchanged.
        const bool active_dialog = dialog_state_ && dialog_state_->active_generation != 0;
        if (active_dialog) {
            tree_.deactivate_focus(platform);
            if (dialog_state_->handle_deactivate()) prepare_overlay_layout();
            close_anchored_overlays();
            return;
        }

        close_anchored_overlays();
        tree_.deactivate_focus(platform);
    }
    void refresh_focus(PlatformServices& platform) { tree_.refresh_focus(platform); }
    EventResult dispatch(const InputEvent& event, PlatformServices& platform) {
        // T063 Escape is dialog policy, not focused-child policy. Resolve it
        // before ordinary retained routing so a focused TextInput/custom body
        // cannot consume Escape ahead of the enabled Cancel action/Dismissed
        // fallback. Keep a strong local DialogState reference because the
        // application completion may destroy this UI before handle_escape()
        // returns; no UI member is touched after a successful completion.
        if (event.type == InputType::KeyDown && event.key == Key::Escape) {
            auto dialog_state = dialog_state_;
            if (dialog_state && dialog_state->handle_escape()) {
                return EventResult::Handled;
            }
        }

        // PointerDown anywhere and Escape are global dismissal gestures for
        // transient presentations such as a pending/visible Tooltip. This is
        // deliberately independent from T061 overlay pointer policy: a
        // non-hit-test tooltip must still be cancelled without consuming the
        // event that passes through to the control underneath it.
        if (event.type == InputType::PointerDown) {
            tree_.begin_pointer_interaction();
        }
        if (event.type == InputType::PointerDown ||
            (event.type == InputType::KeyDown && event.key == Key::Escape)) {
            tree_.dismiss_transient_presentations();
        }

        // Keep the no-overlay path as close as possible to the pre-T061 UI
        // dispatch contract. T035 component requests are drained only after
        // Tree::dispatch reaches its T058 structural safe checkpoint. Capture
        // the source identity before dispatch because application providers may
        // change availability/focus while the source component is still alive.
        if (overlay_state_->entries.empty()) {
            const auto command_source = overlay_command_source(event);
            const auto result = tree_.dispatch(event, platform);
            const bool completing_dialog = has_pending_dialog_completion();
            if (!completing_dialog) {
                process_component_overlay_command(command_source, platform);
                if (!overlay_state_->entries.empty()) {
                    prepare_overlay_layout();
                    enforce_new_modal_capture_barrier(platform);
                }
            }
            // T063 application completion is intentionally the final operation
            // of this dispatch. The callback may replace or destroy this UI;
            // after it runs, return using only the local result value.
            if (completing_dialog) flush_pending_dialog_completion();
            return result;
        }

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
        // can consume it. T035 widget overlays resolve retained teardown here so
        // focus restoration is complete before returning; generic T061 overlays
        // retain their pre-T035 deferred dismissal timing.
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
                    const bool resolve_widget_teardown = it->spec.anchor &&
                        anchor_dismisses_on_tab(*it->spec.anchor);
                    const auto id = it->id;
                    (void)overlay_state_->close_id(id);
                    if (resolve_widget_teardown) prepare_overlay_layout();
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
                    const bool resolve_widget_teardown = it->spec.anchor &&
                        anchor_dismisses_on_tab(*it->spec.anchor);
                    const auto id = it->id;
                    (void)overlay_state_->close_id(id);
                    if (resolve_widget_teardown) prepare_overlay_layout();
                    return EventResult::Handled;
                }

                // Preserve the generic T061 contract: the topmost modal owns
                // Escape even when it is not auto-dismissable. T063 has already
                // handled its dedicated Cancel/Dismissed policy above.
                if (it->spec.mode == OverlayMode::Modal) {
                    return EventResult::Handled;
                }
            }
        } else if (event.type == InputType::KeyDown && event.key == Key::Tab) {
            // T035 popups close on Tab before the tree performs ordinary focus
            // traversal. The policy is anchored-component metadata rather than
            // a second popup stack or a T061-wide behavior change.
            for (auto it = overlay_state_->entries.rbegin();
                 it != overlay_state_->entries.rend(); ++it) {
                if (it->spec.anchor && anchor_dismisses_on_tab(*it->spec.anchor)) {
                    const auto id = it->id;
                    (void)overlay_state_->close_id(id);
                    prepare_overlay_layout();
                    break;
                }
                if (it->spec.mode == OverlayMode::Modal) break;
            }
        }

        const auto command_source = overlay_command_source(event);
        const auto result = tree_.dispatch(event, platform);
        const bool completing_dialog = has_pending_dialog_completion();
        if (!completing_dialog) {
            process_component_overlay_command(command_source, platform);
        }

        // Tree::dispatch may have removed/disabled an anchor while a popup was
        // open. Resolve that source transition in the same outer dispatch, not
        // on a later paint/input event.
        if (!overlay_state_->entries.empty()) prepare_overlay_layout();

        // A callback may have captured the pointer and shown a modal in the
        // same dispatch. T058 mounts that modal only after the callback stack
        // unwinds; cancel the now-lower capture here, after Tree::dispatch has
        // reached that safe checkpoint, never reentrantly inside PointerDown.
        enforce_new_modal_capture_barrier(platform);

        // Keep the application callback last for the same lifetime reason as
        // the no-overlay path above. flush_pending_dialog_completion() first
        // performs the retained detach and releases the per-UI Dialog slot.
        if (completing_dialog) flush_pending_dialog_completion();
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
    /// Read-only T045 semantic projection for one retained node. T068 replaces
    /// this diagnostic read with immutable per-view semantic snapshots.
    [[nodiscard]] std::optional<SemanticInfo> component_semantics(
        NodeId id) const noexcept {
        return tree_.component_semantics(id);
    }
    /// Read-only diagnostic snapshot of the current T061 overlay stack in
    /// creation order. Exposes only overlay policy/resolved geometry; it never
    /// returns content components or platform objects.
    [[nodiscard]] std::vector<OverlayEntryInfo> overlay_entries() const {
        std::vector<OverlayEntryInfo> entries;
        entries.reserve(overlay_state_->entries.size());
        for (const auto& entry : overlay_state_->entries) {
            OverlayEntryInfo info;
            info.id = entry.id;
            info.mode = entry.spec.mode;
            info.pointer_policy = entry.spec.pointer_policy;
            info.anchor = entry.spec.anchor;
            info.placement = entry.spec.placement;
            info.resolved = entry.resolved;
            info.bounds = entry.resolved_bounds;
            entries.push_back(info);
        }
        return entries;
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
        if (overlay_state_->entries.empty()) {
            tree_.paint(canvas, platform);
            return;
        }
        prepare_overlay_layout();
        enforce_new_modal_capture_barrier(platform);
        tree_.paint(canvas, platform);
    }

    /// Queue one in-view overlay through the same T058 structural checkpoint
    /// used by explicit dynamic containers. show/close never splice retained
    /// nodes synchronously on the caller's callback stack.
    ///
    /// Opening a T061 overlay is a global dismissal event for transient
    /// presentations (pending/visible Tooltip) in the same UI. The Tooltip's
    /// own non-hit-test presentation uses OverlayService directly and is
    /// therefore not self-dismissing.
    [[nodiscard]] OverlayHandle show_overlay(OverlaySpec overlay) {
        tree_.dismiss_transient_presentations();
        return overlay_state_->show(std::move(overlay));
    }

    /// Close an overlay handle owned by this UI. Stale, cross-UI and already
    /// closed handles are deterministic no-ops; retained teardown is deferred
    /// through T058 even though the handle becomes stale immediately.
    bool close_overlay(OverlayHandle handle) {
        return overlay_state_->close(std::move(handle));
    }

private:
    friend class Dialog;

    [[nodiscard]] static bool same_rect(Rect a, Rect b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] static Node* find_node(Node& node, NodeId id) noexcept {
        if (node.id == id) return &node;
        for (auto& child : node.children) {
            if (auto* found = find_node(*child, id)) return found;
        }
        return nullptr;
    }

    [[nodiscard]] bool anchor_dismisses_on_tab(NodeId id) const noexcept {
        if (!tree_.root_) return false;
        auto* node = find_node(*tree_.root_, id);
        if (!node) return false;
        auto* policy = dynamic_cast<detail::OverlayAnchorPolicy*>(node->component.get());
        return policy && policy->dismiss_overlay_on_tab();
    }

    [[nodiscard]] bool anchor_dismisses_when_disabled(NodeId id) const noexcept {
        if (!tree_.root_) return false;
        auto* node = find_node(*tree_.root_, id);
        if (!node) return false;
        auto* policy = dynamic_cast<detail::OverlayAnchorPolicy*>(node->component.get());
        return policy && policy->dismiss_overlay_when_disabled();
    }

    [[nodiscard]] bool anchor_dismisses_when_read_only(NodeId id) const noexcept {
        if (!tree_.root_) return false;
        auto* node = find_node(*tree_.root_, id);
        if (!node) return false;
        auto* policy = dynamic_cast<detail::OverlayAnchorPolicy*>(node->component.get());
        return policy && policy->dismiss_overlay_when_read_only();
    }

    [[nodiscard]] static bool event_may_queue_overlay_command(const InputEvent& event) noexcept {
        // T035 requests can only be produced by activation/navigation keys or a
        // completed pointer press. Keeping the ordinary PointerMove/Right-key
        // path entirely free of component discovery preserves the T051 dispatch
        // budget while command ownership stays per-view and UI-thread confined.
        if (event.type == InputType::PointerUp) return true;
        if (event.type == InputType::KeyUp) return event.key == Key::Space;
        if (event.type != InputType::KeyDown) return false;
        return event.key == Key::Down || event.key == Key::Enter || event.key == Key::Space;
    }

    [[nodiscard]] Node* focused_node() noexcept {
        if (!tree_.focus_active_ || tree_.focusables_.empty() ||
            tree_.focused_index_ >= tree_.focusables_.size()) {
            return nullptr;
        }
        return tree_.focusables_[tree_.focused_index_];
    }

    [[nodiscard]] NodeId overlay_command_source(const InputEvent& event) noexcept {
        if (!event_may_queue_overlay_command(event)) return kInvalidNodeId;
        auto* node = focused_node();
        return node ? node->id : kInvalidNodeId;
    }

    [[nodiscard]] std::optional<detail::OverlayComponentCommand>
    take_overlay_command(NodeId source_id) {
        if (source_id == kInvalidNodeId || !tree_.root_) return std::nullopt;
        auto* node = find_node(*tree_.root_, source_id);
        if (!node) return std::nullopt;
        auto* source = dynamic_cast<detail::OverlayCommandSource*>(node->component.get());
        if (!source) return std::nullopt;
        return source->take_overlay_command();
    }

    [[nodiscard]] bool node_is_focused(NodeId id) noexcept {
        const auto* node = focused_node();
        return node && node->id == id;
    }

    [[nodiscard]] bool guarded_anchor_allows_commit(
        NodeId id, bool suppress_when_read_only) const noexcept {
        if (id == kInvalidNodeId) return true;
        const auto availability = tree_.component_availability(id);
        if (!availability || availability->visibility != VisibilityMode::Visible ||
            !availability->enabled) {
            return false;
        }
        return !suppress_when_read_only || !availability->read_only;
    }

    void process_component_overlay_command(
        NodeId source_id, PlatformServices& platform) {
        auto command = take_overlay_command(source_id);
        if (!command) return;

        if (command->kind == detail::OverlayComponentCommandKind::Show) {
            auto on_shown = std::move(command->on_shown);
            const bool suppress_when_read_only = anchor_dismisses_when_read_only(source_id);
            const bool source_still_valid = node_is_focused(source_id) &&
                guarded_anchor_allows_commit(source_id, suppress_when_read_only);
            if (!source_still_valid) {
                // Application-supplied option/item providers may mutate retained
                // availability or focus while the opening event is still being
                // dispatched. Consume the already-taken request, report an
                // invalid handle so the anchor clears opener suppression, and
                // never resurrect a stale popup after focus later returns.
                if (on_shown) on_shown({});
                return;
            }

            const auto handle = show_overlay(std::move(command->overlay));
            if (on_shown) on_shown(handle);
            prepare_overlay_layout();
            enforce_new_modal_capture_barrier(platform);
            return;
        }

        const auto guard_anchor = command->guard_anchor;
        const bool suppress_when_read_only = command->suppress_when_anchor_read_only;
        auto callback = std::move(command->after_close);

        // This is the T035 commit/reentrancy checkpoint: logical close,
        // retained detach and focus/capture reconciliation all complete before
        // application state/callback code is invoked.
        (void)close_overlay(command->handle);
        prepare_overlay_layout();
        const bool allowed = guarded_anchor_allows_commit(
            guard_anchor, suppress_when_read_only);
        if (allowed && callback) callback();

        // The application callback may invalidate dynamic composition, remove
        // its anchor or open another T061 overlay directly.
        prepare_overlay_layout();
        enforce_new_modal_capture_barrier(platform);
    }

    [[nodiscard]] bool has_pending_dialog_completion() const noexcept {
        return dialog_state_ && !dialog_state_->ui_tearing_down &&
               static_cast<bool>(dialog_state_->pending_completion);
    }

    void finish_dialog_completion(
        std::uint64_t generation, std::function<void()> completion) {
        if (!dialog_state_ || !dialog_state_->release(generation)) return;
        if (completion) completion();
    }

    void complete_dialog_close(
        std::uint64_t generation, std::function<void()> completion) {
        if (!dialog_state_ || !dialog_state_->owns(generation)) return;

        // During Tree::dispatch, destroying the retained dialog subtree here
        // would invalidate the currently executing Component::input object.
        // Defer application completion until the outer dispatch checkpoint.
        if (tree_.dispatch_depth_ != 0) {
            (void)dialog_state_->defer_completion(generation, std::move(completion));
            return;
        }

        prepare_overlay_layout();
        finish_dialog_completion(generation, std::move(completion));
    }

    void flush_pending_dialog_completion() {
        if (!dialog_state_ || dialog_state_->ui_tearing_down ||
            !dialog_state_->pending_completion) {
            return;
        }

        const auto generation = dialog_state_->pending_completion_generation;
        auto completion = std::move(dialog_state_->pending_completion);
        dialog_state_->pending_completion_generation = 0;
        dialog_state_->pending_completion = {};
        prepare_overlay_layout();
        finish_dialog_completion(generation, std::move(completion));
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
                availability->visibility != VisibilityMode::Visible ||
                (!availability->enabled &&
                 anchor_dismisses_when_disabled(*entry.spec.anchor)) ||
                (availability->read_only &&
                 anchor_dismisses_when_read_only(*entry.spec.anchor))) {
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

    // Retained components may present anchor-tracking overlays from a T065
    // timer checkpoint through this borrowed seam. It deliberately bypasses
    // show_overlay()'s transient-dismissal policy because a Tooltip presenting
    // itself must not immediately cancel itself; all application/widget overlay
    // requests still flow through show_overlay()/OverlayCommandSource.
    struct OverlayPresenter final : detail::OverlayService {
        explicit OverlayPresenter(std::shared_ptr<detail::OverlayState> state)
            : state_(std::move(state)) {}

        [[nodiscard]] OverlayHandle present(OverlaySpec overlay) override {
            return state_->show(std::move(overlay));
        }

        bool dismiss(OverlayHandle handle) override {
            return state_->close(std::move(handle));
        }

        std::shared_ptr<detail::OverlayState> state_;
    };

    // Shared dialog/overlay state must outlive Tree because controllers and the
    // retained OverlayHost can keep lifetime seams until Tree teardown ends.
    // Declaration order also keeps the borrowed presenter valid while Tree
    // unmounts components that may still close their overlay during teardown.
    std::shared_ptr<detail::DialogState> dialog_state_;
    std::shared_ptr<detail::OverlayState> overlay_state_;
    OverlayPresenter overlay_presenter_;
    Tree tree_;
    Size viewport_{};
    std::uint64_t last_modal_capture_barrier_id_{};
};

using PluginUI = UI; // compatibility alias for the original POC

} // namespace ui
