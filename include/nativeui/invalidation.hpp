#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace ui {

/// Bounded value-semantic accumulator for dirty rectangles in logical coordinates.
///
/// Each input rectangle is clipped before publication. Overlapping or touching
/// rectangles are coalesced, and fragmentation is bounded: if more than
/// `kMaxRects` disjoint fragments would be retained, the complete region
/// conservatively collapses to one bounding rectangle.
///
/// Construction reserves the complete bounded working capacity and may allocate.
/// Once construction succeeds, `add()` is allocation-free and `noexcept`.
/// DirtyRegion itself performs no synchronization; retained NativeUI usage is
/// normally confined to the UI/main thread.
class DirtyRegion {
public:
    /// Maximum number of disjoint dirty rectangles retained before the region
    /// collapses to one bounding rectangle.
    static constexpr std::size_t kMaxRects = 8;

    /// Construct an empty region and reserve the full bounded work capacity.
    /// May throw if the initial storage reservation fails.
    DirtyRegion() {
        reserve_storage();
    }

    /// Copy into independently owned bounded storage. Construction may allocate.
    DirtyRegion(const DirtyRegion& other) : DirtyRegion() {
        rects_.assign(other.rects_.begin(), other.rects_.end());
    }

    /// Replace this region with an independent copy of `other`.
    /// Successfully constructed instances already retain the capacity invariant
    /// required by allocation-free `add()`.
    DirtyRegion& operator=(const DirtyRegion& other) {
        if (this != &other) {
            rects_.assign(other.rects_.begin(), other.rects_.end());
        }
        return *this;
    }

    /// Move-construct while leaving `other` valid, empty and ready for reuse.
    /// Construction may allocate for the destination's reserved empty storage.
    DirtyRegion(DirtyRegion&& other) : DirtyRegion() {
        // Swap with an already-reserved empty vector so both destination and
        // moved-from source retain the capacity invariant required by add().
        rects_.swap(other.rects_);
    }

    /// Move-assign without allocation. The moved-from region remains valid,
    /// empty and retains bounded capacity for later `add()` calls.
    DirtyRegion& operator=(DirtyRegion&& other) noexcept {
        if (this != &other) {
            rects_.swap(other.rects_);
            other.rects_.clear();
        }
        return *this;
    }

    /// True when no dirty logical pixels are recorded.
    [[nodiscard]] bool empty() const noexcept { return rects_.empty(); }

    /// Borrow the current coalesced rectangles. The reference/view is invalidated
    /// by subsequent mutation, assignment or destruction of this DirtyRegion.
    [[nodiscard]] const std::vector<Rect>& rects() const noexcept { return rects_; }

    /// Remove all dirty rectangles while preserving reserved capacity.
    void clear() noexcept { rects_.clear(); }

    /// Add one logical rectangle after clipping it to `clip`.
    ///
    /// `rect` and `clip` use the same caller-defined logical coordinate
    /// space; DirtyRegion performs no device-scale conversion. Overlapping or
    /// touching rectangles are coalesced. Returns the conservative merged
    /// rectangle whose exposure is newly required, or `nullopt` if the clipped
    /// input is empty or already completely covered.
    ///
    /// If a new disjoint fragment would exceed `kMaxRects`, every retained
    /// fragment collapses to one bounding rectangle. Successfully constructed
    /// instances reserve enough storage that this operation performs no
    /// allocation and is `noexcept`.
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
