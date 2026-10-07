#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/layout_types.hpp>
#include <nativeui/grid.hpp>

namespace ui::detail {
struct MainAxisAllocation { std::vector<float> extents; float free_space{}; };
struct GridTrackMetrics { float minimum{}; float preferred{}; };
float main_axis_offset(Justify justify, float extra) noexcept;
float distributed_gap(Justify justify, float base_gap, float extra, std::size_t count) noexcept;
float cross_axis_offset(Align align, float available, float extent) noexcept;
float flex_weight(float value) noexcept;
float saturating_extent(double value) noexcept;
float saturating_coordinate(double value) noexcept;
MainAxisAllocation allocate_main_axis(const std::vector<ChildMetrics>& children,
                                     float available, float gap, bool horizontal);
std::vector<float> allocate_grid_tracks(const std::vector<Track>& tracks,
                                       const std::vector<GridTrackMetrics>& content,
                                       float available, float gap);
} // namespace ui::detail
