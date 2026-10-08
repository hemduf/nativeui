#pragma once

#include "../view_geometry.hpp"
#include <nativeui/ui.hpp>
#include <nativeui/window.hpp>
#include <functional>
#include <utility>

namespace ui::detail {

/// Manages per-view preferred-size coalescing and callback reentrancy.
/// PreferredSizeState owns the lifetime-safe in-flight callback token.
class PreferredSizeNotifier final {
public:
    void set_callback(PreferredSizeCallback callback, UI& ui) {
        callback_ = std::move(callback);
        state_.reset();
        dirty_ = static_cast<bool>(callback_);
        flush(ui);
    }

    void clear_callback() { callback_ = {}; }
    void reset_state() noexcept { state_.reset(); }
    void mark_dirty() noexcept { dirty_ = true; }

    void flush(UI& ui) {
        if (!callback_) return;
        if (dirty_) {
            dirty_ = false;
            state_.queue(ui.measure().preferred);
        }
        // The callback may synchronously destroy its owner. Nothing reads
        // this object after dispatch_once returns.
        (void)state_.dispatch_once([this](Size preferred) {
            auto callback = callback_;
            if (callback) callback(preferred);
        });
    }

private:
    PreferredSizeState state_;
    PreferredSizeCallback callback_;
    bool dirty_{};
};

} // namespace ui::detail
