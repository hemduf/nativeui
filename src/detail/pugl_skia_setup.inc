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

    [[nodiscard]] bool invalidate_from_ui(Rect rect) noexcept {
        if (!retain_pending_damage(rect)) full_repaint_required_ = true;
        present_pending_ = true;
        if (rendering_ && scene_frame_captured_) {
            deferred_redraw_pending_ = true;
        }
        if (rendering_) return false;
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
    [[nodiscard]] bool retain_pending_damage(Rect rect) noexcept {
        if (!std::isfinite(rect.x) || !std::isfinite(rect.y) ||
            !std::isfinite(rect.w) || !std::isfinite(rect.h) ||
            !(rect.w > 0.0f) || !(rect.h > 0.0f)) {
            return false;
        }
        const double right = static_cast<double>(rect.x) + rect.w;
        const double bottom = static_cast<double>(rect.y) + rect.h;
        if (!std::isfinite(right) || !std::isfinite(bottom)) return false;

        if (!pending_damage_valid_) {
            pending_damage_ = rect;
            pending_damage_valid_ = true;
            return true;
        }

        const double left_union = (std::min)(
            static_cast<double>(pending_damage_.x), static_cast<double>(rect.x));
        const double top_union = (std::min)(
            static_cast<double>(pending_damage_.y), static_cast<double>(rect.y));
        const double right_union = (std::max)(
            static_cast<double>(pending_damage_.x) + pending_damage_.w, right);
        const double bottom_union = (std::max)(
            static_cast<double>(pending_damage_.y) + pending_damage_.h, bottom);
        constexpr double max_float =
            static_cast<double>(std::numeric_limits<float>::max());
        if (left_union < -max_float || top_union < -max_float ||
            right_union > max_float || bottom_union > max_float ||
            right_union - left_union > max_float ||
            bottom_union - top_union > max_float) {
            return false;
        }
        const auto lower = [](double value) noexcept {
            float result = static_cast<float>(value);
            if (static_cast<double>(result) > value) {
                result = std::nextafter(result,
                                        -std::numeric_limits<float>::infinity());
            }
            return result;
        };
        const auto upper = [](double value) noexcept {
            float result = static_cast<float>(value);
            if (static_cast<double>(result) < value) {
                result = std::nextafter(result,
                                        std::numeric_limits<float>::infinity());
            }
            return result;
        };
        const float x = lower(left_union);
        const float y = lower(top_union);
        const float right_edge = upper(right_union);
        const float bottom_edge = upper(bottom_union);
        const double width_extent = static_cast<double>(right_edge) - x;
        const double height_extent = static_cast<double>(bottom_edge) - y;
        if (width_extent > max_float || height_extent > max_float) return false;
        const float width = upper(width_extent);
        const float height = upper(height_extent);
        if (!std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(width) || !std::isfinite(height)) {
            return false;
        }
        pending_damage_ = Rect{x, y, width, height};
        return true;
    }

    [[nodiscard]] static bool covers(Rect outer, Rect inner) noexcept {
        const double outer_right = static_cast<double>(outer.x) + outer.w;
        const double outer_bottom = static_cast<double>(outer.y) + outer.h;
        const double inner_right = static_cast<double>(inner.x) + inner.w;
        const double inner_bottom = static_cast<double>(inner.y) + inner.h;
        return std::isfinite(outer_right) && std::isfinite(outer_bottom) &&
               std::isfinite(inner_right) && std::isfinite(inner_bottom) &&
               outer.x <= inner.x && outer.y <= inner.y &&
               outer_right >= inner_right && outer_bottom >= inner_bottom;
    }


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
        scene_frame_captured_ = false;
        pending_damage_ = {};
        pending_damage_valid_ = false;
        scene_uses_effects_ = false;
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        readback_pending_ = false;
        readback_result_.reset();
        readback_region_pending_ = false;
        readback_region_result_.reset();
#endif
    }

public:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    void inject_fault(SceneFaultStage stage) noexcept { fault_stage_ = stage; }

    [[nodiscard]] SceneDiagnostics diagnostics() const noexcept {
        return SceneDiagnostics{
            .scene_allocations = scene_allocations_,
            .scene_builds = scene_builds_,
            .partial_scene_updates = partial_scene_updates_,
            .presentations = presentations_,
            .failed_exposes = 0,
            .deferred_redraw_attempts = 0,
            .deferred_redraw_rejections = 0,
            .redraw_requests_during_render = 0,
            .scene_width = width_,
            .scene_height = height_,
            .last_update_x = last_update_x_,
            .last_update_y = last_update_y_,
            .last_update_width = last_update_width_,
            .last_update_height = last_update_height_,
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

    // Test-only bounded physical-pixel readback for whole-surface scene
    // oracles. Bounded so a test cannot request an unbounded staging buffer.
    static constexpr std::uint64_t kMaxReadbackRegionPixels =
        4u * 1024u * 1024u;

    [[nodiscard]] bool request_readback_region(int x,
                                               int y,
                                               int width,
                                               int height) noexcept {
        readback_region_pending_ = false;
        readback_region_result_.reset();
        if (x < 0 || y < 0 || width <= 0 || height <= 0) return false;
        const auto pixels = static_cast<std::uint64_t>(width) *
                            static_cast<std::uint64_t>(height);
        if (pixels > kMaxReadbackRegionPixels) return false;
        readback_region_x_ = x;
        readback_region_y_ = y;
        readback_region_width_ = width;
        readback_region_height_ = height;
        readback_region_pending_ = true;
        return true;
    }

    [[nodiscard]] std::optional<PlatformReadbackRegion>
    take_readback_region() noexcept {
        auto result = std::move(readback_region_result_);
        readback_region_result_.reset();
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
        const bool needs_scene_update = !scene_valid_ || full_repaint_required_ ||
                                        scene_scale_ != scale_factor || ui.dirty();
        if (needs_scene_update && !ui.scene_partial_paint_supported()) {
            // Overlays and the optional inspector retain their existing full
            // UI preparation/render path until their additional scene layers
            // have a partial-reconstruction contract of their own.
            scene_valid_ = false;
            full_repaint_required_ = true;
            snapshot_.reset();
            auto* canvas = scene_->getCanvas();
            canvas->restoreToCount(1);
            canvas->resetMatrix();
            canvas->clear(SK_ColorBLACK);
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (fail_at(SceneFaultStage::AfterSceneReset)) return false;
#endif
            scene_frame_captured_ = true;
            const ScopeExit finish_capture{
                [this]() noexcept { scene_frame_captured_ = false; }};
            try {
                full_repaint_required_ = false;
                const SkAutoCanvasRestore restore_canvas{canvas, true};
                canvas->scale(scale_factor, scale_factor);
                ui.paint(*canvas, services);
            } catch (...) {
                invalidate();
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
            scene_uses_effects_ = true; // conservative for the legacy paths
            full_repaint_required_ = ui.dirty();
            deferred_redraw_pending_ |= full_repaint_required_;
            if (!full_repaint_required_) clear_pending_damage();
            remember_scene_update(SkIRect::MakeWH(physical_width, physical_height), false);
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            ++scene_builds_;
#endif
        } else if (needs_scene_update) {
            bool preparation_requires_full = false;
            if (!ui.prepare_scene_paint(services, preparation_requires_full)) {
                invalidate();
                return false;
            }

            auto transaction = ui.begin_scene_paint_transaction();
            if (!ui.scene_paint_transaction_valid(transaction)) {
                invalidate();
                return false;
            }
            scene_frame_captured_ = true;
            const ScopeExit finish_capture{
                [this]() noexcept { scene_frame_captured_ = false; }};

            bool full_update = !scene_valid_ || full_repaint_required_ ||
                               scene_scale_ != scale_factor ||
                               preparation_requires_full || scene_uses_effects_;
            std::optional<DeviceDamage> damage;
            if (!full_update) {
                if (!ui.scene_paint_transaction_has_damage(transaction) ||
                    !ui.scene_paint_transaction_damage_valid(transaction) ||
                    !pending_damage_valid_ ||
                    !covers(pending_damage_,
                            ui.scene_paint_transaction_damage(transaction))) {
                    full_update = true;
                } else {
                    damage = ::ui::detail::map_device_damage(
                        pending_damage_,
                        scale_factor, physical_width, physical_height);
                    if (!damage || damage->covers_scene) full_update = true;
                }
            }
            const SkIRect partial_clip = full_update
                ? SkIRect::MakeEmpty()
                : SkIRect::MakeLTRB(
                      damage->left, damage->top, damage->right, damage->bottom);

            snapshot_.reset(); // partial writes must not trigger stale COW snapshots
            scene_valid_ = false;
            full_repaint_required_ = true;
            auto* canvas = scene_->getCanvas();
            canvas->restoreToCount(1);
            canvas->resetMatrix();
            if (full_update) {
                canvas->clear(SK_ColorBLACK); // canonical opaque native background
            } else {
                const SkAutoCanvasRestore restore_region{canvas, true};
                canvas->resetMatrix();
                canvas->clipIRect(partial_clip, SkClipOp::kIntersect);
                SkPaint reset;
                reset.setColor(SK_ColorBLACK);
                reset.setBlendMode(SkBlendMode::kSrc);
                reset.setAntiAlias(false);
                canvas->drawRect(SkRect::Make(partial_clip), reset);
            }
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (fail_at(SceneFaultStage::AfterSceneReset)) return false;
#endif

            bool used_effects = false;
            bool painted = false;
            try {
                const SkAutoCanvasRestore restore_canvas{canvas, true};
                if (full_update) {
                    canvas->scale(scale_factor, scale_factor);
                    painted = ui.paint_full_scene_prepared(
                        transaction, *canvas, services, used_effects);
                } else {
                    canvas->resetMatrix();
                    canvas->clipIRect(partial_clip, SkClipOp::kIntersect);
                    canvas->scale(scale_factor, scale_factor);
                    painted = ui.paint_partial_scene_prepared(
                        transaction, *canvas, services,
                        damage->logical_query, used_effects);
                }
            } catch (...) {
                invalidate();
                throw;
            }
            if (!painted) {
                invalidate();
                return false;
            }
            if (!full_update && used_effects) {
                // Filter input can lie outside the output clip. Preserve the
                // captured generation and rebuild from the full reference path.
                invalidate();
                deferred_redraw_pending_ = true;
                return false;
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

            ui.commit_scene_paint(transaction);
            scene_scale_ = scale_factor;
            scene_valid_ = true;
            scene_uses_effects_ = full_update ? used_effects
                                               : scene_uses_effects_ || used_effects;
            full_repaint_required_ = ui.dirty();
            deferred_redraw_pending_ |= full_repaint_required_;
            if (!full_repaint_required_) clear_pending_damage();
            const auto updated = full_update
                ? SkIRect::MakeWH(physical_width, physical_height)
                : partial_clip;
            remember_scene_update(updated, !full_update);
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            ++scene_builds_;
#endif
        } else if (!ui.dirty()) {
            clear_pending_damage();
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
        present_pending_ = ui.dirty();
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
        if (readback_region_pending_) {
            // Serve the request only on a successful presented frame so the
            // caller observes the same committed scene it would present. The
            // staging allocation is test-only and failure-contained; it never
            // changes scene/presentation state.
            readback_region_pending_ = false;
            readback_region_result_.reset();
            try {
                if (width_ > 0 && height_ > 0) {
                    const int x = std::clamp(readback_region_x_, 0, width_ - 1);
                    const int y = std::clamp(readback_region_y_, 0, height_ - 1);
                    const int region_width =
                        std::clamp(readback_region_width_, 0, width_ - x);
                    const int region_height =
                        std::clamp(readback_region_height_, 0, height_ - y);
                    if (region_width > 0 && region_height > 0) {
                        PlatformReadbackRegion region{};
                        region.x = x;
                        region.y = y;
                        region.width = region_width;
                        region.height = region_height;
                        region.pixels.resize(
                            static_cast<std::size_t>(region_width) *
                            static_cast<std::size_t>(region_height));
                        const auto region_info = SkImageInfo::Make(
                            region_width,
                            region_height,
                            kRGBA_8888_SkColorType,
                            kPremul_SkAlphaType,
                            SkColorSpace::MakeSRGB());
                        if (presentation_->readPixels(
                                region_info,
                                region.pixels.data(),
                                static_cast<std::size_t>(region_width) *
                                    sizeof(PlatformReadbackPixel),
                                x,
                                y)) {
                            readback_region_result_ = std::move(region);
                        }
                    }
                }
            } catch (...) {
                readback_region_result_.reset();
            }
        }
#endif
        return true;
    }

private:
    void clear_pending_damage() noexcept {
        pending_damage_ = {};
        pending_damage_valid_ = false;
    }

    void remember_scene_update(const SkIRect& update, bool partial) noexcept {
        last_update_x_ = update.left();
        last_update_y_ = update.top();
        last_update_width_ = update.width();
        last_update_height_ = update.height();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
        if (partial) ++partial_scene_updates_;
#else
        (void)partial;
#endif
    }

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
    Rect pending_damage_{};
    bool pending_damage_valid_{};
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
    std::uint64_t scene_builds_{};
    std::uint64_t partial_scene_updates_{};
    std::uint64_t presentations_{};
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
