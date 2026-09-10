#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#  import <ApplicationServices/ApplicationServices.h>
#  import <Cocoa/Cocoa.h>
#elif defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <windowsx.h>
#elif defined(__linux__)
#  include <X11/Xlib.h>
#  include <X11/extensions/XTest.h>
#  undef None
#endif

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui t044] " << stage << ": " << message << '\n';
    return 1;
}

bool expect(bool condition, std::string_view stage, std::string_view message) {
    if (condition) return true;
    (void)fail(stage, message);
    return false;
}

bool step(bool condition, std::string_view stage, std::string_view operation) {
    if (condition) return true;
    std::cerr << "[nativeui t044] " << stage << ": native driver step failed: "
              << operation << '\n';
    return false;
}

struct CaptureState final {
    int down{};
    int move{};
    int drag_move{};
    int outside_drag_move{};
    int up{};
    int cancel{};
};

class CaptureProbeComponent final : public ui::Component {
public:
    explicit CaptureProbeComponent(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {160.0f, 100.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            dragging_ = true;
            ++state_->down;
            context.capture_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++state_->move;
            if (dragging_) {
                ++state_->drag_move;
                if (!context.bounds().contains(event.position)) ++state_->outside_drag_move;
            }
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            if (!dragging_) return ui::EventResult::Ignored;
            dragging_ = false;
            ++state_->up;
            context.release_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            if (!dragging_) return ui::EventResult::Ignored;
            dragging_ = false;
            ++state_->cancel;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<CaptureState> state_;
    bool dragging_{};
};

class CaptureProbe final {
public:
    explicit CaptureProbe(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state] { return std::make_unique<CaptureProbeComponent>(state); }, {}};
    }

private:
    std::shared_ptr<CaptureState> state_;
};

bool pump(ui::Application& application, int iterations = 8) {
    for (int i = 0; i < iterations; ++i) {
        if (!application.poll(0.005)) {
            std::cerr << "[nativeui t044] event-pump: "
                      << (application.last_error().empty()
                              ? "application stopped unexpectedly"
                              : std::string{application.last_error()})
                      << '\n';
            return false;
        }
    }
    return true;
}

class NativePointerDriver final {
public:
#if defined(__linux__)
    NativePointerDriver() : display_(XOpenDisplay(nullptr)) {}
    ~NativePointerDriver() {
        if (display_) XCloseDisplay(display_);
    }
#else
    NativePointerDriver() = default;
#endif

    NativePointerDriver(const NativePointerDriver&) = delete;
    NativePointerDriver& operator=(const NativePointerDriver&) = delete;

    [[nodiscard]] bool valid() const noexcept {
#if defined(__linux__)
        return display_ != nullptr;
#else
        return true;
#endif
    }

    bool focus(ui::StandaloneWindow& window) {
#if defined(__APPLE__)
        NSView* view = native_view(window);
        NSWindow* native_window = view ? [view window] : nil;
        if (!native_window) return false;
        [native_window makeKeyAndOrderFront:nil];
        return true;
#elif defined(_WIN32)
        HWND native_window = hwnd(window);
        if (!native_window) return false;
        ShowWindow(native_window, SW_SHOW);
        (void)SetForegroundWindow(native_window);
        (void)SetFocus(native_window);
        return GetFocus() == native_window;
#elif defined(__linux__)
        if (!display_) return false;
        const Window native_window = xwindow(window);
        if (!native_window) return false;
        XRaiseWindow(display_, native_window);
        XSetInputFocus(display_, native_window, RevertToParent, CurrentTime);
        XSync(display_, False);
        return true;
#else
        (void)window;
        return false;
#endif
    }

