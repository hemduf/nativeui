#pragma once

#include <nativeui/detail/semantic_native_publication.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>
#include <nativeui/window.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace ui::detail {

/// Readback of one native view's committed semantic publication state.
struct SemanticPublicationDiagnostics final {
    bool has_publication{};
    std::uint64_t native_generation{};
    std::uint64_t semantic_generation{};
    SemanticNativeGeometry geometry{};
};

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

/// Deterministic model of the embedded parent-to-screen platform query used by
/// the production EmbeddedView pump. RealPlatform defers to the production
/// native_screen_origin.hpp seam; Missing models a failed query and Point models
/// one successful physical screen-origin observation.
struct EmbeddedNativeScreenOriginQuery final {
    enum class Mode : std::uint8_t { RealPlatform, Missing, Point };
    Mode mode{Mode::RealPlatform};
    Point origin{};
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

    /// Construct a pre-v1 unmanaged window so the platform smoke can drive the
    /// real legacy StandaloneWindow::poll checkpoint. Test-only: the public
    /// pre-v1 constructor is deprecated for production consumers.
    [[nodiscard]] static std::unique_ptr<StandaloneWindow>
    make_unmanaged_standalone_window(UI& ui, WindowDesc desc);

    /// Install a test sink on the window's per-view semantic domain. Returns
    /// false when the window has no live semantic domain (invalid/retired view).
    [[nodiscard]] static bool install_semantic_notification_sink(
        StandaloneWindow& window,
        std::shared_ptr<SemanticNativeNotificationSink> sink) noexcept;

    /// Read the window's committed native publication state.
    [[nodiscard]] static SemanticPublicationDiagnostics
    semantic_publication_diagnostics(StandaloneWindow& window) noexcept;

    /// Model one T043 platform geometry observation on the window's view, as an
    /// accepted T065 dispatcher callback would during a real configure event.
    [[nodiscard]] static bool observe_native_scale(
        StandaloneWindow& window,
        float scale) noexcept;
    [[nodiscard]] static bool observe_native_physical_screen_origin(
        StandaloneWindow& window,
        Point origin) noexcept;

    [[nodiscard]] static bool request_gpu_readback(
        EmbeddedView& view,
        Point logical_point) noexcept;
    [[nodiscard]] static std::optional<PlatformReadbackPixel>
    take_gpu_readback(EmbeddedView& view) noexcept;
    static bool inject_scene_fault(EmbeddedView& view,
                                   SceneFaultStage stage) noexcept;
    [[nodiscard]] static SceneDiagnostics scene_diagnostics(
        EmbeddedView& view) noexcept;

    [[nodiscard]] static bool install_semantic_notification_sink(
        EmbeddedView& view,
        std::shared_ptr<SemanticNativeNotificationSink> sink) noexcept;
    [[nodiscard]] static SemanticPublicationDiagnostics
    semantic_publication_diagnostics(EmbeddedView& view) noexcept;
    [[nodiscard]] static bool observe_native_scale(
        EmbeddedView& view,
        float scale) noexcept;
    [[nodiscard]] static bool observe_native_physical_screen_origin(
        EmbeddedView& view,
        Point origin) noexcept;

    /// Read the embedded view's retained per-view T043 capture pair (last valid
    /// scale + physical screen origin) without creating a capture source.
    [[nodiscard]] static std::optional<SemanticNativeGeometry>
    retained_native_geometry(EmbeddedView& view) noexcept;

    /// Replace the embedded pump's platform screen-origin query result for
    /// subsequent polls. Passing Mode::RealPlatform restores the production
    /// native_screen_origin.hpp query.
    static bool override_embedded_screen_origin_query(
        EmbeddedView& view,
        EmbeddedNativeScreenOriginQuery query) noexcept;
};

} // namespace ui::detail
