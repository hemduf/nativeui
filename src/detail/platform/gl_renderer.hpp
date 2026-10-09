#pragma once

// Scene rendering, GPU resources, and presentation in the consumer GL context.
#include <nativeui/nativeui.hpp>

#include "../view_geometry.hpp"
#include "../scene_extent.hpp"
#include "../scene_damage.hpp"
#include "scene_damage_state.hpp"
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
#  include "../platform_test_access.hpp"
#endif

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include "../painter_private_hooks.hpp"
#include "../render_resource_materialization.hpp"
#if defined(__EMSCRIPTEN__)
#  include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#endif

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace ui {


namespace detail {

class SkiaGlRenderer {
public:
    // PUGL_UNREALIZE enters the owning GL context before calling us. Release
    // GPU objects there; if entering the context failed, abandon instead.
    void reset() noexcept;

    void abandon() noexcept;

    void invalidate() noexcept;

    [[nodiscard]] bool invalidate_from_ui(Rect rect) noexcept;

    [[nodiscard]] bool rendering() const noexcept;

    void invalidate_content() noexcept;

    [[nodiscard]] bool take_deferred_redraw() noexcept;

private:
    void clear_identity() noexcept;

public:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    [[nodiscard]] static bool register_root_raster_cache_boundary(UI& ui);
    [[nodiscard]] static bool invalidate_root_raster_cache_boundary(UI& ui);

    void inject_fault(SceneFaultStage stage) noexcept;

    [[nodiscard]] SceneDiagnostics diagnostics() const noexcept;

    void request_readback(int x, int y) noexcept;

    [[nodiscard]] std::optional<PlatformReadbackPixel> take_readback() noexcept;

    // Test-only bounded physical-pixel readback for whole-surface scene
    // oracles. Bounded so a test cannot request an unbounded staging buffer.
    static constexpr std::uint64_t kMaxReadbackRegionPixels =
        4u * 1024u * 1024u;

    [[nodiscard]] bool request_readback_region(int x,
                                               int y,
                                               int width,
                                               int height) noexcept;

    [[nodiscard]] std::optional<PlatformReadbackRegion>
    take_readback_region() noexcept;
#endif

    bool render(UI& ui,
                PlatformServices& services,
                Size physical_size,
                float scale_factor);

private:
    void remember_scene_update(const SkIRect& update, bool partial) noexcept;

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    bool fail_at(SceneFaultStage stage) noexcept;
#endif
    sk_sp<const GrGLInterface> interface_;
    sk_sp<GrDirectContext> context_;
    RenderResourceMaterializationContext render_resources_;
    GrGLBindFramebufferFn* bind_framebuffer_{};
    sk_sp<SkSurface> scene_;
    sk_sp<SkImage> snapshot_;
    sk_sp<SkSurface> presentation_; // borrowed Pugl framebuffer, never owned
    int width_{};
    int height_{};
    float scene_scale_{};
    PendingSceneDamage pending_damage_{};
    bool scene_uses_effects_{};
    bool scene_frame_captured_{};
    int last_update_x_{};
    int last_update_y_{};
    int last_update_width_{};
    int last_update_height_{};
    GLint presentation_framebuffer_{};
    GLint presentation_samples_{};
    GLint presentation_stencil_bits_{};
    bool scene_valid_{};
    bool full_repaint_required_{true};
    bool present_pending_{true};
    bool rendering_{};
    bool deferred_redraw_pending_{};
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    SceneFaultStage fault_stage_{SceneFaultStage::None};
    std::uint64_t scene_allocations_{};
    std::uint64_t raster_cache_hits_{};
    std::uint64_t raster_cache_updates_{};
    std::uint64_t scene_builds_{};
    std::uint64_t partial_scene_updates_{};
    std::uint64_t presentations_{};
    std::uint64_t render_resource_cache_clears_{};
    int readback_x_{};
    int readback_y_{};
    bool readback_pending_{};
    std::optional<PlatformReadbackPixel> readback_result_;
    int readback_region_x_{};
    int readback_region_y_{};
    int readback_region_width_{};
    int readback_region_height_{};
    bool readback_region_pending_{};
    std::optional<PlatformReadbackRegion> readback_region_result_;
#endif
};

} // namespace detail

} // namespace ui
