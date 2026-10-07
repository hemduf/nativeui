#pragma once

#include <nativeui/detail/raster_cache_paint.hpp>

namespace ui::detail {

struct RasterPaintObservation final {
    bool painted{};
    bool transformed{};
};

// Kept behind a source-private function because pinned Skia is built without
// RTTI. Only the derived canvas adapter is compiled with that ABI setting.
[[nodiscard]] RasterPaintObservation observe_raster_paint(
    SkCanvas& canvas,
    void* callback_state,
    RasterCachePaintCallback paint_callback,
    const PainterPrivateHooks* hooks);

} // namespace ui::detail
