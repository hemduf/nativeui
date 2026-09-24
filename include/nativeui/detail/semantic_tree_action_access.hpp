#pragma once

#include <nativeui/component_tree.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>
#include <nativeui/detail/semantic_tree.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui::detail {

/// Narrow internal access seam from a native-view semantic endpoint to one
/// retained Tree. The caller must already be on the owning UI thread.
///
/// Stable semantic identities cross dispatcher checkpoints; Tree/Node/Component
/// pointers never do. Operations borrow the retained tree only for the duration
/// of one UI-thread call and delegate lifecycle/current-state validation to the
/// T068 Tree seam.
struct SemanticTreeActionAccess final {
    /// Build one detached immutable semantic candidate from the committed
    /// retained tree. Lifecycle transitions fail closed so a native bridge never
    /// publishes a half-mutated retained state. The returned snapshot owns only
    /// values/shared immutable T067 metadata; it retains no live tree pointer.
    [[nodiscard]] static std::optional<SemanticTreeSnapshot> build_snapshot(
        const Tree& tree) {
        if (!tree.root_ || tree.lifecycle_transition_active()) {
            return std::nullopt;
        }
        return build_semantic_tree_snapshot(
            *tree.root_, tree.semantic_action_focused_node_id());
    }

    [[nodiscard]] static std::optional<SemanticInfo> current_semantics(
        const Tree& tree,
        const SemanticIdentity& identity) {
        return tree.current_semantics_for_action(identity);
    }

    [[nodiscard]] static bool dispatch_semantic_action(
        Tree& tree,
        const SemanticIdentity& identity,
        const SemanticActionRequest& request) {
        // The component callback may synchronously remove its subtree. Return
        // directly so this adapter never observes Tree state after dispatch.
        return tree.dispatch_semantic_action(identity, request);
    }
};

/// Production binding seam between one native-view semantic domain and its
/// retained Tree. ViewCore supplies the existing T065 Dispatcher plus the weak
/// lifetime token of the object that owns `tree`; this helper deliberately adds
/// no scheduler, queue, or ownership of retained state.
///
/// The target callbacks retain only a borrowed Tree reference guarded by the
/// supplied owner lifetime. The owner must publish lifetime death before Tree
/// teardown, and the native view must call SemanticNativeViewBridge::shutdown()
/// before its own endpoint is destroyed. This is the same ordering required by
/// LifetimeBoundSemanticActionTarget and keeps raw Node/Component pointers from
/// crossing a Dispatcher checkpoint.
struct SemanticRetainedViewBinding final {
    static void bind_actions(
        SemanticNativeViewBridge& bridge,
        Dispatcher dispatcher,
        std::weak_ptr<const void> owner_lifetime,
        Tree& tree) {
        bridge.bind_lifetime_actions(
            std::move(dispatcher),
            std::move(owner_lifetime),
            [&tree](const SemanticIdentity& requested) {
                return SemanticTreeActionAccess::current_semantics(tree, requested);
            },
            [&tree](const SemanticIdentity& requested,
                    const SemanticActionRequest& request) {
                return SemanticTreeActionAccess::dispatch_semantic_action(
                    tree, requested, request);
            });
    }

    /// Stage the latest committed retained state for the native view's next
    /// coalescing checkpoint. Lifecycle transitions fail closed and leave any
    /// previously staged candidate untouched. This lower-level seam deliberately
    /// has no owner-lifetime policy; production view ownership goes through
    /// SemanticRetainedViewDomain, which guards re-entrant teardown around the
    /// retained semantic projection.
    [[nodiscard]] static bool stage_snapshot(
        SemanticNativeViewBridge& bridge,
        const Tree& tree) {
        auto candidate = SemanticTreeActionAccess::build_snapshot(tree);
        if (!candidate) return false;
        bridge.stage(std::move(*candidate));
        return true;
    }
};

