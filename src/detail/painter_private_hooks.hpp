#pragma once

#include <nativeui/paint_style.hpp>
#include <nativeui/detail/raster_cache_epoch.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkShader.h"

#include <memory>

namespace ui::detail {

struct ShaderBrushSnapshot;

struct RasterCachePaintRequest final {
    NodeId node_id{};
    RasterCacheEpoch::Token token{};
    Rect local_extent{};
    float device_scale{1.0f};
    bool allow_reuse{};
};

using RasterCachePaintCallback =
    bool (*)(void* callback_state,
             SkCanvas& canvas,
             const PainterPrivateHooks* hooks);

using RasterCacheCommitCallback =
    bool (*)(void* callback_state) noexcept;

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
