#include <nativeui/nativeui.hpp>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#undef None

#include <algorithm>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

struct CaptureState final {
    int down{};
    int move{};
    int outside_move{};
    int up{};
    int cancel{};
};

class CaptureProbe final : public ui::Component {
public:
    explicit CaptureProbe(std::shared_ptr<CaptureState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {180.0f, 120.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            dragging_ = true;
            ++state_->down;
            context.capture_pointer();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            if (dragging_) {
                ++state_->move;
                if (!context.bounds().contains(event.position)) ++state_->outside_move;
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

ui::Spec probe_spec(const std::shared_ptr<CaptureState>& state) {
    return ui::Spec{[state] { return std::make_unique<CaptureProbe>(state); }, {}};
}

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui t044 x11] " << stage << ": " << message << '\n';
    return 1;
}

bool expect(bool condition, std::string_view stage, std::string_view message) {
    if (condition) return true;
    (void)fail(stage, message);
    return false;
}

bool pump(ui::Application& app, int count = 10) {
    for (int i = 0; i < count; ++i) {
        if (!app.poll(0.005)) {
            std::cerr << "[nativeui t044 x11] pump: "
                      << (app.last_error().empty() ? "application stopped" : app.last_error())
                      << '\n';
            return false;
        }
    }
    return true;
}

class X11Driver final {
public:
    X11Driver() : display_(XOpenDisplay(nullptr)) {}
    ~X11Driver() {
        if (display_) XCloseDisplay(display_);
    }

    [[nodiscard]] bool valid() const noexcept { return display_ != nullptr; }

    [[nodiscard]] static Window native_window(const ui::StandaloneWindow& window) noexcept {
        return static_cast<Window>(window.native_handle());
    }

    bool activate(const ui::StandaloneWindow& window) {
        if (!display_) return false;
        const Window target = native_window(window);
        if (!target) return false;
        XMapRaised(display_, target);
        XSetInputFocus(display_, target, RevertToParent, CurrentTime);
        XWarpPointer(display_, 0, target, 0, 0, 0, 0, 24, 24);
        XSync(display_, False);
        return pointer_is_inside(target);
    }

    bool press_inside(const ui::StandaloneWindow& window) {
        if (!activate(window)) return false;
        if (!XTestFakeButtonEvent(display_, 1, True, CurrentTime)) return false;
        XSync(display_, False);
        return true;
    }

    bool move_outside(const ui::StandaloneWindow& window, int extra = 0) {
        Geometry g{};
        if (!geometry(window, g)) return false;
        const int screen = DefaultScreen(display_);
        const int x = std::clamp(g.root_x + g.width + 60 + extra,
                                 0,
                                 DisplayWidth(display_, screen) - 1);
        const int y = std::clamp(g.root_y + g.height + 60 + extra,
                                 0,
                                 DisplayHeight(display_, screen) - 1);
        if (!XTestFakeMotionEvent(display_, screen, x, y, CurrentTime)) return false;
        XSync(display_, False);
        return true;
    }

    bool move_inside(const ui::StandaloneWindow& window) {
        if (!display_) return false;
        const Window target = native_window(window);
        if (!target) return false;
        XWarpPointer(display_, 0, target, 0, 0, 0, 0, 24, 24);
        XSync(display_, False);
        return pointer_is_inside(target);
    }

    bool release() {
        if (!display_) return false;
        if (!XTestFakeButtonEvent(display_, 1, False, CurrentTime)) return false;
        XSync(display_, False);
        return true;
    }

private:
    struct Geometry final {
        int root_x{};
        int root_y{};
        int width{};
        int height{};
    };

    bool geometry(const ui::StandaloneWindow& window, Geometry& out) const {
        if (!display_) return false;
        const Window target = native_window(window);
        XWindowAttributes attrs{};
        if (!target || !XGetWindowAttributes(display_, target, &attrs)) return false;
        Window child{};
        if (!XTranslateCoordinates(display_, target, DefaultRootWindow(display_), 0, 0,
                                   &out.root_x, &out.root_y, &child)) {
            return false;
        }
        out.width = std::max(1, attrs.width);
        out.height = std::max(1, attrs.height);
        return true;
    }

    bool pointer_is_inside(Window expected) const {
        Window root{};
        Window child{};
        int root_x{};
        int root_y{};
        int win_x{};
        int win_y{};
        unsigned int mask{};
        if (!XQueryPointer(display_, expected, &root, &child,
                           &root_x, &root_y, &win_x, &win_y, &mask)) {
            return false;
        }
        XWindowAttributes attrs{};
        if (!XGetWindowAttributes(display_, expected, &attrs)) return false;
        return win_x >= 0 && win_y >= 0 && win_x < attrs.width && win_y < attrs.height;
    }

    Display* display_{};
};

bool outside_release_cycle(ui::Application& app,
                           X11Driver& driver,
                           ui::StandaloneWindow& window,
                           const std::shared_ptr<CaptureState>& state,
                           std::string_view stage) {
    const int down = state->down;
    const int move = state->move;
    const int outside = state->outside_move;
    const int up = state->up;
    const int cancel = state->cancel;

    if (!driver.press_inside(window) || !pump(app)) return false;
    if (!expect(state->down == down + 1, stage, "pointer down not delivered")) return false;
    if (!driver.move_outside(window) || !pump(app)) return false;
    if (!driver.move_outside(window, 20) || !pump(app)) return false;
    if (!expect(state->move >= move + 2, stage, "captured outside motion was lost")) return false;
    if (!expect(state->outside_move >= outside + 2, stage, "outside coordinates were not delivered")) return false;
    if (!driver.release() || !pump(app)) return false;
    if (!expect(state->up == up + 1, stage, "outside release was lost")) return false;
    return expect(state->cancel == cancel, stage, "normal release synthesized cancel");
}

} // namespace

int main() {
    ui::Application app;
    if (!app.valid()) return fail("application", app.last_error());
    app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

    auto a_state = std::make_shared<CaptureState>();
    auto b_state = std::make_shared<CaptureState>();
    ui::UI a_ui{probe_spec(a_state)};
    ui::UI b_ui{probe_spec(b_state)};
    auto a = std::make_unique<ui::StandaloneWindow>(
        app, a_ui, ui::WindowDesc{.title = "T044 X11 A", .size = {260.0f, 180.0f}, .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        app, b_ui, ui::WindowDesc{.title = "T044 X11 B", .size = {260.0f, 180.0f}, .resizable = true});
    if (!a->valid()) return fail("window-a", a->last_error());
    if (!b->valid()) return fail("window-b", b->last_error());

    X11Driver driver;
    if (!driver.valid()) return fail("driver", "XOpenDisplay failed");
    if (!pump(app, 16)) return 1;

    if (!outside_release_cycle(app, driver, *a, a_state, "outside-a")) return 1;
    if (!expect(b_state->down == 0 && b_state->move == 0 && b_state->up == 0,
                "isolation-a", "view B observed view A capture")) return 1;
    if (!outside_release_cycle(app, driver, *b, b_state, "outside-b")) return 1;

    const int cancel_before = a_state->cancel;
    const int up_before = a_state->up;
    if (!driver.press_inside(*a) || !pump(app)) return 1;
    if (!driver.activate(*b) || !pump(app)) return 1;
    if (!expect(a_state->cancel == cancel_before + 1,
                "focus-loss", "focus loss did not cancel retained capture")) return 1;
    const int b_move_before = b_state->move;
    if (!driver.move_inside(*b) || !pump(app)) return 1;
    if (!expect(b_state->move > b_move_before,
                "focus-loss", "X11 grab remained owned by A after retained cancellation")) return 1;
    if (!driver.release() || !pump(app)) return 1;
    if (!expect(a_state->up == up_before,
                "focus-loss", "cancelled capture received a duplicate release")) return 1;

    auto c_state = std::make_shared<CaptureState>();
    auto c_ui = std::make_unique<ui::UI>(probe_spec(c_state));
    auto c = std::make_unique<ui::StandaloneWindow>(
        app, *c_ui, ui::WindowDesc{.title = "T044 X11 C", .size = {240.0f, 170.0f}, .resizable = true});
    if (!c->valid()) return fail("window-c", c->last_error());
    if (!driver.press_inside(*c) || !pump(app)) return 1;
    if (!expect(c_state->down == 1, "destroy", "view C did not receive pointer down")) return 1;
    c.reset();
    c_ui.reset();
    if (!pump(app)) return 1;
    const int after_destroy = b_state->move;
    if (!driver.move_inside(*b) || !pump(app)) return 1;
    if (!expect(b_state->move > after_destroy,
                "destroy", "destroyed X11 view retained pointer grab")) return 1;
    if (!driver.release() || !pump(app)) return 1;

    for (int i = 0; i < 32; ++i) {
        if (!outside_release_cycle(app, driver, *a, a_state, "stress")) return 1;
    }

    std::cout << "T044 X11 native pointer capture smoke passed\n";
    return 0;
}