/// Stateful per-view owner for the retained semantic bridge.
///
/// This is the object a real ViewCore can keep next to its native endpoint: it
/// binds exactly one retained Tree to the existing per-window T065 Dispatcher,
/// keeps query/action identity in one SemanticNativeViewBridge, and centralizes
/// terminal teardown. The referenced Tree must outlive this domain; the supplied
/// weak owner lifetime is the independent guard that makes already queued action
/// work fail closed before Tree destruction.
///
/// No retained Node/Component pointer is stored here and no queue/scheduler is
/// introduced. Checkpoints observe owner lifetime before and after the UI-thread
/// Tree projection without keeping the token alive across component semantic
/// callbacks. Re-entrant owner death therefore retires the endpoint before a
/// candidate can be staged or published.
class SemanticRetainedViewDomain final {
public:
    SemanticRetainedViewDomain(
        Dispatcher dispatcher,
        std::weak_ptr<const void> owner_lifetime,
        Tree& tree)
        : owner_lifetime_(owner_lifetime), tree_(tree) {
        // Construction is itself a lifetime boundary. A view whose owner token
        // is already dead must never expose a transiently live publisher/action
        // endpoint and wait for a later pump to discover teardown.
        if (owner_lifetime_.expired()) {
            bridge_.shutdown();
            return;
        }

        SemanticRetainedViewBinding::bind_actions(
            bridge_,
            std::move(dispatcher),
            std::move(owner_lifetime),
            tree_);
    }

    ~SemanticRetainedViewDomain() noexcept {
        shutdown();
    }

    SemanticRetainedViewDomain(const SemanticRetainedViewDomain&) = delete;
    SemanticRetainedViewDomain& operator=(const SemanticRetainedViewDomain&) = delete;
    SemanticRetainedViewDomain(SemanticRetainedViewDomain&&) = delete;
    SemanticRetainedViewDomain& operator=(SemanticRetainedViewDomain&&) = delete;

    [[nodiscard]] SemanticNativeViewBridge& bridge() noexcept {
        return bridge_;
    }

    [[nodiscard]] const SemanticNativeViewBridge& bridge() const noexcept {
        return bridge_;
    }

    /// Publish the current committed retained state at the caller's existing UI
    /// checkpoint. The returned batch owns the exact immutable generation that
    /// produced its change categories, allowing platform notification adapters
    /// to avoid a separate `current()` lookup after publication. Shutdown and
    /// lifecycle transitions fail closed.
    ///
    /// The owner is deliberately observed weakly on both sides of snapshot
    /// projection: a custom semantic callback may synchronously tear down the
    /// surrounding view, and that death must be visible before any candidate is
    /// staged/published.
    [[nodiscard]] std::optional<SemanticPublicationBatch> checkpoint_publication() {
        if (!stage_current_snapshot()) {
            return std::nullopt;
        }
        return bridge_.checkpoint_publication();
    }

    [[nodiscard]] bool has_pending_native_publication() const noexcept {
        return bridge_.has_pending_native_publication();
    }

    /// Retry one already committed semantic/native batch without sampling newer
    /// geometry. This is used after the native wrapper allocation failed: the
    /// pending batch owns the exact geometry from its original checkpoint and
    /// must commit before a newer retained candidate can be projected.
    [[nodiscard]] std::optional<SemanticNativePublicationBatch>
    retry_pending_native_publication() {
        if (!bridge_.publisher()) {
            return std::nullopt;
        }
        if (owner_lifetime_.expired()) {
            shutdown();
            return std::nullopt;
        }
        return bridge_.retry_pending_native_publication();
    }

    /// Publish the current committed retained state together with the exact
    /// native geometry captured by the owning ViewCore at this pump checkpoint.
    /// Platform callbacks can retain the returned immutable pair without ever
    /// reading live geometry or the retained Tree cross-thread.
    ///
    /// A native wrapper publication that previously failed allocation is retried
    /// before projecting newer retained state. This preserves both the exact
    /// notification batch and the geometry that belonged to that already
    /// committed semantic generation; the newer candidate remains for a later
    /// checkpoint after recovery.
    [[nodiscard]] std::optional<SemanticNativePublicationBatch>
    checkpoint_native_publication(SemanticNativeGeometry geometry) {
        if (!bridge_.publisher()) {
            return std::nullopt;
        }
        if (owner_lifetime_.expired()) {
            shutdown();
            return std::nullopt;
        }

        if (bridge_.has_pending_native_publication()) {
            return bridge_.retry_pending_native_publication();
        }

        if (!stage_current_snapshot()) {
            return std::nullopt;
        }
        return bridge_.checkpoint_native_publication(geometry);
    }

    /// Compatibility form used by retained tests/callers that need only the
    /// notification categories. Native platform adapters should consume
    /// checkpoint_publication() so a notification batch cannot drift from its
    /// immutable semantic generation.
    [[nodiscard]] std::optional<std::vector<SemanticChange>> checkpoint_snapshot() {
        auto publication = checkpoint_publication();
        if (!publication) {
            return std::nullopt;
        }
        return std::move(publication->changes);
    }

