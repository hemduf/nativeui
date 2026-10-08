#pragma once

// View lifecycle and resource ownership, independent from event implementations.
#include "gl_renderer.hpp"
#include "platform_common.hpp"
#include "text_input_state.hpp"
#include "preferred_size_notifier.hpp"
#include "drop_offer_state.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include "input_translation.hpp"
#include "native_platform.hpp"
#include "pointer_state.hpp"
#include "../native_view_fault_probe.hpp"
#include "../native_ime_bridge.h"
#include "../window_control_state.hpp"
#include "../scoped_borrow_state.hpp"

namespace ui::detail {

class NativeViewConstructionFault final {};

class ViewCore {
public:
    ViewCore(UI& ui,
             ViewPlatformServices& services,
             PuglWorldType world_type,
             WindowDesc desc,
             NativeParentHandle parent,
             PuglWorld* shared_world = nullptr,
             std::function<void()> close_callback = {},
             NativeViewConstructionFaultStage fault_stage = NativeViewConstructionFaultStage::AfterViewCreation,
             NativeViewConstructionFaultResult* fault_result = nullptr,
             bool initially_visible = true);
    ~ViewCore() noexcept;

    ViewCore(const ViewCore&) = delete;
    ViewCore& operator=(const ViewCore&) = delete;

    bool poll(double timeout);

    void request_close();
    [[nodiscard]] bool should_close() const noexcept;
    [[nodiscard]] bool native_view_open() const noexcept;
    [[nodiscard]] Size size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] NativeViewHandle native_handle() const noexcept;

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    bool inject_scene_fault(detail::SceneFaultStage stage) noexcept;

    [[nodiscard]] detail::SceneDiagnostics scene_diagnostics() const noexcept;

    bool request_context_recreation() noexcept;

    bool override_scene_scale(std::optional<float> scale) noexcept;

    bool reject_next_deferred_redraw() noexcept;

    bool request_expose() noexcept;

    [[nodiscard]] bool request_gpu_readback(Point logical_point) noexcept;

    [[nodiscard]] std::optional<PlatformReadbackPixel>
    take_gpu_readback() noexcept;

    [[nodiscard]] bool request_gpu_readback_region(int x,
                                                   int y,
                                                   int width,
                                                   int height) noexcept;

    [[nodiscard]] std::optional<PlatformReadbackRegion>
    take_gpu_readback_region() noexcept;

    [[nodiscard]] bool suppress_platform_focus(bool suppressed) noexcept;
#endif
    [[nodiscard]] const std::string& last_error() const noexcept;

    bool set_title(std::string_view title);

    bool show();

    [[nodiscard]] bool visible() const noexcept;

    bool hide();

    bool set_size(Size logical);

    bool set_min_size(std::optional<Size> logical);

    bool set_max_size(std::optional<Size> logical);

    bool close_native_view() noexcept;

    void set_preferred_size_callback(PreferredSizeCallback callback);

    void request_redraw(Rect logical_rect);

    void set_text_input(bool active, Rect logical_area, float logical_cursor_offset);

    void set_clipboard_text(std::string_view text);

    void request_clipboard_text();

    bool accept_drop(std::string_view type, Rect logical_region);

    void reject_drop(Rect logical_region);

private:
    struct ConstructionCleanup {
        explicit ConstructionCleanup(ViewCore& owner,
                                     NativeViewConstructionFaultResult* fault_result) noexcept
            : owner_(&owner), fault_result(fault_result) {}
        ~ConstructionCleanup() noexcept {
            if (armed_) owner_->cleanup_partial_construction_noexcept(*this);
        }
        void release() noexcept { armed_ = false; }

        ViewCore* owner_{};
        NativeViewConstructionFaultResult* fault_result{};
        bool armed_{true};
        bool world_acquired{};
        bool view_acquired{};
        bool realized{};
        bool ime_acquired{};
        bool retained_started{};
        bool invalidation_attached{};
    };

    [[nodiscard]] static Size validated_initial_size(const WindowDesc& desc);

    [[nodiscard]] bool apply_size_constraints();

    [[nodiscard]] bool update_size_constraints(std::optional<Size> min_size,
                                               std::optional<Size> max_size);

    void record_scale_observation(bool accepted);

    void record_scene_error(const char* message) noexcept;

    void clear_scene_error() noexcept;


    void flush_preferred_size_notification();

    void remember_teardown_error(const char* message) noexcept;

    void release_owned_world_noexcept() noexcept;

    void cleanup_partial_construction_noexcept(ConstructionCleanup& cleanup) noexcept;

    static void magnify_thunk(void* user_data, float magnification,
                              double x, double y, PuglMods mods) noexcept;

    static void ime_event_thunk(void* user_data,
                                NativeUIImeEventType type,
                                const char* utf8,
                                size_t utf8_size,
                                size_t cursor_byte,
                                size_t selection_bytes) noexcept;

    void consume_failed_native_key(const PuglEvent* event) noexcept;

    static PuglStatus event_thunk(PuglView* view, const PuglEvent* event) noexcept;

    PuglStatus on_event(const PuglEvent* event);

    UI& ui_;
    PlatformServices& services_;
    PuglWorld* world_{};
    bool owns_world_{};
    PuglView* view_{};
    NativeUIImeBridge* ime_bridge_{};
    SkiaGlRenderer renderer_;
    detail::ViewGeometryState geometry_;
    detail::WindowSizeConstraints size_constraints_;
    PreferredSizeNotifier preferred_size_;
    bool should_close_{};
    detail::WindowVisibilityState visibility_;
    bool embedded_{};
    bool suppress_embedded_focus_cleanup_{};
    bool deferred_scene_redraw_{};
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    std::uint64_t failed_scene_exposes_{};
    std::uint64_t deferred_redraw_attempts_{};
    std::uint64_t deferred_redraw_rejections_{};
    std::uint64_t redraw_requests_during_render_{};
    bool recreate_renderer_on_expose_{};
    std::optional<float> scene_scale_override_;
    bool reject_deferred_redraw_once_{};
    bool suppress_platform_focus_{};
#endif
    ViewTextInputState text_input_;
    bool have_pointer_position_{};
    Point last_pointer_position_{};
    RawPointerTracker pointer_positions_{};
    MultiClickTracker click_sequence_{};
    ViewDropOffer drop_offer_;
    std::string last_error_;
    bool scene_error_active_{};
    std::function<void()> close_callback_;
};

#if defined(__linux__)
// A more specific overload is needed for the X11-aware native construction probe.
NativeViewConstructionFaultResult exercise_native_view_construction_fault(
    UI& ui, ViewPlatformServices& services, NativeParentHandle parent,
    Size size, NativeViewConstructionFaultStage stage) noexcept;
#endif

} // namespace ui::detail
