#pragma once

#include <nativeui/state.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ui::detail {

struct StateDependencyAccess final {
    template <StateValue T>
    [[nodiscard]] static const void* identity(const Binding<T>& source) noexcept {
        return source.control_.get();
    }
};

// Type erasure retains State's control block, never the State object or a copy
// of its value. Every mounted boundary owns a separate RAII subscription.
class CachedLayerDependency final {
public:
    class Subscription {
    public:
        virtual ~Subscription() = default;
    };

    template <StateValue T>
    explicit CachedLayerDependency(Binding<T> source)
        : source_(std::make_shared<const Source<T>>(std::move(source))) {}

    [[nodiscard]] const void* identity() const noexcept { return source_->identity(); }
    [[nodiscard]] std::uint64_t revision() const noexcept { return source_->revision(); }

    [[nodiscard]] std::unique_ptr<Subscription> observe(std::function<void()> invalidate) const {
        return source_->observe(std::move(invalidate));
    }

private:
    class SourceBase {
    public:
        virtual ~SourceBase() = default;
        [[nodiscard]] virtual const void* identity() const noexcept = 0;
        [[nodiscard]] virtual std::uint64_t revision() const noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<Subscription> observe(
            std::function<void()> invalidate) const = 0;
    };

    template <StateValue T>
    class Source final : public SourceBase {
        class OwnedSubscription final : public Subscription {
        public:
            explicit OwnedSubscription(typename Binding<T>::Subscription value)
                : value_(std::move(value)) {}
        private:
            typename Binding<T>::Subscription value_;
        };

    public:
        explicit Source(Binding<T> source) : source_(std::move(source)) {}
        [[nodiscard]] const void* identity() const noexcept override {
            return StateDependencyAccess::identity(source_);
        }
        [[nodiscard]] std::uint64_t revision() const noexcept override {
            return source_.revision();
        }
        [[nodiscard]] std::unique_ptr<Subscription> observe(
            std::function<void()> invalidate) const override {
            auto source = source_;
            auto subscription = source.observe(
                [invalidate = std::move(invalidate)](const T&) { invalidate(); });
            return std::make_unique<OwnedSubscription>(std::move(subscription));
        }

    private:
        Binding<T> source_;
    };

    std::shared_ptr<const SourceBase> source_;
};

// Immutable construction metadata. Consecutive annotations retain their own
// declaration and ordering; they never become the runtime cache identity.
struct CachedLayerSource final {
    std::vector<CachedLayerDependency> dependencies;
    std::shared_ptr<const CachedLayerSource> inner;
};

} // namespace ui::detail
