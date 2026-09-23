#include <nativeui/nativeui.hpp>

#include "view_geometry.hpp"
#include "scene_extent.hpp"

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


namespace {

constexpr uintptr_t kCaretTimerId = 0x4E554943u; // "NUIC"
constexpr double kCaretBlinkSeconds = 0.5;
constexpr double kMultiClickSeconds = 0.35;
constexpr float kMultiClickDistance = 5.0f;
constexpr GrGLenum kGlRgba8 = 0x8058u;
constexpr GrGLenum kGlFramebuffer = 0x8D40u;

template <class Callback>
class ScopeExit final {
public:
    explicit ScopeExit(Callback callback) : callback_(std::move(callback)) {}
    ~ScopeExit() noexcept { callback_(); }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

private:
    Callback callback_;
};

PuglSpan to_pugl_span(float physical) {
    return static_cast<PuglSpan>(detail::physical_to_pugl_view_span(physical));
}

[[noreturn]] void throw_pugl(PuglStatus status, const char* what) {
    throw std::runtime_error(std::string(what) + ": " + puglStrerror(status));
}

sk_sp<const GrGLInterface> make_skia_gl_interface() {
#if defined(__EMSCRIPTEN__)
    // Skia's WebGL interface binds the emscripten_gl* entry points of the
    // WebGL2 context Pugl created; the generic assembled path would have to
    // resolve them through emscripten_webgl_get_proc_address instead.
    return GrGLInterfaces::MakeWebGL();
#elif defined(__linux__)
    // On GLX, Skia's native interface is built against the same current
    // context and system GL implementation as the pinned desktop package.
    // The generic assembled path has been observed to hand GrGLExtensions an
    // unusable discovery function under Xvfb/Mesa llvmpipe, causing a SIGSEGV
    // during the first exposure instead of returning an initialization error.
    // Do not fall back to that unsafe path on Linux: a missing native interface
    // is a controlled renderer-init failure.
    return GrGLMakeNativeInterface();
#else
    return GrGLMakeAssembledInterface(
        nullptr,
        [](void*, const char name[]) -> GrGLFuncPtr {
            return reinterpret_cast<GrGLFuncPtr>(puglGetProcAddress(name));
        });
#endif
}

} // namespace

namespace detail {

class SkiaGlRenderer {
public:
    // PUGL_UNREALIZE enters the owning GL context before calling us. Release
    // GPU objects there; if entering the context failed, abandon instead.
    void reset() noexcept {
        if (context_ && context_->abandoned()) {
            abandon();
            return;
        }
        snapshot_.reset();
        presentation_.reset();
        scene_.reset();
        context_.reset();
        interface_.reset();
        clear_identity();
    }

    void abandon() noexcept {
        if (context_) context_->abandonContext();
        snapshot_.reset();
        presentation_.reset();
        scene_.reset();
        context_.reset();
        interface_.reset();
        clear_identity();
    }

    void invalidate() noexcept {
        full_repaint_required_ = true;
        present_pending_ = true;
    }