    bool pointer_down(ui::StandaloneWindow& window) {
#if defined(__APPLE__)
        return post_mouse(window, kCGEventMouseMoved, false, false) &&
               post_mouse(window, kCGEventLeftMouseDown, false, true);
#elif defined(_WIN32)
        const HWND native_window = hwnd(window);
        if (!native_window) return false;
        return PostMessageW(native_window, WM_MOUSEMOVE, 0, MAKELPARAM(24, 24)) != FALSE &&
               PostMessageW(native_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 24)) != FALSE;
#elif defined(__linux__)
        Geometry geometry{};
        if (!geometry_for(window, geometry)) return false;
        const int x = geometry.root_x + std::min(24, std::max(1, geometry.width - 1));
        const int y = geometry.root_y + std::min(24, std::max(1, geometry.height - 1));
        const Bool moved = XTestFakeMotionEvent(display_, DefaultScreen(display_), x, y, CurrentTime);
        const Bool pressed = XTestFakeButtonEvent(display_, 1, True, CurrentTime);
        XFlush(display_);
        return moved && pressed;
#else
        (void)window;
        return false;
#endif
    }

    bool drag_outside(ui::StandaloneWindow& window, int extra) {
#if defined(__APPLE__)
        return post_mouse(window, kCGEventLeftMouseDragged, true, true, extra);
#elif defined(_WIN32)
        RECT rect{};
        const HWND native_window = hwnd(window);
        if (!native_window || !GetClientRect(native_window, &rect)) return false;
        const int x = (rect.right - rect.left) + 48 + extra;
        const int y = (rect.bottom - rect.top) + 48 + extra;
        return PostMessageW(native_window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y)) != FALSE;
#elif defined(__linux__)
        Geometry geometry{};
        if (!geometry_for(window, geometry)) return false;
        int x = geometry.root_x + geometry.width + 48 + extra;
        int y = geometry.root_y + geometry.height + 48 + extra;
        x = std::clamp(x, 0, DisplayWidth(display_, DefaultScreen(display_)) - 1);
        y = std::clamp(y, 0, DisplayHeight(display_, DefaultScreen(display_)) - 1);
        const Bool moved = XTestFakeMotionEvent(display_, DefaultScreen(display_), x, y, CurrentTime);
        XFlush(display_);
        return moved;
#else
        (void)window;
        (void)extra;
        return false;
#endif
    }

    bool pointer_up_outside(ui::StandaloneWindow& window) {
#if defined(__APPLE__)
        return post_mouse(window, kCGEventLeftMouseUp, true, false, 64);
#elif defined(_WIN32)
        RECT rect{};
        const HWND native_window = hwnd(window);
        if (!native_window || !GetClientRect(native_window, &rect)) return false;
        const int x = (rect.right - rect.left) + 64;
        const int y = (rect.bottom - rect.top) + 64;
        return PostMessageW(native_window, WM_LBUTTONUP, 0, MAKELPARAM(x, y)) != FALSE;
#elif defined(__linux__)
        (void)window;
        if (!display_) return false;
        const Bool released = XTestFakeButtonEvent(display_, 1, False, CurrentTime);
        XFlush(display_);
        return released;
#else
        (void)window;
        return false;
#endif
    }

    bool move_inside_while_held(ui::StandaloneWindow& window) {
#if defined(__APPLE__)
        return post_mouse(window, kCGEventLeftMouseDragged, false, true);
#elif defined(_WIN32)
        const HWND native_window = hwnd(window);
        return native_window &&
               PostMessageW(native_window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(24, 24)) != FALSE;
#elif defined(__linux__)
        Geometry geometry{};
        if (!geometry_for(window, geometry)) return false;
        const int x = geometry.root_x + std::min(24, std::max(1, geometry.width - 1));
        const int y = geometry.root_y + std::min(24, std::max(1, geometry.height - 1));
        const Bool moved = XTestFakeMotionEvent(display_, DefaultScreen(display_), x, y, CurrentTime);
        XFlush(display_);
        return moved;
#else
        (void)window;
        return false;
#endif
    }

    bool release_button(ui::StandaloneWindow& window) {
#if defined(__APPLE__)
        return post_mouse(window, kCGEventLeftMouseUp, false, false);
#elif defined(_WIN32)
        const HWND native_window = hwnd(window);
        return native_window && PostMessageW(native_window, WM_LBUTTONUP, 0, MAKELPARAM(24, 24)) != FALSE;
#elif defined(__linux__)
        (void)window;
        if (!display_) return false;
        const Bool released = XTestFakeButtonEvent(display_, 1, False, CurrentTime);
        XFlush(display_);
        return released;
#else
        (void)window;
        return false;
#endif
    }

    [[nodiscard]] bool capture_owned_by(const ui::StandaloneWindow& window) const noexcept {
#if defined(_WIN32)
        return GetCapture() == hwnd(window);
#else
        (void)window;
        return true;
#endif
    }

    [[nodiscard]] bool capture_clear() const noexcept {
#if defined(_WIN32)
        return GetCapture() == nullptr;
#else
        return true;
#endif
    }

