#pragma once

#include <cstdint>
#include <limits>

namespace ui::detail {

struct DialogState final {
    bool ui_tearing_down{};
    std::uint64_t next_generation{1};
    std::uint64_t active_generation{};

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

    [[nodiscard]] bool release(std::uint64_t generation) noexcept {
        if (!owns(generation)) return false;
        active_generation = 0;
        return true;
    }

    void begin_ui_teardown() noexcept {
        ui_tearing_down = true;
        active_generation = 0;
    }
};

} // namespace ui::detail
