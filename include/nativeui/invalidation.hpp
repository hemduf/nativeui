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

    DirtyRegion() {
        reserve_storage();
    }

    DirtyRegion(const DirtyRegion& other) : DirtyRegion() {
        rects_.assign(other.rects_.begin(), other.rects_.end());
    }

    DirtyRegion& operator=(const DirtyRegion& other) {
        if (this != &other) {
            rects_.assign(other.rects_.begin(), other.rects_.end());
        }
        return *this;
    }

    DirtyRegion(DirtyRegion&& other) : DirtyRegion() {
        // Swap with an already-reserved empty vector so both destination and
        // moved-from source retain the capacity invariant required by add().
        rects_.swap(other.rects_);
    }

    DirtyRegion& operator=(DirtyRegion&& other) noexcept {
        if (this != &other) {
            rects_.swap(other.rects_);
            other.rects_.clear();
        }
        return *this;
    }

    [[nodiscard]] bool empty() const noexcept { return rects_.empty(); }
    [[nodiscard]] const std::vector<Rect>& rects() const noexcept { return rects_; }

    void clear() noexcept { rects_.clear(); }

    /// Add a rectangle clipped to `clip`. Returns the (possibly merged) region
    /// that newly needs exposure, or nullopt when it was already fully covered.
    [[nodiscard]] std::optional<Rect> add(Rect rect, Rect clip) noexcept {
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
    void reserve_storage() {
        // add() may temporarily append a ninth fragment before collapsing to
        // the bounded union. Construction/copy/move may allocate, but every
        // successfully constructed object (including a moved-from source)
        // retains this capacity so add() itself is truly noexcept.
        rects_.reserve(kMaxRects + 1);
    }

    std::vector<Rect> rects_;
};

} // namespace ui
