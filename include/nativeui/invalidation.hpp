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

        // Discover the complete transitive merge without mutating published
        // dirty work. The region is bounded to kMaxRects, so at most N scans are
        // sufficient for every newly touching rectangle to enlarge `merged`.
        Rect merged = rect;
        for (std::size_t pass = 0; pass < rects_.size(); ++pass) {
            for (const auto existing : rects_) {
                if (overlaps_or_touches(existing, merged)) {
                    merged = unite(merged, existing);
                }
            }
        }

        std::size_t merged_count = 0;
        for (const auto existing : rects_) {
            if (overlaps_or_touches(existing, merged)) ++merged_count;
        }
        const auto final_size = rects_.size() - merged_count + 1;

        // Capacity growth is the only fallible publication step. Perform it
        // before erasing any already-published rectangle so std::bad_alloc has
        // the strong guarantee: the old dirty set remains byte-for-byte valid.
        if (final_size > rects_.capacity()) rects_.reserve(final_size);

        for (std::size_t i = 0; i < rects_.size();) {
            if (overlaps_or_touches(rects_[i], merged)) {
                rects_.erase(rects_.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        rects_.push_back(merged);

        if (rects_.size() > kMaxRects) {
            Rect bounds{};
            for (const auto item : rects_) bounds = unite(bounds, item);
            // Capacity for final_size was secured before mutation. Rect is a
            // trivial value type, so collapsing in place is allocation-free and
            // cannot fail after publication has begun.
            rects_.front() = bounds;
            rects_.resize(1);
            merged = bounds;
        }
        return merged;
    }

private:
    std::vector<Rect> rects_;
};

} // namespace ui