    /// Publish endpoint death before the surrounding native/retained view starts
    /// destruction. Idempotence and terminal non-resurrection are provided by
    /// SemanticNativeViewBridge::shutdown().
    void shutdown() noexcept {
        bridge_.shutdown();
    }

private:
    [[nodiscard]] bool stage_current_snapshot() {
        if (!bridge_.publisher()) {
            return false;
        }
        if (owner_lifetime_.expired()) {
            shutdown();
            return false;
        }

        auto candidate = SemanticTreeActionAccess::build_snapshot(tree_);
        if (!candidate) {
            return false;
        }

        if (!bridge_.publisher() || owner_lifetime_.expired()) {
            shutdown();
            return false;
        }

        bridge_.stage(std::move(*candidate));
        return true;
    }

    std::weak_ptr<const void> owner_lifetime_;
    Tree& tree_;
    SemanticNativeViewBridge bridge_;
};

/// Lifecycle-safe production checkpoint seam for a native view.
///
/// T065 callbacks always drain exactly once at the native-view pump boundary,
/// independent of whether the semantic endpoint is still alive. Semantic/view
/// lifetime gates only the subsequent retained projection/publication: retiring
/// accessibility must never strand unrelated work that was already accepted by
/// the owning dispatcher.
///
/// The dispatcher owner is held strongly for the drain, and the semantic domain
/// is copied into an independent shared lease before that drain begins. A T065
/// callback may therefore release the surrounding view's own domain reference
/// without invalidating this checkpoint stack frame. Owner death published
/// before or during the drain makes publication fail closed after the drain.
/// This helper adds no queue or scheduler; it only codifies ordering/lifetime at
/// an existing UI-thread checkpoint.
struct SemanticRetainedViewCheckpoint final {
    [[nodiscard]] static std::optional<SemanticPublicationBatch> drain_and_publish(
        const std::shared_ptr<DispatcherOwner>& dispatcher_owner,
        std::weak_ptr<const void> view_lifetime,
        std::shared_ptr<SemanticRetainedViewDomain> domain) {
        if (!dispatcher_owner) {
            return std::nullopt;
        }

        (void)dispatcher_owner->checkpoint();
        if (!domain || view_lifetime.expired()) {
            return std::nullopt;
        }

        return domain->checkpoint_publication();
    }

    /// Native-reader form of the production pump seam for callers that already
    /// own a copied geometry value. Accepted T065 work drains first; the copied
    /// value is then committed with the resulting retained state. New platform
    /// wiring should prefer drain_and_publish_native_with_capture() so geometry
    /// that can change during accepted dispatcher work is sampled afterwards.
    [[nodiscard]] static std::optional<SemanticNativePublicationBatch>
    drain_and_publish_native(
        const std::shared_ptr<DispatcherOwner>& dispatcher_owner,
        std::weak_ptr<const void> view_lifetime,
        std::shared_ptr<SemanticRetainedViewDomain> domain,
        SemanticNativeGeometry geometry) {
        return drain_and_publish_native_with_capture(
            dispatcher_owner,
            std::move(view_lifetime),
            std::move(domain),
            [geometry]() noexcept { return geometry; });
    }

    /// Native-reader production seam when geometry belongs to the owning view.
    /// The capture callable is invoked exactly once, only after all accepted T065
    /// work has drained and only if the independent native-view lifetime still
    /// survives. A durable native batch awaiting retry is the sole exception:
    /// it already owns its exact geometry, so retry commits that batch without
    /// invoking the newer platform capture. This ordering prevents a stale
    /// pre-dispatch origin/scale from being paired with post-dispatch retained
    /// semantics while avoiding an unnecessary native query during recovery.
    ///
    /// The callable must return a copied SemanticNativeGeometry and must not
    /// retain live Tree/Node/Component state. No queue, scheduler or fallback
    /// synchronous dispatch is introduced by this seam.
    template <class GeometryCapture>
    [[nodiscard]] static std::optional<SemanticNativePublicationBatch>
    drain_and_publish_native_with_capture(
        const std::shared_ptr<DispatcherOwner>& dispatcher_owner,
        std::weak_ptr<const void> view_lifetime,
        std::shared_ptr<SemanticRetainedViewDomain> domain,
        GeometryCapture&& capture_geometry) {
        if (!dispatcher_owner) {
            return std::nullopt;
        }

        (void)dispatcher_owner->checkpoint();
        if (!domain || view_lifetime.expired()) {
            return std::nullopt;
        }

        if (domain->has_pending_native_publication()) {
            return domain->retry_pending_native_publication();
        }

        return domain->checkpoint_native_publication(
            std::forward<GeometryCapture>(capture_geometry)());
    }
};

} // namespace ui::detail
