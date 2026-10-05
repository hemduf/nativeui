#include "detail/layout_support.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ui::detail {

namespace {
float axis_extent(Size size, bool horizontal) noexcept { return horizontal ? size.w : size.h; }
}

float main_axis_offset(Justify justify, float extra) noexcept {
    extra = std::isfinite(extra) ? std::max(0.0f, extra) : 0.0f;
    switch (justify) {
    case Justify::Center: return extra * 0.5f;
    case Justify::End: return extra;
    case Justify::Start:
    case Justify::SpaceBetween: return 0.0f;
    }
    return 0.0f;
}

float distributed_gap(
    Justify justify, float base_gap, float extra, std::size_t count) noexcept {
    base_gap = std::isfinite(base_gap) ? std::max(0.0f, base_gap) : 0.0f;
    extra = std::isfinite(extra) ? std::max(0.0f, extra) : 0.0f;
    return (justify == Justify::SpaceBetween && count > 1)
               ? base_gap + extra / static_cast<float>(count - 1)
               : base_gap;
}

float cross_axis_offset(
    Align align, float available, float extent) noexcept {
    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    extent = std::isfinite(extent) ? std::clamp(extent, 0.0f, available) : 0.0f;
    const float extra = std::max(0.0f, available - extent);
    switch (align) {
    case Align::Center: return extra * 0.5f;
    case Align::End: return extra;
    case Align::Start:
    case Align::Stretch: return 0.0f;
    }
    return 0.0f;
}



float saturating_extent(double value) noexcept {
    if (std::isnan(value) || value < 0.0) return 0.0f;
    return static_cast<float>(std::min(value, static_cast<double>(std::numeric_limits<float>::max())));
}

float saturating_coordinate(double value) noexcept {
    if (std::isnan(value)) return 0.0f;
    const auto maximum = static_cast<double>(std::numeric_limits<float>::max());
    return static_cast<float>(std::clamp(value, -maximum, maximum));
}

