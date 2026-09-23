#pragma once

#include <nativeui/window.hpp>

#include <cstdint>
#include <optional>

namespace ui::detail {

struct PlatformReadbackPixel final {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
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
    std::uint64_t presentations{};
    std::uint64_t failed_exposes{};
    std::uint64_t deferred_redraw_attempts{};
    std::uint64_t deferred_redraw_rejections{};
    std::uint64_t redraw_requests_during_render{};
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
