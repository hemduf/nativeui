#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace ui::detail {

struct VirtualListRange {
    std::size_t first{};
    std::size_t last{}; // exclusive

    [[nodiscard]] std::size_t size() const noexcept { return last - first; }
    [[nodiscard]] bool empty() const noexcept { return first == last; }

    bool operator==(const VirtualListRange&) const = default;
};

[[nodiscard]] inline std::optional<float> virtual_list_content_height(
    std::size_t item_count,
    float row_height) noexcept {
    if (!std::isfinite(row_height) || !(row_height > 0.0f)) return std::nullopt;
    if (item_count == 0) return 0.0f;

    const double extent = static_cast<double>(item_count) * static_cast<double>(row_height);
    if (!std::isfinite(extent) ||
        extent > static_cast<double>(std::numeric_limits<float>::max())) {
        return std::nullopt;
    }
    return static_cast<float>(extent);
}

[[nodiscard]] inline std::optional<VirtualListRange> virtual_list_materialization_range(
    std::size_t item_count,
    float row_height,
    float scroll_y,
    float viewport_height,
    std::size_t overscan = 2) noexcept {
    const auto content_height = virtual_list_content_height(item_count, row_height);
    if (!content_height || !std::isfinite(scroll_y) || !std::isfinite(viewport_height) ||
        scroll_y < 0.0f || viewport_height < 0.0f) {
        return std::nullopt;
    }
    if (item_count == 0 || viewport_height == 0.0f) return VirtualListRange{};

    const double content = static_cast<double>(*content_height);
    const double viewport = static_cast<double>(viewport_height);
    const double maximum_offset = std::max(0.0, content - viewport);
    const double offset = std::clamp(static_cast<double>(scroll_y), 0.0, maximum_offset);
    const double row = static_cast<double>(row_height);

    std::size_t first_visible = static_cast<std::size_t>(std::floor(offset / row));
    first_visible = std::min(first_visible, item_count - 1);

    const double visible_end = std::min(content, offset + viewport);
    std::size_t last_visible = static_cast<std::size_t>(std::ceil(visible_end / row));
    last_visible = std::clamp(last_visible, first_visible + 1, item_count);

    const std::size_t before = std::min(overscan, first_visible);
    const std::size_t after = std::min(overscan, item_count - last_visible);
    return VirtualListRange{first_visible - before, last_visible + after};
}

} // namespace ui::detail
