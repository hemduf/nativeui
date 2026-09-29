#pragma once

#include <nativeui/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

namespace ui::detail {

class WindowSizeConstraints final {
public:
    [[nodiscard]] const std::optional<Size>& min_size() const noexcept { return min_size_; }
    [[nodiscard]] const std::optional<Size>& max_size() const noexcept { return max_size_; }

    [[nodiscard]] bool update(std::optional<Size> min_size,
                              std::optional<Size> max_size) noexcept {
        if (!valid_bound(min_size) || !valid_bound(max_size) || !ordered(min_size, max_size)) {
            return false;
        }

        min_size_ = min_size;
        max_size_ = max_size;
        return true;
    }

    [[nodiscard]] bool set_min(std::optional<Size> min_size) noexcept {
        return update(min_size, max_size_);
    }

    [[nodiscard]] bool set_max(std::optional<Size> max_size) noexcept {
        return update(min_size_, max_size);
    }

    [[nodiscard]] std::optional<Size> clamp(Size logical) const noexcept {
        if (!valid_size(logical)) return std::nullopt;

        if (min_size_) {
            logical.w = std::max(logical.w, min_size_->w);
            logical.h = std::max(logical.h, min_size_->h);
        }
        if (max_size_) {
            logical.w = std::min(logical.w, max_size_->w);
            logical.h = std::min(logical.h, max_size_->h);
        }
        return logical;
    }

private:
    [[nodiscard]] static bool valid_size(Size size) noexcept {
        return std::isfinite(size.w) && std::isfinite(size.h) &&
               size.w > 0.0f && size.h > 0.0f;
    }

    [[nodiscard]] static bool valid_bound(const std::optional<Size>& value) noexcept {
        return !value || valid_size(*value);
    }

    [[nodiscard]] static bool ordered(const std::optional<Size>& min_size,
                                      const std::optional<Size>& max_size) noexcept {
        return !min_size || !max_size ||
               (min_size->w <= max_size->w && min_size->h <= max_size->h);
    }

    std::optional<Size> min_size_;
    std::optional<Size> max_size_;
};

/// Bookkeeping for one best-effort Dispatcher post used only as an optimization
/// for window lifecycle control. Rejection or exception-before-enqueue clears
/// the posted bit and leaves the close phase itself as the durable source of
/// truth for the next owner/platform checkpoint.
class ScopedBooleanState final {
public:
    ScopedBooleanState(bool& slot, bool value) noexcept
        : slot_(slot), previous_(slot) {
        slot_ = value;
    }

    ~ScopedBooleanState() noexcept { slot_ = previous_; }

    ScopedBooleanState(const ScopedBooleanState&) = delete;
    ScopedBooleanState& operator=(const ScopedBooleanState&) = delete;
    ScopedBooleanState(ScopedBooleanState&&) = delete;
    ScopedBooleanState& operator=(ScopedBooleanState&&) = delete;

private:
    bool& slot_;
    bool previous_{};
};

/// Transactional visibility bookkeeping for synchronous native show/hide calls.
///
/// Native callbacks may re-enter show/hide before the outer platform call
/// returns. Each transition gets a monotonically increasing generation so the
/// outer caller can detect that a nested transition superseded it and must not
/// publish rollback, cleanup, or redraw work over the newer state.
class WindowVisibilityState final {
public:
    using Generation = std::uint64_t;

    [[nodiscard]] bool visible() const noexcept { return visible_; }

    [[nodiscard]] Generation begin_show() noexcept {
        visible_ = true;
        return ++generation_;
    }

    [[nodiscard]] Generation begin_hide() noexcept {
        visible_ = false;
        return ++generation_;
    }

    [[nodiscard]] bool current(Generation generation) const noexcept {
        return generation_ == generation;
    }

    void rollback_show(Generation generation) noexcept {
        if (current(generation)) visible_ = false;
    }

    void rollback_hide(Generation generation) noexcept {
        if (current(generation)) visible_ = true;
    }

    void set_visible(bool visible) noexcept {
        visible_ = visible;
        ++generation_;
    }

private:
    Generation generation_{};
    bool visible_{};
};

class WindowControlPostState final {
public:
    template <class Post>
    [[nodiscard]] bool try_post(Post&& post) noexcept {
        if (posted_) return true;
        posted_ = true;
        try {
            if (std::forward<Post>(post)()) return true;
        } catch (...) {
            // Lifecycle control must never unwind through a native/input callback
            // merely because ordinary Dispatcher enqueue allocated and failed.
        }
        posted_ = false;
        return false;
    }

    void callback_started() noexcept { posted_ = false; }
    void cancel() noexcept { posted_ = false; }
    [[nodiscard]] bool posted() const noexcept { return posted_; }

private:
    bool posted_{};
};

enum class WindowClosePhase {
    Open,
    Requesting,
    Pending,
    Closed,
    Teardown,
};

/// Pure close lifecycle state used at the native window boundary.
///
/// User callbacks are deliberately outside this object. The platform owner
/// enters Requesting before scheduling a user veto callback at a safe Dispatcher
/// checkpoint, then revalidates the phase afterwards. A programmatic request
/// made reentrantly from that callback moves directly to Pending, so a later
/// Cancel result cannot undo the explicit accepted close. Teardown is terminal
/// and suppresses pending completion.
class WindowCloseState final {
public:
    [[nodiscard]] WindowClosePhase phase() const noexcept { return phase_; }
    [[nodiscard]] bool requesting() const noexcept {
        return phase_ == WindowClosePhase::Requesting;
    }
    [[nodiscard]] bool closed() const noexcept { return phase_ == WindowClosePhase::Closed; }
    [[nodiscard]] bool pending() const noexcept { return phase_ == WindowClosePhase::Pending; }

    [[nodiscard]] bool begin_user_request() noexcept {
        if (phase_ != WindowClosePhase::Open) return false;
        phase_ = WindowClosePhase::Requesting;
        return true;
    }

    void finish_user_request(bool accept) noexcept {
        if (phase_ != WindowClosePhase::Requesting) return;
        phase_ = accept ? WindowClosePhase::Pending : WindowClosePhase::Open;
    }

    [[nodiscard]] bool request_programmatic() noexcept {
        if (phase_ != WindowClosePhase::Open && phase_ != WindowClosePhase::Requesting) {
            return false;
        }
        phase_ = WindowClosePhase::Pending;
        return true;
    }

    [[nodiscard]] bool complete_accepted_close() noexcept {
        if (phase_ != WindowClosePhase::Pending) return false;
        phase_ = WindowClosePhase::Closed;
        return true;
    }

    void begin_teardown() noexcept {
        if (phase_ == WindowClosePhase::Closed || phase_ == WindowClosePhase::Teardown) return;
        phase_ = WindowClosePhase::Teardown;
    }

private:
    WindowClosePhase phase_{WindowClosePhase::Open};
};

} // namespace ui::detail
