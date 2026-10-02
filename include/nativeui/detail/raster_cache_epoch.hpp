#pragma once

#include <cstdint>
#include <limits>
#include <memory>

namespace ui::detail {

struct RasterCacheEpochTestAccess;

// UI-thread-confined content identity, not ownership of a rendered image.
// Tokens contain no node, Tree, renderer or graphics-context pointer.
class RasterCacheEpoch final {
    struct Identity final {};

public:
    class Token final {
    public:
        Token() noexcept = default;
        [[nodiscard]] bool expired() const noexcept { return identity_.expired(); }

        [[nodiscard]] std::uint64_t generation() const noexcept {
            return generation_;
        }

        [[nodiscard]] bool same_lifetime(const Token& other) const noexcept {
            return !identity_.owner_before(other.identity_) &&
                   !other.identity_.owner_before(identity_);
        }

        [[nodiscard]] bool same_content(const Token& other) const noexcept {
            return generation_ == other.generation_ && same_lifetime(other);
        }

    private:
        friend class RasterCacheEpoch;
        Token(const std::shared_ptr<const Identity>& identity,
              std::uint64_t generation) noexcept
            : identity_(identity), generation_(generation) {}

        std::weak_ptr<const Identity> identity_;
        std::uint64_t generation_{};
    };

    RasterCacheEpoch() : identity_(std::make_shared<const Identity>()) {}
    RasterCacheEpoch(const RasterCacheEpoch&) = delete;
    RasterCacheEpoch& operator=(const RasterCacheEpoch&) = delete;
    RasterCacheEpoch(RasterCacheEpoch&&) = delete;
    RasterCacheEpoch& operator=(RasterCacheEpoch&&) = delete;
    ~RasterCacheEpoch() noexcept = default;

    [[nodiscard]] bool stale() const noexcept { return stale_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

    void invalidate() noexcept {
        if (exhausted_) return;
        // Coalesce only before a consumer has captured this generation. Once
        // painting has started, even an already-stale boundary needs a new
        // generation so the in-flight result cannot swallow newer damage.
        if (!stale_ || captured_) {
            if (generation_ == (std::numeric_limits<std::uint64_t>::max)()) {
                exhausted_ = true;
            } else {
                ++generation_;
            }
        }
        stale_ = true;
        captured_ = false;
    }

    [[nodiscard]] Token capture() noexcept {
        if (exhausted_) return {};
        captured_ = true;
        return Token{identity_, generation_};
    }

    [[nodiscard]] Token lifetime_token() const noexcept {
        return Token{identity_, 0};
    }

    [[nodiscard]] bool same_lifetime(const Token& token) const noexcept {
        // Control-block identity, not the reusable address of a destroyed node.
        return token.same_lifetime(Token{identity_, generation_});
    }

    [[nodiscard]] bool reusable(const Token& token) const noexcept {
        return !stale_ && current(token);
    }

    [[nodiscard]] bool committable(const Token& token) const noexcept {
        return captured_ && current(token);
    }

    [[nodiscard]] bool commit(const Token& token) noexcept {
        if (!captured_ || !current(token)) return false;
        stale_ = false;
        // Keep captured_ set: another consumer may publish the same generation,
        // but any subsequent invalidation must revoke all outstanding tokens.
        return true;
    }

private:
    friend struct RasterCacheEpochTestAccess;

    [[nodiscard]] bool current(const Token& token) const noexcept {
        return !exhausted_ && token.generation_ == generation_ && same_lifetime(token);
    }

    std::shared_ptr<const Identity> identity_;
    std::uint64_t generation_{1};
    bool stale_{true};
    bool captured_{};
    bool exhausted_{};
};

} // namespace ui::detail
