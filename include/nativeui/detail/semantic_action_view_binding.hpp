#pragma once

#include <nativeui/detail/semantic_action.hpp>

#include <memory>
#include <utility>

namespace ui::detail {

/// Lifetime-safe per-native-view endpoint for platform accessibility actions.
///
/// Native adapters retain this object weakly and submit only stable semantic
/// identity plus a typed request. The endpoint never owns retained Tree/Node/
/// Component state: it weakly observes the view publisher/target and reuses the
/// existing SemanticActionRouter/T065 path for snapshot and live-state
/// revalidation before any mutation can run.
class SemanticActionViewEndpoint final {
public:
    SemanticActionViewEndpoint(
        std::weak_ptr<SemanticSnapshotPublisher> publisher,
        Dispatcher dispatcher,
        std::weak_ptr<SemanticActionTarget> target,
        std::weak_ptr<const void> endpoint_lifetime)
        : publisher_(std::move(publisher)),
          dispatcher_(std::move(dispatcher)),
          target_(std::move(target)),
          endpoint_lifetime_(std::move(endpoint_lifetime)) {}

    [[nodiscard]] bool post(
        const SemanticIdentity& identity,
        SemanticActionRequest request) const noexcept {
        try {
            if (identity.node_id == kInvalidSemanticId ||
                (identity.virtual_token.has_value() &&
                 *identity.virtual_token == kInvalidVirtualSemanticItemToken) ||
                endpoint_lifetime_.expired()) {
                return false;
            }

            const auto publisher = publisher_.lock();
            const auto target = target_.lock();
            if (!publisher || !target) {
                return false;
            }

            auto proxy = identity.virtual_token.has_value()
                ? SemanticSnapshotProxy::virtual_item(
                      publisher, identity.node_id, *identity.virtual_token)
                : SemanticSnapshotProxy::ordinary(publisher, identity.node_id);

            return SemanticActionRouter{
                std::move(proxy), dispatcher_, target, endpoint_lifetime_}
                .post(std::move(request));
        } catch (...) {
            // Platform accessibility callbacks are foreign ABI boundaries. A
            // failed allocation/proxy construction/post must fail closed rather
            // than escape into AppKit/UIA/AT-SPI2.
            return false;
        }
    }

private:
    std::weak_ptr<SemanticSnapshotPublisher> publisher_;
    Dispatcher dispatcher_;
    std::weak_ptr<SemanticActionTarget> target_;
    std::weak_ptr<const void> endpoint_lifetime_;
};

/// Per-native-view owner for dispatcher-backed semantic action routing.
///
/// The owning native view keeps exactly one binding alive. Native routers keep
/// only weak references to both this binding's endpoint lifetime and its
/// SemanticActionTarget. reset() publishes endpoint death before releasing the
/// target/Dispatcher, so already-queued work becomes a no-op and nested/future
/// requests fail closed even while an in-flight callback still strongly owns
/// the target. This object stores no live Node/Component pointer and introduces
/// no queue beyond the owning Dispatcher.
class SemanticActionViewBinding final {
public:
    SemanticActionViewBinding() = default;

    SemanticActionViewBinding(
        Dispatcher dispatcher,
        std::shared_ptr<SemanticActionTarget> target,
        std::shared_ptr<SemanticSnapshotPublisher> publisher = {})
        : endpoint_lifetime_(std::make_shared<int>(0)),
          dispatcher_(std::move(dispatcher)),
          target_(std::move(target)),
          platform_endpoint_(publisher
              ? std::make_shared<SemanticActionViewEndpoint>(
                    publisher, dispatcher_, target_, endpoint_lifetime_)
              : std::shared_ptr<SemanticActionViewEndpoint>{}) {}

    SemanticActionViewBinding(const SemanticActionViewBinding&) = delete;
    SemanticActionViewBinding& operator=(const SemanticActionViewBinding&) = delete;
    SemanticActionViewBinding(SemanticActionViewBinding&&) noexcept = default;
    SemanticActionViewBinding& operator=(SemanticActionViewBinding&&) noexcept = default;

    [[nodiscard]] SemanticActionRouter make_router(
        SemanticSnapshotProxy proxy) const {
        return SemanticActionRouter{
            std::move(proxy), dispatcher_, target_, endpoint_lifetime_};
    }

    /// Weak platform-facing endpoint for this exact view. A native proxy may
    /// lock it for one callback without retaining the view binding itself.
    [[nodiscard]] std::weak_ptr<const SemanticActionViewEndpoint>
    endpoint() const noexcept {
        return platform_endpoint_;
    }

    /// Invalidate this view's semantic endpoint before retained/native teardown.
    ///
    /// Publish endpoint death first. A currently executing Dispatcher callback
    /// or platform callback may still hold a bounded strong object lease, but all
    /// nested/future requests observe the expired lifetime token before they can
    /// enqueue or touch current semantic state.
    void reset() noexcept {
        endpoint_lifetime_.reset();
        platform_endpoint_.reset();
        target_.reset();
        dispatcher_ = Dispatcher{};
    }

private:
    std::shared_ptr<const void> endpoint_lifetime_;
    Dispatcher dispatcher_;
    std::shared_ptr<SemanticActionTarget> target_;
    std::shared_ptr<SemanticActionViewEndpoint> platform_endpoint_;
};

} // namespace ui::detail