private:
#if defined(__APPLE__)
    [[nodiscard]] static NSView* native_view(const ui::StandaloneWindow& window) noexcept {
        return (__bridge NSView*)(reinterpret_cast<void*>(window.native_handle()));
    }

    static bool post_mouse(ui::StandaloneWindow& window,
                           CGEventType type,
                           bool outside,
                           bool held,
                           int extra = 0) {
        NSView* view = native_view(window);
        NSWindow* native_window = view ? [view window] : nil;
        NSScreen* screen = native_window ? [native_window screen] : nil;
        if (!view || !native_window || !screen) return false;

        const NSRect bounds = [view bounds];
        NSPoint local = NSMakePoint(24.0, 24.0);
        if (outside) {
            local = NSMakePoint(NSMaxX(bounds) + 48.0 + extra,
                                NSMaxY(bounds) + 48.0 + extra);
        }
        const NSPoint in_window = [view convertPoint:local toView:nil];
        const NSPoint cocoa_screen = [native_window convertPointToScreen:in_window];
        const CGRect display_bounds = CGDisplayBounds(CGMainDisplayID());
        const CGPoint quartz_point = CGPointMake(
            std::clamp<double>(cocoa_screen.x,
                               CGRectGetMinX(display_bounds),
                               CGRectGetMaxX(display_bounds) - 1.0),
            std::clamp<double>(CGRectGetMaxY(display_bounds) - cocoa_screen.y,
                               CGRectGetMinY(display_bounds),
                               CGRectGetMaxY(display_bounds) - 1.0));

        CGEventRef event = CGEventCreateMouseEvent(nullptr, type, quartz_point, kCGMouseButtonLeft);
        if (!event) return false;
        if (held) CGEventSetIntegerValueField(event, kCGMouseEventButtonNumber, 0);
        CGEventPost(kCGSessionEventTap, event);
        CFRelease(event);
        return true;
    }
#elif defined(_WIN32)
    [[nodiscard]] static HWND hwnd(const ui::StandaloneWindow& window) noexcept {
        return reinterpret_cast<HWND>(window.native_handle());
    }
#elif defined(__linux__)
    struct Geometry final {
        int root_x{};
        int root_y{};
        int width{};
        int height{};
    };

    [[nodiscard]] static Window xwindow(const ui::StandaloneWindow& window) noexcept {
        return static_cast<Window>(window.native_handle());
    }

    bool geometry_for(const ui::StandaloneWindow& window, Geometry& out) const {
        if (!display_) return false;
        const Window native_window = xwindow(window);
        if (!native_window) return false;

        XWindowAttributes attributes{};
        if (!XGetWindowAttributes(display_, native_window, &attributes)) return false;
        Window child{};
        int root_x = 0;
        int root_y = 0;
        if (!XTranslateCoordinates(display_, native_window, DefaultRootWindow(display_),
                                   0, 0, &root_x, &root_y, &child)) {
            return false;
        }
        out.root_x = root_x;
        out.root_y = root_y;
        out.width = std::max(1, attributes.width);
        out.height = std::max(1, attributes.height);
        return true;
    }

    Display* display_{};
#endif
};

bool run_outside_sequence(ui::Application& application,
                          NativePointerDriver& driver,
                          ui::StandaloneWindow& window,
                          const std::shared_ptr<CaptureState>& state,
                          std::string_view stage) {
    const int down_before = state->down;
    const int drag_before = state->drag_move;
    const int outside_before = state->outside_drag_move;
    const int up_before = state->up;
    const int cancel_before = state->cancel;

    if (!step(driver.focus(window), stage, "focus") || !pump(application)) return false;
    if (!step(driver.pointer_down(window), stage, "pointer-down") || !pump(application)) return false;
    if (!expect(state->down == down_before + 1, stage, "pointer down was not delivered")) return false;
    if (!expect(driver.capture_owned_by(window), stage, "native capture is not owned by the pressed view")) {
        return false;
    }

    if (!step(driver.drag_outside(window, 0), stage, "first outside motion") || !pump(application)) return false;
    if (!step(driver.drag_outside(window, 20), stage, "second outside motion") || !pump(application)) return false;
    if (!expect(state->drag_move >= drag_before + 2, stage, "outside drag motion was lost")) return false;
    if (!expect(state->outside_drag_move >= outside_before + 2,
                stage,
                "drag events were not delivered with out-of-view coordinates")) {
        return false;
    }

    if (!step(driver.pointer_up_outside(window), stage, "outside pointer-up") || !pump(application)) return false;
    if (!expect(state->up == up_before + 1, stage, "outside pointer up was lost")) return false;
    if (!expect(state->cancel == cancel_before, stage, "normal outside release synthesized cancel")) return false;
    if (!expect(driver.capture_clear(), stage, "native capture remained after pointer up")) return false;
    return true;
}

} // namespace