float flex_weight(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

MainAxisAllocation allocate_main_axis(
    const std::vector<ChildMetrics>& children,
    float available,
    float gap,
    bool horizontal) {
    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    gap = std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f;

    MainAxisAllocation result;
    result.extents.reserve(children.size());

    const auto participant_count = static_cast<std::size_t>(std::count_if(
        children.begin(), children.end(),
        [](const ChildMetrics& child) { return child.participates_in_layout; }));
    const float gap_total = participant_count > 1
                                ? gap * static_cast<float>(participant_count - 1)
                                : 0.0f;
    const float available_for_children = std::max(0.0f, available - gap_total);

    float preferred_total = 0.0f;
    for (const auto& child : children) {
        if (!child.participates_in_layout) {
            result.extents.push_back(0.0f);
            continue;
        }
        const float minimum = std::max(0.0f, axis_extent(child.minimum, horizontal));
        const float preferred = std::max(minimum, std::max(0.0f, axis_extent(child.preferred, horizontal)));
        result.extents.push_back(preferred);
        preferred_total += preferred;
    }

    if (preferred_total < available_for_children) {
        const float extra = available_for_children - preferred_total;
        float total_grow = 0.0f;
        for (const auto& child : children) {
            if (child.participates_in_layout) total_grow += flex_weight(child.flex.grow);
        }

        if (total_grow > 0.0f) {
            for (std::size_t i = 0; i < children.size(); ++i) {
                if (!children[i].participates_in_layout) continue;
                const float weight = flex_weight(children[i].flex.grow);
                if (weight > 0.0f) {
                    result.extents[i] += extra * (weight / total_grow);
                }
            }
            result.free_space = 0.0f;
        } else {
            result.free_space = extra;
        }
        return result;
    }

    float deficit = preferred_total - available_for_children;
    constexpr float epsilon = 0.0001f;

    while (deficit > epsilon) {
        float total_shrink = 0.0f;
        for (std::size_t i = 0; i < children.size(); ++i) {
            if (!children[i].participates_in_layout) continue;
            const float minimum = std::max(0.0f, axis_extent(children[i].minimum, horizontal));
            if (result.extents[i] > minimum + epsilon) {
                total_shrink += flex_weight(children[i].flex.shrink);
            }
        }
        if (total_shrink <= 0.0f) break;

        const float pass_deficit = deficit;
        float reduced = 0.0f;
        for (std::size_t i = 0; i < children.size(); ++i) {
            if (!children[i].participates_in_layout) continue;
            const float weight = flex_weight(children[i].flex.shrink);
            const float minimum = std::max(0.0f, axis_extent(children[i].minimum, horizontal));
            const float capacity = std::max(0.0f, result.extents[i] - minimum);
            if (weight <= 0.0f || capacity <= epsilon) continue;

            const float requested = pass_deficit * (weight / total_shrink);
            const float delta = std::min(capacity, requested);
            result.extents[i] -= delta;
            reduced += delta;
        }

        if (reduced <= epsilon) break;
        deficit = std::max(0.0f, deficit - reduced);
    }

    // If minimum sizes cannot fit, overflow remains explicit. T011 defines
    // clipping/overflow policy; silently shrinking below a minimum here would
    // violate the constraint contract.
    result.free_space = 0.0f;
    return result;
}



std::vector<float> allocate_grid_tracks(
    const std::vector<Track>& tracks,
    const std::vector<GridTrackMetrics>& content,
    float available,
    float gap) {
    const std::size_t count = tracks.size();
    std::vector<float> extents(count, 0.0f);
    if (count == 0) return extents;

    available = std::isfinite(available) ? std::max(0.0f, available) : 0.0f;
    gap = std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f;
    const float gap_total = gap * static_cast<float>(count - 1);
    const float available_for_tracks = std::max(0.0f, available - gap_total);

    float base_total = 0.0f;
    float total_flex = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
        switch (tracks[i].type()) {
        case TrackType::Fixed:
            extents[i] = tracks[i].value();
            break;
        case TrackType::Auto:
            extents[i] = std::max(metrics.minimum, metrics.preferred);
            break;
        case TrackType::Flex:
            if (tracks[i].value() > 0.0f) {
                extents[i] = metrics.minimum;
                total_flex += tracks[i].value();
            } else {
                // A zero-weight flex track behaves like intrinsic auto sizing
                // instead of collapsing content to its minimum.
                extents[i] = std::max(metrics.minimum, metrics.preferred);
            }
            break;
        }
        base_total += extents[i];
    }

    if (base_total < available_for_tracks && total_flex > 0.0f) {
        const float extra = available_for_tracks - base_total;
        for (std::size_t i = 0; i < count; ++i) {
            if (tracks[i].type() == TrackType::Flex && tracks[i].value() > 0.0f) {
                extents[i] += extra * (tracks[i].value() / total_flex);
            }
        }
        return extents;
    }

    if (base_total > available_for_tracks) {
        float deficit = base_total - available_for_tracks;
        float total_capacity = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            if (tracks[i].type() != TrackType::Auto) continue;
            const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
            total_capacity += std::max(0.0f, extents[i] - metrics.minimum);
        }

        if (total_capacity > 0.0f) {
            const float requested = std::min(deficit, total_capacity);
            for (std::size_t i = 0; i < count; ++i) {
                if (tracks[i].type() != TrackType::Auto) continue;
                const auto metrics = i < content.size() ? content[i] : GridTrackMetrics{};
                const float capacity = std::max(0.0f, extents[i] - metrics.minimum);
                if (capacity > 0.0f) {
                    extents[i] -= requested * (capacity / total_capacity);
                }
            }
        }
    }

    // Fixed tracks and content minimums are never violated. If they cannot fit,
    // the grid overflows explicitly until T011 applies clipping policy.
    return extents;
}

} // namespace ui::detail
