#pragma once

#include <nativeui/paint_style.hpp>
#include <nativeui/detail/raster_cache_paint.hpp>

#include "include/core/SkImageFilter.h"
#include "include/core/SkShader.h"

#include <memory>

namespace ui::detail {

struct ShaderBrushSnapshot;
struct PainterPrivateHooks final {
    void* state{};
    sk_sp<SkShader> (*materialize_image_texture)(
        void* state, const ImageTexture& texture){};
    sk_sp<SkShader> (*materialize_shader_brush)(
        void* state,
        const std::shared_ptr<const ShaderBrushSnapshot>& snapshot){};
    sk_sp<SkImageFilter> (*materialize_effect_filter)(
        void* state, const Effect& effect){};
    sk_sp<SkShader> (*materialize_linear_gradient)(
        void* state, const LinearGradient& gradient){};
    sk_sp<SkShader> (*materialize_radial_gradient)(
        void* state, const RadialGradient& gradient){};
    bool (*paint_raster_cache_boundary)(
        void* state,
        const RasterCachePaintRequest& request,
        SkCanvas& destination,
        void* callback_state,
        RasterCachePaintCallback paint_callback,
        RasterCacheCommitCallback commit_callback){};
};

} // namespace ui::detail
