#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <utility>

namespace ui::detail {

struct DialogState final {
    bool ui_tearing_down{};
    std::uint64_t next_generation{1};
    std::uint64_t active_generation{};
    std::uint64_t pending_completion_generation{};
    std::function<void()> pending_completion;
    std::function<void()> active_overlay_close;

    [[nodiscard]] std::uint64_t acquire() noexcept {
        if (ui_tearing_down || active_generation != 0 || next_generation == 0) return 0;

        const auto generation = next_generation;
        active_generation = generation;
        if (next_generation == std::numeric_limits<std::uint64_t>::max()) {
            next_generation = 0;
        } else {
            ++next_generation;
        }
        return generation;
    }

    [[nodiscard]] bool owns(std::uint64_t generation) const noexcept {
        return !ui_tearing_down && generation != 0 && active_generation == generation;
    }

    [[nodiscard]] bool set_active_overlay_close(
        std::uint64_t generation, std::function<void()> close) {
        if (!owns(generation) || !close) return false;
        active_overlay_close = std::move(close);
        return true;
    }

    [[nodiscard]] bool release(std::uint64_t generation) noexcept {
        if (!owns(generation)) return false;
        active_generation = 0;
        active_overlay_close = {};
        return true;
    }

    [[nodiscard]] bool defer_completion(
        std::uint64_t generation, std::function<void()> completion) {
        if (!owns(generation) || pending_completion) return false;
        pending_completion_generation = generation;
        pending_completion = std::move(completion);
        return true;
    }

    /// Abandon the active Dialog because the owning UI is leaving its active
    /// platform lifetime. No application completion may escape this boundary.
    /// The returned closure only tears down the retained T061 overlay and must
    /// be executed while the UI/overlay state is still alive.
    [[nodiscard]] std::function<void()> abandon_active() noexcept {
        active_generation = 0;
        pending_completion_generation = 0;
        pending_completion = {};
        return std::exchange(active_overlay_close, {});
    }

    [[nodiscard]] std::function<void()> begin_ui_teardown() noexcept {
        ui_tearing_down = true;
        return abandon_active();
    }
};

} // namespace ui::detail
