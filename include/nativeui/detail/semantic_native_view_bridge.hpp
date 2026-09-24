#pragma once

#include <nativeui/detail/semantic_action_target_binding.hpp>
#include <nativeui/detail/semantic_action_view_binding.hpp>
#include <nativeui/detail/semantic_native_publication.hpp>
#include <nativeui/detail/semantic_view_state.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui::detail {

/// Per-native-view semantic bridge shared by platform accessibility adapters.
///
/// One bridge owns both the immutable semantic publication state and the
/// action endpoint for exactly one native view. Native adapters therefore
/// cannot accidentally construct an action router from another view's
/// snapshot publisher. The action endpoint still posts through the supplied
/// Dispatcher; this class adds no queue or scheduler of its own.
class SemanticNativeViewBridge final {
public:
    SemanticNativeViewBridge() = default;

    /// Publish action/query endpoint death before member destruction tears down
    /// the target/Dispatcher or immutable publication owner.
    ~SemanticNativeViewBridge() noexcept {
        shutdown();
    }

    SemanticNativeViewBridge(const SemanticNativeViewBridge&) = delete;
    SemanticNativeViewBridge& operator=(const SemanticNativeViewBridge&) = delete;
    SemanticNativeViewBridge(SemanticNativeViewBridge&&) = delete;
    SemanticNativeViewBridge& operator=(SemanticNativeViewBridge&&) = delete;

    /// Prepare a replacement endpoint before invalidating the currently bound
    /// one. If endpoint construction throws, the existing binding is left
    /// intact. Once preparation succeeds, reset publishes endpoint death before
    /// the replacement becomes reachable, so already queued work from the old
    /// binding cannot dispatch through the new target. A terminally shut-down
    /// bridge never accepts a replacement endpoint.
    void bind_actions(Dispatcher dispatcher,
                      std::shared_ptr<SemanticActionTarget> target) {
        if (!view_state_.active()) return;
        SemanticActionViewBinding prepared{
            std::move(dispatcher), std::move(target)};
        action_binding_.reset();
        action_binding_ = std::move(prepared);
    }

    /// Bind the production live-state callback seam to an owning view lifetime.
    ///
    /// ViewCore can use this overload without separately retaining a target
    /// object: the bridge owns the target while the target itself observes only
    /// the owner's weak lifetime token. The resolver callbacks may borrow the
    /// retained tree for one UI-thread call, but become unreachable before that
    /// owner begins teardown. Construction is prepare-then-commit: allocation or
    /// callback-storage failure leaves the previous action endpoint intact.
    void bind_lifetime_actions(
        Dispatcher dispatcher,
        std::weak_ptr<const void> owner_lifetime,
        LifetimeBoundSemanticActionTarget::CurrentSemanticsResolver current_semantics,
        LifetimeBoundSemanticActionTarget::ActionDispatcher dispatch_action) {
        if (!view_state_.active()) return;
        auto prepared_target = std::make_shared<LifetimeBoundSemanticActionTarget>(
            std::move(owner_lifetime),
            std::move(current_semantics),
            std::move(dispatch_action));
        bind_actions(std::move(dispatcher), std::move(prepared_target));
    }

    /// Publish endpoint death before native-view teardown. Snapshot readers are
    /// intentionally independent: immutable generations already retained by an
    /// OS callback remain readable until their normal shared ownership ends,
    /// while routers become inert immediately.
    void unbind_actions() noexcept {
        action_binding_.reset();
    }

    /// Permanently retire this native view's semantic domain. Invalidate the
    /// mutation endpoint first, then the native-reader publication and finally
    /// the logical semantic publisher. Readers that already retained either
    /// immutable object stay valid through normal shared ownership. Pending
    /// native notification work is terminally discarded because a closed view
    /// must never resurrect later.
    void shutdown() noexcept {
        action_binding_.reset();
        pending_native_publication_.reset();
        native_publication_state_.shutdown();
        view_state_.shutdown();
    }

    [[nodiscard]] std::shared_ptr<SemanticSnapshotPublisher> publisher() const noexcept {
        return view_state_.publisher();
    }

    [[nodiscard]] std::shared_ptr<const SemanticTreeSnapshot> current() const noexcept {
        return view_state_.current();
    }

    /// Exact immutable semantic+native-geometry pair for platform readers.
    /// Native callbacks retain this one object instead of loading semantic data
    /// and coordinate state independently.
    [[nodiscard]] std::shared_ptr<const SemanticNativePublicationSnapshot>
    native_current() const noexcept {
        return native_publication_state_.current();
    }

    /// Lifetime-safe endpoint for lazy platform proxy reads. The bridge owns the
    /// source; proxies retain only the weak value returned here. shutdown()
    /// clears the source before retained/native teardown, while an already
    /// loaded immutable publication remains independently readable.
    [[nodiscard]] std::weak_ptr<const SemanticNativePublicationSource>
    native_reader_source() const noexcept {
        return native_publication_state_.reader_source();
    }

