#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/detail/raster_cache_epoch.hpp>

#include "include/core/SkCanvas.h"

#include <cstdint>

namespace ui::detail {

struct PainterPrivateHooks;

struct RasterCachePaintRequest final {
    std::uint64_t node_id{};
    RasterCacheEpoch::Token token{};
    Rect local_extent{};
    Rect scene_extent{};
    bool allow_reuse{};
};

using RasterCachePaintCallback =
    bool (*)(void* callback_state,
             SkCanvas& canvas,
             const PainterPrivateHooks* hooks);

using RasterCacheUnsafeCallback =
    void (*)(void* callback_state) noexcept;

using RasterCacheValidateCallback =
    bool (*)(void* callback_state) noexcept;

using RasterCacheCommitCallback =
    bool (*)(void* callback_state) noexcept;

} // namespace ui::detail