    [[nodiscard]] bool invalidate_from_ui() noexcept {
        invalidate();
        if (rendering_) {
            deferred_redraw_pending_ = true;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool rendering() const noexcept { return rendering_; }

    void invalidate_content() noexcept {
        scene_valid_ = false;
        invalidate();
    }

    [[nodiscard]] bool take_deferred_redraw() noexcept {
        return std::exchange(deferred_redraw_pending_, false);
    }

private:
    void clear_identity() noexcept {
        width_ = height_ = 0;
        scene_scale_ = 0.0f;
        bind_framebuffer_ = nullptr;
        presentation_framebuffer_ = 0;
        presentation_samples_ = presentation_stencil_bits_ = 0;
        scene_valid_ = false;
        full_repaint_required_ = true;
        present_pending_ = true;
        rendering_ = false;
        deferred_redraw_pending_ = false;
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        readback_pending_ = false;
        readback_result_.reset();
#endif
    }

public:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    void inject_fault(SceneFaultStage stage) noexcept { fault_stage_ = stage; }

    [[nodiscard]] SceneDiagnostics diagnostics() const noexcept {
        return SceneDiagnostics{
            .scene_allocations = scene_allocations_,
            .scene_builds = scene_builds_,
            .presentations = presentations_,
            .failed_exposes = 0,
            .scene_valid = scene_valid_,
            .full_repaint_required = full_repaint_required_,
            .present_pending = present_pending_,
        };
    }

    void request_readback(int x, int y) noexcept {
        readback_x_ = x;
        readback_y_ = y;
        readback_pending_ = true;
        readback_result_.reset();
    }

    [[nodiscard]] std::optional<PlatformReadbackPixel> take_readback() noexcept {
        auto result = readback_result_;
        readback_result_.reset();
        return result;
    }
#endif

    bool render(UI& ui,
                PlatformServices& services,
                Size physical_size,
                float scale_factor) {
        if (rendering_) {
            invalidate();
            return false;
        }
        if (ui.scene_paint_blocked()) {
            invalidate();
            return false;
        }
        rendering_ = true;
        const ScopeExit finish_render{[this]() noexcept { rendering_ = false; }};

        // Validate before GL calls or narrowing; backend caps are checked once
        // the context exists. The scene allocator also uses this exact extent.
        const auto extent = validate_scene_extent(
            physical_size, scale_factor, std::numeric_limits<int>::max());
        if (!extent) {
            invalidate_content();
            return false;
        }
        const int physical_width = extent->width;
        const int physical_height = extent->height;

        // Capture Pugl's borrowed target before Skia binds its scene FBO. The
        // restore guard also makes the next expose's entry binding authoritative.
        constexpr GLenum kFramebufferBinding = 0x8CA6;
        constexpr GLenum kStencilBits = 0x0D57;
        constexpr GLenum kSamples = 0x80A9;
        GLint framebuffer = 0;
        GLint stencil_bits = 0;
        GLint samples = 0;
        glGetIntegerv(kFramebufferBinding, &framebuffer);
        glGetIntegerv(kStencilBits, &stencil_bits);
        glGetIntegerv(kSamples, &samples);
        // Resolve through Pugl once per renderer/context lifetime. Resolving
        // on every expose allocates a CFString on macOS; accessing the prebuilt
        // Skia function table here fails in Linux sanitizer builds.
        if (!bind_framebuffer_) {
            bind_framebuffer_ = reinterpret_cast<GrGLBindFramebufferFn*>(
                puglGetProcAddress("glBindFramebuffer"));
        }
        const auto bind_framebuffer = bind_framebuffer_;
        if (!bind_framebuffer) {
            invalidate_content();
            return false;
        }
        const ScopeExit restore_binding{[this, framebuffer, bind_framebuffer]() noexcept {
            if (interface_ && (!context_ || !context_->abandoned())) {
                bind_framebuffer(kGlFramebuffer, static_cast<GrGLuint>(framebuffer));
                if (context_) context_->resetContext(kRenderTarget_GrGLBackendState);
            }
        }};

        glViewport(0, 0, physical_width, physical_height);

        if (!context_) {
            interface_ = make_skia_gl_interface();
            if (!interface_) return false;
            context_ = GrDirectContexts::MakeGL(interface_);
            if (!context_) return false;
        }
        if (context_->abandoned()) {
            abandon();
            return false;
        }
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        if (fail_at(SceneFaultStage::ConfirmedContextLoss)) {
            abandon();
            return false;
        }
#endif
        if (physical_width > context_->maxRenderTargetSize() ||
            physical_height > context_->maxRenderTargetSize()) {
            invalidate_content();
            return false;
        }

        if (!scene_ || width_ != physical_width || height_ != physical_height) {
            invalidate_content();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (fail_at(SceneFaultStage::BeforeSceneAllocation)) return false;
#endif
            const auto info = SkImageInfo::Make(
                physical_width, physical_height, kRGBA_8888_SkColorType,
                kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
            auto replacement = SkSurfaces::RenderTarget(
                context_.get(), skgpu::Budgeted::kYes, info, 0,
                kBottomLeft_GrSurfaceOrigin, nullptr);
            if (!replacement) {
                invalidate();
                return false;
            }
            snapshot_.reset();
            scene_ = std::move(replacement);
            presentation_.reset();
            width_ = physical_width;
            height_ = physical_height;
            scene_valid_ = false;
            invalidate();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            ++scene_allocations_;
#endif
        }

        if (scene_scale_ != scale_factor) {
            scene_valid_ = false;
            invalidate();
        }
        if (ui.dirty()) invalidate();

        if (!scene_valid_ || full_repaint_required_) {
            scene_valid_ = false; // in-place pixels are invalid from the first mutation
            full_repaint_required_ = true;
            snapshot_.reset(); // avoid snapshot-triggered copy-on-write on rebuild
            auto* canvas = scene_->getCanvas();
            canvas->restoreToCount(1);
            canvas->resetMatrix();
            canvas->clear(SK_ColorBLACK); // canonical opaque native background
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (fail_at(SceneFaultStage::AfterSceneReset)) return false;
#endif
            full_repaint_required_ = false;
            try {
                const SkAutoCanvasRestore restore_canvas{canvas, true};
                canvas->scale(scale_factor, scale_factor);
                ui.paint(*canvas, services);
            } catch (...) {
                full_repaint_required_ = true;
                throw;
            }
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (fail_at(SceneFaultStage::AfterRetainedPaint)) {
                invalidate();
                return false;
            }
#endif
            const auto scene_submission = context_->flushAndSubmit(scene_.get());
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            const bool scene_submission_failed =
                fail_at(SceneFaultStage::SceneSubmission);
#else
            constexpr bool scene_submission_failed = false;
#endif
            if (!scene_submission.fSuccess || scene_submission_failed) {
                invalidate();
                return false;
            }
            scene_scale_ = scale_factor;
            scene_valid_ = true;
            if (ui.dirty()) full_repaint_required_ = true;
            deferred_redraw_pending_ = full_repaint_required_;
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            ++scene_builds_;
#endif
        }

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        if (fail_at(SceneFaultStage::Snapshot)) {
            present_pending_ = true;
            return false;
        }
#endif
        if (!snapshot_) snapshot_ = scene_->makeImageSnapshot();
        if (!snapshot_) {
            present_pending_ = true;
            return false;
        }

        if (!presentation_ || presentation_framebuffer_ != framebuffer ||
            presentation_samples_ != samples ||
            presentation_stencil_bits_ != stencil_bits) {
            GrGLFramebufferInfo framebuffer_info{};
            framebuffer_info.fFBOID = static_cast<GrGLuint>(framebuffer);
            framebuffer_info.fFormat = kGlRgba8;
            const auto backend = GrBackendRenderTargets::MakeGL(
                physical_width, physical_height,
                std::max(0, static_cast<int>(samples)),
                std::max(0, static_cast<int>(stencil_bits)), framebuffer_info);
            auto replacement = SkSurfaces::WrapBackendRenderTarget(
                context_.get(), backend, kBottomLeft_GrSurfaceOrigin,
                kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
            if (!replacement) {
                present_pending_ = true;
                return false;
            }
            presentation_ = std::move(replacement);
            presentation_framebuffer_ = framebuffer;
            presentation_samples_ = samples;
            presentation_stencil_bits_ = stencil_bits;
        }

        auto* target = presentation_->getCanvas();
        target->restoreToCount(1);
        target->resetMatrix();
        const SkAutoCanvasRestore restore_target{target, true};
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        if (fail_at(SceneFaultStage::PresentationCopy)) {
            present_pending_ = true;
            return false;
        }
#endif
        SkPaint copy;
        copy.setBlendMode(SkBlendMode::kSrc);
        target->drawImageRect(snapshot_.get(),
                              SkRect::MakeWH(static_cast<SkScalar>(width_),
                                             static_cast<SkScalar>(height_)),
                              SkSamplingOptions{SkFilterMode::kNearest}, &copy);
        const auto presentation_submission = context_->flushAndSubmit(presentation_.get());
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        const bool presentation_submission_failed =
            fail_at(SceneFaultStage::PresentationSubmission);
#else
        constexpr bool presentation_submission_failed = false;
#endif
        if (!presentation_submission.fSuccess || presentation_submission_failed) {
            present_pending_ = true;
            return false;
        }
        present_pending_ = false;
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        ++presentations_;
#endif

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        if (readback_pending_) {
            readback_pending_ = false;
            const int x = std::clamp(readback_x_, 0, width_ - 1);
            const int y = std::clamp(readback_y_, 0, height_ - 1);
            std::array<std::uint8_t, 4> rgba{};
            const auto readback_info = SkImageInfo::Make(
                1,
                1,
                kRGBA_8888_SkColorType,
                kPremul_SkAlphaType,
                SkColorSpace::MakeSRGB());
            if (presentation_->readPixels(
                    readback_info,
                    rgba.data(),
                    rgba.size(),
                    x,
                    y)) {
                readback_result_ = PlatformReadbackPixel{
                    rgba[0], rgba[1], rgba[2], rgba[3]};
            }
        }
#endif
        return true;
    }

private:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    bool fail_at(SceneFaultStage stage) noexcept {
        if (fault_stage_ != stage) return false;
        fault_stage_ = SceneFaultStage::None;
        return true;
    }
#endif
    sk_sp<const GrGLInterface> interface_;
    sk_sp<GrDirectContext> context_;
    GrGLBindFramebufferFn* bind_framebuffer_{};
    sk_sp<SkSurface> scene_;
    sk_sp<SkImage> snapshot_;
    sk_sp<SkSurface> presentation_; // borrowed Pugl framebuffer, never owned
    int width_{};
    int height_{};
    float scene_scale_{};
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
    std::uint64_t scene_builds_{};
    std::uint64_t presentations_{};
    int readback_x_{};
    int readback_y_{};
    bool readback_pending_{};
    std::optional<PlatformReadbackPixel> readback_result_;
#endif
};

} // namespace detail

namespace {

[[nodiscard]] bool platform_primary_modifier(PuglMods mods) noexcept {
#if defined(__APPLE__)
    return (mods & PUGL_MOD_SUPER) != 0;
#else
    return (mods & PUGL_MOD_CTRL) != 0;
#endif
}

Key translate_key(uint32_t key, PuglMods mods) {
    const bool primary = platform_primary_modifier(mods);
    switch (key) {
    case PUGL_KEY_LEFT: return Key::Left;
    case PUGL_KEY_RIGHT: return Key::Right;
    case PUGL_KEY_UP: return Key::Up;
    case PUGL_KEY_DOWN: return Key::Down;
    case PUGL_KEY_HOME: return Key::Home;
    case PUGL_KEY_END: return Key::End;
    case PUGL_KEY_BACKSPACE: return Key::Backspace;
    case PUGL_KEY_DELETE: return Key::Delete;
    case PUGL_KEY_ENTER: return Key::Enter;
    case PUGL_KEY_ESCAPE: return Key::Escape;
    default: break;
    }

    return detail::translate_ascii_key(key, primary);
}

void apply_modifiers(InputEvent& out, PuglMods state) {
    out.shift = (state & PUGL_MOD_SHIFT) != 0;
    out.ctrl = (state & PUGL_MOD_CTRL) != 0;
    out.alt = (state & PUGL_MOD_ALT) != 0;
    out.gui = (state & PUGL_MOD_SUPER) != 0;
    out.primary = platform_primary_modifier(state);
}