    /// Mint a query proxy from this native view's immutable publisher. Platform
    /// accessibility adapters use this seam instead of accepting an arbitrary
    /// publisher, keeping query identity scoped to the same view as actions.
    [[nodiscard]] SemanticSnapshotProxy make_ordinary_proxy(
        SemanticId node_id) const {
        return SemanticSnapshotProxy::ordinary(view_state_.publisher(), node_id);
    }

    /// Mint a lazy logical-item query proxy from this native view only. The
    /// proxy resolves through the immutable token index and never materializes
    /// a visual row or native object eagerly.
    [[nodiscard]] SemanticSnapshotProxy make_virtual_proxy(
        SemanticId list_node_id,
        VirtualSemanticItemToken token) const {
        return SemanticSnapshotProxy::virtual_item(
            view_state_.publisher(), list_node_id, token);
    }

    void stage(SemanticTreeSnapshot candidate) {
        view_state_.stage(std::move(candidate));
    }

    [[nodiscard]] bool has_pending_publication() const noexcept {
        return view_state_.has_pending_publication();
    }

    [[nodiscard]] bool has_pending_native_publication() const noexcept {
        return pending_native_publication_.has_value();
    }

    /// Platform notification adapters use this form so the change categories
    /// are retained together with the exact immutable generation that produced
    /// them. No second `current()` lookup is required at the native boundary.
    [[nodiscard]] std::optional<SemanticPublicationBatch> checkpoint_publication() {
        return view_state_.checkpoint_publication();
    }

    /// Retry the exact durable native batch after a fallible wrapper publication.
    /// The pending record already owns both the committed semantic generation and
    /// the geometry captured for it, so retry deliberately takes no replacement
    /// geometry and performs no platform sampling. Newer retained/native state is
    /// handled only after this exact batch commits.
    [[nodiscard]] std::optional<SemanticNativePublicationBatch>
    retry_pending_native_publication() {
        if (!pending_native_publication_) {
            return std::nullopt;
        }
        return commit_pending_native_publication();
    }

    /// Commit one native-reader publication at the same per-view checkpoint.
    ///
    /// A semantic publication commits before the native wrapper is allocated.
    /// Therefore the exact semantic batch is first moved into durable per-view
    /// pending state. If native allocation fails, the previous native snapshot
    /// remains authoritative and the semantic batch is retried on the next
    /// checkpoint instead of silently losing its notifications. A newer staged
    /// semantic candidate remains in SemanticViewState until that retry commits.
    [[nodiscard]] std::optional<SemanticNativePublicationBatch>
    checkpoint_native_publication(SemanticNativeGeometry geometry) {
        if (pending_native_publication_) {
            return retry_pending_native_publication();
        }

        auto semantic = view_state_.checkpoint_publication();
        if (!semantic) {
            return std::nullopt;
        }

        pending_native_publication_.emplace(
            PendingNativePublication{std::move(*semantic), geometry});
        return commit_pending_native_publication();
    }

    [[nodiscard]] std::vector<SemanticChange> checkpoint() {
        return view_state_.checkpoint();
    }

    [[nodiscard]] std::optional<SemanticPublicationBatch> publish_publication(
        SemanticTreeSnapshot candidate) {
        return view_state_.publish_publication(std::move(candidate));
    }

    [[nodiscard]] std::vector<SemanticChange> publish(
        SemanticTreeSnapshot candidate) {
        return view_state_.publish(std::move(candidate));
    }

    /// Deterministic per-view fault seam for the transactional native
    /// publication tests. Production code never needs this control.
    void fail_next_native_publish_at_for_test(
        SemanticNativePublicationState::FailurePointForTest point) noexcept {
        native_publication_state_.fail_next_publish_at_for_test(point);
    }

    /// Build an ordinary-node router from this view's publisher only. Platform
    /// code never supplies an arbitrary SemanticSnapshotProxy at this seam.
    [[nodiscard]] SemanticActionRouter make_ordinary_router(
        SemanticId node_id) const {
        return action_binding_.make_router(make_ordinary_proxy(node_id));
    }

    /// Build a logical virtual-item router from this view's indexed immutable
    /// metadata. The stable token is re-resolved again on the UI thread by the
    /// live target before a mutation is allowed.
    [[nodiscard]] SemanticActionRouter make_virtual_router(
        SemanticId list_node_id,
        VirtualSemanticItemToken token) const {
        return action_binding_.make_router(make_virtual_proxy(list_node_id, token));
    }

private:
    struct PendingNativePublication final {
        SemanticPublicationBatch semantic;
        SemanticNativeGeometry geometry;
    };

    [[nodiscard]] std::optional<SemanticNativePublicationBatch>
    commit_pending_native_publication() {
        if (!pending_native_publication_) {
            return std::nullopt;
        }

        const auto& pending = *pending_native_publication_;
        auto publication = native_publication_state_.publish(
            pending.semantic.snapshot,
            pending.semantic.changes,
            pending.geometry);
        if (publication) {
            pending_native_publication_.reset();
        }
        return publication;
    }

    SemanticViewState view_state_;
    SemanticNativePublicationState native_publication_state_;
    std::optional<PendingNativePublication> pending_native_publication_;
    SemanticActionViewBinding action_binding_;
};

} // namespace ui::detail
