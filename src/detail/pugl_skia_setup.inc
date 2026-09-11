#include <nativeui/nativeui.hpp>

#include "view_geometry.hpp"

#include <pugl/gl.h>
#include <pugl/pugl.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <new>
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

PuglSpan to_pugl_span(float physical) {
    return static_cast<PuglSpan>(detail::physical_to_pugl_view_span(physical));
}

[[noreturn]] void throw_pugl(PuglStatus status, const char* what) {
    throw std::runtime_error(std::string(what) + ": " + puglStrerror(status));
}

sk_sp<const GrGLInterface> make_skia_gl_interface() {
#if defined(__linux__)
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
    void reset() {
        surface_.reset();
        if (context_) {
            context_->abandonContext();
            context_.reset();
        }
        interface_.reset();
        width_ = 0;
        height_ = 0;
    }

    bool render(UI& ui,
                PlatformServices& services,
                Size physical_size,
                float scale_factor) {
        // Pugl geometry is expressed in physical pixels.  The toolkit itself
        // uses logical coordinates, so the framebuffer keeps the Pugl size
        // while the SkCanvas is scaled once for component painting.
        const int physical_width = std::max(1, static_cast<int>(std::lround(physical_size.w)));
        const int physical_height = std::max(1, static_cast<int>(std::lround(physical_size.h)));

        glViewport(0, 0, physical_width, physical_height);

        if (!context_) {
            interface_ = make_skia_gl_interface();
            if (!interface_) return false;
            context_ = GrDirectContexts::MakeGL(interface_);
            if (!context_) return false;
        }

        if (!surface_ || width_ != physical_width || height_ != physical_height) {
            // Query the Pugl-owned default framebuffer instead of assuming FBO 0.
            // Numeric constants keep this code compatible with the conservative
            // system GL headers used by Windows.
            constexpr GLenum kFramebufferBinding = 0x8CA6;
            constexpr GLenum kStencilBits = 0x0D57;
            constexpr GLenum kSamples = 0x80A9;
            GLint framebuffer = 0;
            GLint stencil_bits = 0;
            GLint samples = 0;
            glGetIntegerv(kFramebufferBinding, &framebuffer);
            glGetIntegerv(kStencilBits, &stencil_bits);
            glGetIntegerv(kSamples, &samples);

            GrGLFramebufferInfo framebuffer_info{};
            framebuffer_info.fFBOID = static_cast<GrGLuint>(framebuffer);
            framebuffer_info.fFormat = kGlRgba8;

            auto backend = GrBackendRenderTargets::MakeGL(
                physical_width,
                physical_height,
                std::max(0, static_cast<int>(samples)),
                std::max(0, static_cast<int>(stencil_bits)),
                framebuffer_info);

            surface_ = SkSurfaces::WrapBackendRenderTarget(
                context_.get(),
                backend,
                kBottomLeft_GrSurfaceOrigin,
                kRGBA_8888_SkColorType,
                SkColorSpace::MakeSRGB(),
                nullptr);

            if (!surface_) return false;
            width_ = physical_width;
            height_ = physical_height;
        }

        auto* canvas = surface_->getCanvas();
        canvas->clear(SK_ColorBLACK);
        canvas->save();
        canvas->scale(scale_factor, scale_factor);
        ui.paint(*canvas, services);
        canvas->restore();
        context_->flushAndSubmit(surface_.get());
        return true;
    }

private:
    sk_sp<const GrGLInterface> interface_;
    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
    int width_{};
    int height_{};
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

    if (key == ' ') return Key::Space;
    if (key == 'a' || key == 'A') return Key::A;
    if (key == 'c' || key == 'C') return Key::C;
    if (key == 'v' || key == 'V') return Key::V;
    if (key == 'x' || key == 'X') return Key::X;
    if (key == 'y' || key == 'Y') return Key::Y;
    if (key == 'z' || key == 'Z') return Key::Z;
    if (primary && (key == 'q' || key == 'Q')) return Key::Quit;
    return Key::None;
}

void apply_modifiers(InputEvent& out, PuglMods state) {
    out.shift = (state & PUGL_MOD_SHIFT) != 0;
    out.ctrl = (state & PUGL_MOD_CTRL) != 0;
    out.alt = (state & PUGL_MOD_ALT) != 0;
    out.gui = (state & PUGL_MOD_SUPER) != 0;
    out.primary = platform_primary_modifier(state);
}
