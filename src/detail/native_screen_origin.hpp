#pragma once

#include <nativeui/geometry.hpp>

#include <cmath>
#include <cstdint>
#include <optional>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  undef None
#elif defined(__APPLE__)
#  include <CoreGraphics/CGGeometry.h>
#  include <objc/message.h>
#  include <objc/runtime.h>
#endif

namespace ui::detail {

/// Lifetime-neutral native handles needed to resolve one realized child view's
/// physical top-left screen origin. Callers must capture these while the owning
/// view is live and invoke the query only after their independent view-lifetime
/// gate has proved the native endpoint still exists.
struct NativeScreenOriginCaptureSource final {
    std::uintptr_t native_view{};
    void* native_world{};
};

#if defined(__APPLE__)
namespace appkit_screen_origin_detail {

[[nodiscard]] inline id send_object(id receiver, const char* selector_name) noexcept {
    if (!receiver || !selector_name) return nullptr;
    const auto selector = sel_registerName(selector_name);
    return reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(receiver, selector);
}

[[nodiscard]] inline CGFloat send_scalar(id receiver, const char* selector_name) noexcept {
    if (!receiver || !selector_name) return 0.0;
    const auto selector = sel_registerName(selector_name);
    return reinterpret_cast<CGFloat (*)(id, SEL)>(objc_msgSend)(receiver, selector);
}

[[nodiscard]] inline CGPoint send_point_to_view(id receiver,
                                                const char* selector_name,
                                                CGPoint point,
                                                id target) noexcept {
    const auto selector = sel_registerName(selector_name);
    return reinterpret_cast<CGPoint (*)(id, SEL, CGPoint, id)>(objc_msgSend)(
        receiver, selector, point, target);
}

[[nodiscard]] inline CGPoint send_point(id receiver,
                                        const char* selector_name,
                                        CGPoint point) noexcept {
    const auto selector = sel_registerName(selector_name);
    return reinterpret_cast<CGPoint (*)(id, SEL, CGPoint)>(objc_msgSend)(
        receiver, selector, point);
}

[[nodiscard]] inline CGRect send_rect(id receiver, const char* selector_name) noexcept {
    const auto selector = sel_registerName(selector_name);
#if defined(__x86_64__) || defined(__i386__)
    CGRect result{};
    reinterpret_cast<void (*)(CGRect*, id, SEL)>(objc_msgSend_stret)(
        &result, receiver, selector);
    return result;
#else
    return reinterpret_cast<CGRect (*)(id, SEL)>(objc_msgSend)(receiver, selector);
#endif
}

} // namespace appkit_screen_origin_detail
#endif

/// Query the platform for the current physical screen origin of a realized
/// native child view. This is intentionally a narrow, read-only boundary: it
/// retains no native object, dispatches no application callback and never falls
/// back to parent-relative coordinates on failure.
///
/// The returned coordinate system matches Pugl's physical configure convention:
/// Win32 ClientToScreen and X11 XTranslateCoordinates already report physical
/// root/screen pixels; AppKit converts the flipped child view's local top-left
/// through its NSWindow to Cocoa screen points, applies the current screen's
/// backing scale exactly once, then mirrors Pugl's top-down main-screen Y basis.
[[nodiscard]] inline std::optional<Point> capture_native_physical_screen_origin(
    NativeScreenOriginCaptureSource source) noexcept {
#if defined(_WIN32)
    if (!source.native_view) return std::nullopt;

    POINT origin{};
    const auto window = reinterpret_cast<HWND>(source.native_view);
    if (!ClientToScreen(window, &origin)) return std::nullopt;
    return Point{static_cast<float>(origin.x), static_cast<float>(origin.y)};
#elif defined(__linux__)
    auto* display = static_cast<Display*>(source.native_world);
    const Window window = static_cast<Window>(source.native_view);
    if (!display || !window) return std::nullopt;

    const Window root = DefaultRootWindow(display);
    if (!root) return std::nullopt;

    int screen_x = 0;
    int screen_y = 0;
    Window child{};
    if (!XTranslateCoordinates(
            display, window, root, 0, 0, &screen_x, &screen_y, &child)) {
        return std::nullopt;
    }
    return Point{static_cast<float>(screen_x), static_cast<float>(screen_y)};
#elif defined(__APPLE__)
    if (!source.native_view) return std::nullopt;

    using namespace appkit_screen_origin_detail;
    id const view = reinterpret_cast<id>(source.native_view);
    id const window = send_object(view, "window");
    id const screen = send_object(window, "screen");
    id const screen_class = reinterpret_cast<id>(objc_getClass("NSScreen"));
    id const main_screen = send_object(screen_class, "mainScreen");
    if (!window || !screen || !main_screen) return std::nullopt;

    const CGFloat scale = send_scalar(screen, "backingScaleFactor");
    if (!std::isfinite(static_cast<double>(scale)) || scale <= 0.0) {
        return std::nullopt;
    }

    // Pugl's Cocoa wrapper view is flipped, so local {0, 0} is the child
    // content's top-left. Convert it in point space before applying scale so
    // mixed-DPI screen translation is performed by AppKit rather than guessed.
    const CGPoint window_point = send_point_to_view(
        view, "convertPoint:toView:", CGPoint{0.0, 0.0}, nullptr);
    const CGPoint screen_point = send_point(
        window, "convertPointToScreen:", window_point);
    const CGRect main_frame = send_rect(main_screen, "frame");

    const double screen_x = static_cast<double>(screen_point.x) * scale;
    const double screen_y =
        (static_cast<double>(main_frame.size.height) -
         static_cast<double>(screen_point.y)) * scale;
    if (!std::isfinite(screen_x) || !std::isfinite(screen_y)) {
        return std::nullopt;
    }

    const Point result{static_cast<float>(screen_x), static_cast<float>(screen_y)};
    if (!std::isfinite(result.x) || !std::isfinite(result.y)) {
        return std::nullopt;
    }
    return result;
#else
    (void)source;
    return std::nullopt;
#endif
}

} // namespace ui::detail
