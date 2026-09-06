#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace ui {

/// Small logical-coordinate dirty-region accumulator. Overlapping/touching
/// rectangles are coalesced. If fragmentation grows beyond the bounded budget,
/// the region collapses to one bounding rectangle.
class DirtyRegion {
public:
    static constexpr std::size_t kMaxRects = 8;

    [[nodiscard]] bool empty() const noexcept { return rects_.empty(); }
    [[nodiscard]] const std::vector<Rect>& rects() const noexcept { return rects_; }

    void clear() noexcept { rects_.clear(); }

    /// Add a rectangle clipped to `clip`. Returns the (possibly merged) region
    /// that newly needs exposure, or nullopt when it was already fully covered.
    [[nodiscard]] std::optional<Rect> add(Rect rect, Rect clip) {
        rect = intersect(rect, clip);
        if (rect.empty()) return std::nullopt;

        for (const auto existing : rects_) {
            if (existing.contains(rect)) return std::nullopt;
        }

        Rect merged = rect;
        for (std::size_t i = 0; i < rects_.size();) {
            if (overlaps_or_touches(rects_[i], merged)) {
                merged = unite(merged, rects_[i]);
                rects_.erase(rects_.begin() + static_cast<std::ptrdiff_t>(i));
                i = 0; // the enlarged rectangle may now touch earlier entries
            } else {
                ++i;
            }
        }

        rects_.push_back(merged);
        if (rects_.size() > kMaxRects) {
            Rect bounds{};
            for (const auto item : rects_) bounds = unite(bounds, item);
            rects_.assign(1, bounds);
            merged = bounds;
        }
        return merged;
    }

private:
    std::vector<Rect> rects_;
};

} // namespace ui
