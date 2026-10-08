#include "render_resource_allocation_canvas.hpp"
#include "render_resource_allocation_probe.hpp"

#include "include/utils/SkPaintFilterCanvas.h"

namespace test {
namespace {

class SkiaAllocationScope final {
public:
    SkiaAllocationScope() noexcept
        : previously_enabled_(render_resource_allocations::pause_counting()) {}
    ~SkiaAllocationScope() noexcept {
        render_resource_allocations::resume_counting(previously_enabled_);
    }

private:
    bool previously_enabled_;
};

class AllocationObservingCanvas final : public SkPaintFilterCanvas {
public:
    explicit AllocationObservingCanvas(SkCanvas& target)
        : SkPaintFilterCanvas(&target) {}

protected:
    bool onFilter(SkPaint&) const override { return true; }

    void onDrawImageRect2(
        const SkImage* image, const SkRect& source, const SkRect& destination,
        const SkSamplingOptions& sampling, const SkPaint* paint,
        SrcRectConstraint constraint) override {
        // Linux Skia's raster image blit allocates internally. Exclude only the
        // concrete Skia delegation, keeping NativeUI and all other canvas
        // operations in the strict zero-allocation measurement.
        const SkiaAllocationScope pause;
        SkPaintFilterCanvas::onDrawImageRect2(
            image, source, destination, sampling, paint, constraint);
    }
};

} // namespace

std::unique_ptr<SkCanvas> make_allocation_observing_canvas(SkCanvas& target) {
    return std::make_unique<AllocationObservingCanvas>(target);
}

} // namespace test
