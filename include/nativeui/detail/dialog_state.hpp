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
    std::function<void()> escape_handler;
    std::function<void()> deactivate_handler;

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

    [[nodiscard]] bool bind_handlers(
        std::uint64_t generation,
        std::function<void()> on_escape,
        std::function<void()> on_deactivate) {
        if (!owns(generation)) return false;
        escape_handler = std::move(on_escape);
        deactivate_handler = std::move(on_deactivate);
        return true;
    }

    [[nodiscard]] bool handle_escape() {
        if (ui_tearing_down || active_generation == 0 || !escape_handler) return false;
        auto callback = escape_handler;
        callback();
        return true;
    }

    [[nodiscard]] bool handle_deactivate() {
        if (ui_tearing_down || active_generation == 0 || !deactivate_handler) return false;
        auto callback = deactivate_handler;
        callback();
        return true;
    }

    [[nodiscard]] bool release(std::uint64_t generation) noexcept {
        if (!owns(generation)) return false;
        active_generation = 0;
        escape_handler = {};
        deactivate_handler = {};
        return true;
    }

    [[nodiscard]] bool defer_completion(
        std::uint64_t generation, std::function<void()> completion) {
        if (!owns(generation) || pending_completion) return false;
        pending_completion_generation = generation;
        pending_completion = std::move(completion);
        return true;
    }

    void begin_ui_teardown() noexcept {
        ui_tearing_down = true;
        active_generation = 0;
        pending_completion_generation = 0;
        pending_completion = {};
        escape_handler = {};
        deactivate_handler = {};
    }
};

} // namespace ui::detail