int main() {
    ui::Application application;
    if (!application.valid()) return fail("application", application.last_error());
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

    auto a_state = std::make_shared<CaptureState>();
    auto b_state = std::make_shared<CaptureState>();
    ui::UI a_ui{CaptureProbe{a_state}};
    ui::UI b_ui{CaptureProbe{b_state}};

    auto a = std::make_unique<ui::StandaloneWindow>(
        application, a_ui,
        ui::WindowDesc{.title = "NativeUI T044 A", .size = {240.0f, 160.0f}, .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        application, b_ui,
        ui::WindowDesc{.title = "NativeUI T044 B", .size = {240.0f, 160.0f}, .resizable = true});
    if (!a->valid()) return fail("create-a", a->last_error());
    if (!b->valid()) return fail("create-b", b->last_error());

    NativePointerDriver driver;
    if (!driver.valid()) return fail("native-driver", "native input driver is unavailable");
    if (!pump(application, 16)) return 1;

    if (!run_outside_sequence(application, driver, *a, a_state, "outside-a")) return 1;
    if (!expect(b_state->drag_move == 0 && b_state->up == 0,
                "isolation-a", "view B observed view A's captured drag")) return 1;
    if (!run_outside_sequence(application, driver, *b, b_state, "outside-b")) return 1;

    const int a_cancel_before = a_state->cancel;
    const int a_up_before = a_state->up;
    if (!step(driver.focus(*a), "focus-loss", "focus A") || !pump(application)) return 1;
    if (!step(driver.pointer_down(*a), "focus-loss", "pointer-down A") || !pump(application)) return 1;
    if (!expect(driver.capture_owned_by(*a), "focus-loss", "A did not own native capture")) return 1;
    if (!step(driver.focus(*b), "focus-loss", "focus B") || !pump(application)) return 1;
    if (!expect(a_state->cancel == a_cancel_before + 1,
                "focus-loss", "focus loss did not cancel toolkit capture exactly once")) return 1;
    if (!expect(driver.capture_clear(), "focus-loss", "native capture remained after focus loss")) return 1;

#if defined(__linux__)
    const int b_move_before = b_state->move;
    if (!step(driver.move_inside_while_held(*b), "focus-loss-x11", "move inside B while held") ||
        !pump(application)) return 1;
    if (!expect(b_state->move > b_move_before,
                "focus-loss-x11", "motion remained grabbed by A after A lost focus")) return 1;
#endif

    if (!step(driver.release_button(*b), "focus-loss", "release button") || !pump(application)) return 1;
    if (!expect(a_state->up == a_up_before,
                "focus-loss", "cancelled capture later received a duplicate pointer up")) return 1;

    auto c_state = std::make_shared<CaptureState>();
    auto c_ui = std::make_unique<ui::UI>(CaptureProbe{c_state});
    auto c = std::make_unique<ui::StandaloneWindow>(
        application, *c_ui,
        ui::WindowDesc{.title = "NativeUI T044 C", .size = {220.0f, 150.0f}, .resizable = true});
    if (!c->valid()) return fail("create-c", c->last_error());
    if (!step(driver.focus(*c), "destroy-capture", "focus C") || !pump(application)) return 1;
    if (!step(driver.pointer_down(*c), "destroy-capture", "pointer-down C") || !pump(application)) return 1;
    if (!expect(c_state->down == 1, "destroy-capture", "C did not receive pointer down")) return 1;
    c.reset();
    c_ui.reset();
    if (!pump(application)) return 1;
    if (!expect(driver.capture_clear(), "destroy-capture", "native capture survived view destruction")) return 1;
#if defined(__linux__)
    const int b_move_after_destroy = b_state->move;
    if (!step(driver.move_inside_while_held(*b), "destroy-capture-x11", "move inside B after destroy") ||
        !pump(application)) return 1;
    if (!expect(b_state->move > b_move_after_destroy,
                "destroy-capture-x11", "destroyed view retained the X11 pointer grab")) return 1;
#endif
    if (!step(driver.release_button(*b), "destroy-capture", "release button") || !pump(application)) return 1;

    for (int iteration = 0; iteration < 32; ++iteration) {
        if (!run_outside_sequence(application, driver, *a, a_state, "stress-a")) return 1;
    }

    std::cout << "T044 native pointer capture smoke passed\n";
    return 0;
}
