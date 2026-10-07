#pragma once

#include "painter_private_hooks.hpp"
#include "render_resource_materialization.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "include/utils/SkPaintFilterCanvas.h"

#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace ui::detail {

// Both the Ganesh renderer and the display-less raster fixture use this exact
// transaction. Backend acquisition/submission remain owned by the caller.
struct RasterCacheBackend final {
    void* state{};
    float device_scale{1.0f};
    int max_surface_size{16777216};
    const PainterPrivateHooks* painter_hooks{};
    sk_sp<SkSurface> (*create_surface)(void*, const SkImageInfo&){};
    void (*submit_surface)(void*, SkSurface&){};
    sk_sp<SkImage> (*snapshot_surface)(void*, SkSurface&){};
    void (*before_retention)(void*){};
    void (*did_hit)(void*) noexcept{};
};

// Observe operations even when the component restores its transform before
// returning. SkPaintFilterCanvas preserves target matrix/clip/metadata and
// forwards drawing; the observer exists only on cold/stale passes.
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

[[nodiscard]] inline bool paint_raster_cache_boundary(
    RenderResourceMaterializationContext& resources,
    const RasterCacheBackend& backend,
    const RasterCachePaintRequest& request,
    SkCanvas& destination,
    void* callback_state,
    RasterCachePaintCallback paint_callback,
    RasterCacheValidateCallback validate_callback,
    RasterCacheCommitCallback commit_callback) {
    if (!backend.create_surface || !paint_callback ||
        !validate_callback || !commit_callback) return false;

    const auto local = request.local_extent;
    const auto scene = request.scene_extent;
    const double scale = backend.device_scale;
    const double right = static_cast<double>(scene.x) + scene.w;
    const double bottom = static_cast<double>(scene.y) + scene.h;
    if (!std::isfinite(local.x) || !std::isfinite(local.y) ||
        !std::isfinite(local.w) || !std::isfinite(local.h) ||
        !std::isfinite(scene.x) || !std::isfinite(scene.y) ||
        !std::isfinite(scene.w) || !std::isfinite(scene.h) ||
        !(local.w > 0.0f) || !(local.h > 0.0f) ||
        scene.w != local.w || scene.h != local.h ||
        !std::isfinite(right) || !std::isfinite(bottom) ||
        !std::isfinite(scale) || !(scale > 0.0)) return false;

    const double left_px = std::floor(static_cast<double>(scene.x) * scale);
    const double top_px = std::floor(static_cast<double>(scene.y) * scale);
    const double right_px = std::ceil(right * scale);
    const double bottom_px = std::ceil(bottom * scale);
    const double width_px = right_px - left_px;
    const double height_px = bottom_px - top_px;
    constexpr double kMaxExactFloatInteger = 16777216.0;
    const double max_int = static_cast<double>((std::numeric_limits<int>::max)());
    if (!std::isfinite(left_px) || !std::isfinite(top_px) ||
        !std::isfinite(right_px) || !std::isfinite(bottom_px) ||
        !(width_px > 0.0) || !(height_px > 0.0) ||
        width_px > max_int || height_px > max_int ||
        left_px < -kMaxExactFloatInteger || top_px < -kMaxExactFloatInteger ||
        right_px > kMaxExactFloatInteger || bottom_px > kMaxExactFloatInteger ||
        width_px > backend.max_surface_size ||
        height_px > backend.max_surface_size) return false;

    const auto matrix = destination.getLocalToDeviceAs3x3();
    if (!matrix.isFinite() || matrix.hasPerspective() ||
        matrix.getScaleX() != backend.device_scale ||
        matrix.getScaleY() != backend.device_scale ||
        matrix.getSkewX() != 0.0f || matrix.getSkewY() != 0.0f ||
        matrix.getTranslateX() != 0.0f || matrix.getTranslateY() != 0.0f) {
        return false;
    }

    const int width = static_cast<int>(width_px);
    const int height = static_cast<int>(height_px);
    const RenderResourceMaterializationContext::RasterCacheKey key{
        request.node_id, request.token, local, scene, backend.device_scale};
    const auto draw = [&](const sk_sp<SkImage>& image) {
        if (!image) return false;
        const SkAutoCanvasRestore restore{&destination, true};
        destination.resetMatrix();
        destination.drawImageRect(
            image,
            SkRect::MakeXYWH(static_cast<float>(left_px), static_cast<float>(top_px),
                            static_cast<float>(width), static_cast<float>(height)),
            SkSamplingOptions{SkFilterMode::kNearest}, nullptr);
        return true;
    };

    if (request.allow_reuse) {
        if (auto image = resources.find_raster(key)) {
            if (backend.did_hit) backend.did_hit(backend.state);
            return draw(image);
        }
    }

    const auto info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType,
                                       destination.imageInfo().refColorSpace());
    auto surface = backend.create_surface(backend.state, info);
    if (!surface) throw std::bad_alloc{};
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    bool transformed = false;
    {
        const SkAutoCanvasRestore restore{canvas, true};
        canvas->setMatrix(SkMatrix::MakeAll(
            backend.device_scale, 0.0f, -static_cast<float>(left_px),
            0.0f, backend.device_scale, -static_cast<float>(top_px),
            0.0f, 0.0f, 1.0f));
        RasterTransformCanvas observer{*canvas};
        if (!paint_callback(callback_state, observer, backend.painter_hooks)) {
            return false;
        }
        transformed = observer.transformed();
    }
    if (backend.submit_surface) backend.submit_surface(backend.state, *surface);
    auto image = backend.snapshot_surface
        ? backend.snapshot_surface(backend.state, *surface)
        : surface->makeImageSnapshot();
    if (!image) throw std::runtime_error("subtree raster cache snapshot failed");

    if (transformed || !validate_callback(callback_state)) return draw(image);
    if (backend.before_retention) backend.before_retention(backend.state);
    auto acquisition = resources.retain_raster(
        key, raster_retained_storage_bytes(width, height), image);
    if (!acquisition || !commit_callback(callback_state)) return draw(image);
    return draw(acquisition.image);
}

} // namespace ui::detail
