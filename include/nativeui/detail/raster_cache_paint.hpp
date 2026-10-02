#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/raster_cache_epoch.hpp>

#include "include/core/SkCanvas.h"

namespace ui::detail {

struct PainterPrivateHooks;

struct RasterCachePaintRequest final {
    NodeId node_id{};
    RasterCacheEpoch::Token token{};
    Rect local_extent{};
    Rect scene_extent{};
    float device_scale{1.0f};
    bool allow_reuse{};
};

using RasterCachePaintCallback =
    bool (*)(void* callback_state,
             SkCanvas& canvas,
             const PainterPrivateHooks* hooks);

using RasterCacheCommitCallback =
    bool (*)(void* callback_state) noexcept;

} // namespace ui::detail
