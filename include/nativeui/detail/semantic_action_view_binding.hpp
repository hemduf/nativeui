#pragma once

#include <nativeui/detail/semantic_action.hpp>

#include <memory>
#include <utility>

namespace ui::detail {

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
        std::shared_ptr<SemanticActionTarget> target)
        : endpoint_lifetime_(std::make_shared<int>(0)),
          dispatcher_(std::move(dispatcher)),
          target_(std::move(target)) {}

    SemanticActionViewBinding(const SemanticActionViewBinding&) = delete;
    SemanticActionViewBinding& operator=(const SemanticActionViewBinding&) = delete;
    SemanticActionViewBinding(SemanticActionViewBinding&&) noexcept = default;
    SemanticActionViewBinding& operator=(SemanticActionViewBinding&&) noexcept = default;

    [[nodiscard]] SemanticActionRouter make_router(
        SemanticSnapshotProxy proxy) const {
        return SemanticActionRouter{
            std::move(proxy), dispatcher_, target_, endpoint_lifetime_};
    }

    /// Invalidate this view's semantic endpoint before retained/native teardown.
    ///
    /// Publish endpoint death first. A currently executing Dispatcher callback
    /// may still hold the target strongly, but its nested routers observe the
    /// expired endpoint token immediately and accepted-but-not-started work also
    /// checks the token before touching the current snapshot/live target.
    void reset() noexcept {
        endpoint_lifetime_.reset();
        target_.reset();
        dispatcher_ = Dispatcher{};
    }

private:
    std::shared_ptr<const void> endpoint_lifetime_;
    Dispatcher dispatcher_;
    std::shared_ptr<SemanticActionTarget> target_;
};

} // namespace ui::detail