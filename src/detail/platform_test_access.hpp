#pragma once

#include <nativeui/window.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace ui::detail {

struct PlatformReadbackPixel final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

/// A bounded physical-pixel rectangle copied from the presented surface.
/// Pixels are row-major from the top-left corner of the requested region.
struct PlatformReadbackRegion final {
    int x{};
    int y{};
    int width{};
    int height{};
    std::vector<PlatformReadbackPixel> pixels;
};

enum class SceneFaultStage : std::uint8_t {
    None,
    BeforeSceneAllocation,
    ConfirmedContextLoss,
    AfterSceneReset,
    AfterRetainedPaint,
    SceneSubmission,
    Snapshot,
    PresentationCopy,
    PresentationSubmission,
};

struct SceneDiagnostics final {
    std::uint64_t scene_allocations{};
    std::uint64_t scene_builds{};
    std::uint64_t partial_scene_updates{};
    std::uint64_t presentations{};
    std::uint64_t failed_exposes{};
    std::uint64_t deferred_redraw_attempts{};
    std::uint64_t deferred_redraw_rejections{};
    std::uint64_t redraw_requests_during_render{};
    // Physical extent of the persistent scene surface.
    int scene_width{};
    int scene_height{};
    // Device rectangle of the most recent scene update. These describe a scene
    // update (full or partial) and are not advanced by present-only frames.
    int last_update_x{};
    int last_update_y{};
    int last_update_width{};
    int last_update_height{};
    bool scene_valid{};
    bool full_repaint_required{};
    bool present_pending{};
};

struct PlatformTestAccess final {
    [[nodiscard]] static bool request_gpu_readback(
        StandaloneWindow& window,
        Point logical_point) noexcept;

    [[nodiscard]] static std::optional<PlatformReadbackPixel>
    take_gpu_readback(StandaloneWindow& window) noexcept;

    /// Request a bounded physical-pixel region from the presented surface.
    /// Returns false for non-positive or over-budget regions.
    [[nodiscard]] static bool request_gpu_readback_region(
        StandaloneWindow& window,
        int x,
        int y,
        int width,
        int height) noexcept;
    [[nodiscard]] static std::optional<PlatformReadbackRegion>
    take_gpu_readback_region(StandaloneWindow& window) noexcept;

    /// Test-only: ignore platform focus-in/out/configure focus refresh so a
    /// fixture can drive activation explicitly. Background windows do not
    /// reliably retain OS key focus, and an inactive tree conservatively
    /// requires a full repaint. Returns false when the view is not open.
    static bool suppress_platform_focus(
        StandaloneWindow& window, bool suppressed) noexcept;

    static bool inject_scene_fault(StandaloneWindow& window,
                                   SceneFaultStage stage) noexcept;
    [[nodiscard]] static SceneDiagnostics scene_diagnostics(
        StandaloneWindow& window) noexcept;
    static bool request_context_recreation(StandaloneWindow& window) noexcept;
    static bool override_scene_scale(StandaloneWindow& window,
                                     std::optional<float> scale) noexcept;
    static bool reject_next_deferred_redraw(StandaloneWindow& window) noexcept;
    static bool request_expose(StandaloneWindow& window) noexcept;

    [[nodiscard]] static bool request_gpu_readback(
        EmbeddedView& view,
        Point logical_point) noexcept;
    [[nodiscard]] static std::optional<PlatformReadbackPixel>
    take_gpu_readback(EmbeddedView& view) noexcept;
    static bool inject_scene_fault(EmbeddedView& view,
                                   SceneFaultStage stage) noexcept;
    [[nodiscard]] static SceneDiagnostics scene_diagnostics(
        EmbeddedView& view) noexcept;
};

} // namespace ui::detail
