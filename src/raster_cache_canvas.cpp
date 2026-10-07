#include "detail/raster_cache_canvas.hpp"

#include "include/utils/SkPaintFilterCanvas.h"

namespace ui::detail {
namespace {

// Observe operations even when the component restores its transform before
// returning. The proxy preserves target matrix/clip/metadata and forwards draws.
class RasterTransformCanvas final : public SkPaintFilterCanvas {
public:
    explicit RasterTransformCanvas(SkCanvas& target)
        : SkPaintFilterCanvas(&target) {}

    [[nodiscard]] bool transformed() const noexcept { return transformed_; }

protected:
    bool onFilter(SkPaint&) const override { return true; }
    void didTranslate(SkScalar x, SkScalar y) override {
        transformed_ |= x != 0.0f || y != 0.0f;
        SkPaintFilterCanvas::didTranslate(x, y);
    }
    void didScale(SkScalar x, SkScalar y) override {
        transformed_ |= x != 1.0f || y != 1.0f;
        SkPaintFilterCanvas::didScale(x, y);
    }
    void didConcat44(const SkM44& matrix) override {
        transformed_ |= matrix != SkM44{};
        SkPaintFilterCanvas::didConcat44(matrix);
    }
    void didSetM44(const SkM44& matrix) override {
        transformed_ = true;
        SkPaintFilterCanvas::didSetM44(matrix);
    }

private:
    bool transformed_{};
};

} // namespace

RasterPaintObservation observe_raster_paint(
    SkCanvas& canvas,
    void* callback_state,
    RasterCachePaintCallback paint_callback,
    const PainterPrivateHooks* hooks) {
    RasterTransformCanvas observer{canvas};
    const bool painted = paint_callback(callback_state, observer, hooks);
    return {painted, observer.transformed()};
}

} // namespace ui::detail
