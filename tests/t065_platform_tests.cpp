#include <nativeui/nativeui.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

using namespace std::chrono_literals;

constexpr std::string_view kNativeBoundaryFailure{"T128 native boundary failure"};

struct PaintFaultState final {
    bool throwing{true};
    int paints{};
};

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[T065 platform] " << stage << ": " << message << '\n';
    return 1;
}

struct Fixture {
    ui::State<std::string> text{"T065"};
    ui::UI tree{ui::TextInput{"T065 text", text}};
};

class ThrowingPaintComponent final : public ui::Component {
public:
    explicit ThrowingPaintComponent(std::shared_ptr<PaintFaultState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {180.0f, 72.0f};
    }

    void paint(ui::PaintContext&) const override {
        ++state_->paints;
        if (state_->throwing) throw std::runtime_error{kNativeBoundaryFailure.data()};
    }

private:
    std::shared_ptr<PaintFaultState> state_;
};

struct ThrowingPaintRoot {
    std::shared_ptr<PaintFaultState> state;

    [[nodiscard]] ui::Spec spec() const {
        ui::Spec root;
        root.factory = [state = state] {
            return std::make_unique<ThrowingPaintComponent>(state);
        };
        return root;
    }
};

bool valid_window(const ui::StandaloneWindow& window, std::string_view stage) {
    if (!window.valid()) {
        (void)fail(stage, window.last_error().empty() ? "window is invalid" : window.last_error());
        return false;
    }
    if (!window.native_handle()) {
        (void)fail(stage, "native handle is zero");
        return false;
    }
    return true;
}

int standalone_worker_wake() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) return fail("standalone", application.last_error());

    Fixture fixture;
    ui::StandaloneWindow window{
        application,
        fixture.tree,
        ui::WindowDesc{.title = "NativeUI T065 worker wake",
                       .size = {300.0f, 130.0f},
                       .resizable = true}};
    if (!valid_window(window, "standalone")) return 1;

    auto dispatcher = window.dispatcher();
    if (!dispatcher.valid()) return fail("standalone", "window dispatcher is invalid");

    // Drain realization/show traffic so the following bounded wait primarily
    // exercises the dispatcher wake path rather than an already queued expose.
    for (int i = 0; i < 12; ++i) {
        if (!application.poll(0.0)) return fail("standalone-drain", application.last_error());
    }

    const auto ui_thread = std::this_thread::get_id();
    std::atomic<bool> accepted{false};
    std::atomic<int> calls{0};
    std::atomic<bool> wrong_thread{false};
    const auto start = std::chrono::steady_clock::now();

    std::thread worker{[dispatcher, &accepted, &calls, &wrong_thread, ui_thread] {
        std::this_thread::sleep_for(50ms);
        accepted.store(
            dispatcher.post([&calls, &wrong_thread, ui_thread] {
                wrong_thread.store(std::this_thread::get_id() != ui_thread,
                                   std::memory_order_release);
                calls.fetch_add(1, std::memory_order_release);
            }),
            std::memory_order_release);
    }};

    // A successful empty->non-empty post must interrupt this long blocking
    // application wait. Without a native wake this returns only at the timeout.
    bool poll_ok = true;
    while (calls.load(std::memory_order_acquire) == 0 &&
           std::chrono::steady_clock::now() - start < 1500ms) {
        if (!application.poll(1.0)) {
            poll_ok = false;
            break;
        }
    }

    // Always join before returning so a platform-poll failure reports its real
    // diagnostic instead of being masked by std::thread's destructor terminate.
    worker.join();
    if (!poll_ok) return fail("standalone-poll", application.last_error());

    if (!accepted.load(std::memory_order_acquire)) return fail("standalone", "worker post was rejected");
    if (calls.load(std::memory_order_acquire) != 1) return fail("standalone", "callback did not execute exactly once");
    if (wrong_thread.load(std::memory_order_acquire)) return fail("standalone", "callback did not execute on UI thread");
    if (std::chrono::steady_clock::now() - start >= 900ms) {
        return fail("standalone", "worker post did not wake blocked application promptly");
    }
    return 0;
}

int embedded_host_checkpoint() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) return fail("embedded", application.last_error());

    Fixture parent_fixture;
    ui::StandaloneWindow parent{
        application,
        parent_fixture.tree,
        ui::WindowDesc{.title = "NativeUI T065 embedded parent",
                       .size = {320.0f, 160.0f},
                       .resizable = true}};
    if (!valid_window(parent, "embedded-parent")) return 1;

    Fixture child_fixture;
    ui::EmbeddedView child{child_fixture.tree, parent.native_handle(), {240.0f, 96.0f}};
    auto dispatcher = child.dispatcher();
    if (!dispatcher.valid()) return fail("embedded", "embedded dispatcher is invalid");

    const auto ui_thread = std::this_thread::get_id();
    std::atomic<int> calls{0};
    std::atomic<bool> wrong_thread{false};
    std::atomic<bool> accepted{false};
    std::thread worker{[dispatcher, &calls, &wrong_thread, &accepted, ui_thread] {
        accepted.store(
            dispatcher.post([&calls, &wrong_thread, ui_thread] {
                wrong_thread.store(std::this_thread::get_id() != ui_thread,
                                   std::memory_order_release);
                calls.fetch_add(1, std::memory_order_release);
            }),
            std::memory_order_release);
    }};
    worker.join();

    if (!accepted.load(std::memory_order_acquire)) return fail("embedded", "worker post was rejected");

    // Pumping the standalone parent is intentionally insufficient: an embedded
    // owner remains host-driven and must not gain a background dispatcher loop.
    for (int i = 0; i < 8; ++i) {
        if (!application.poll(0.0)) return fail("embedded-parent-poll", application.last_error());
    }
    std::this_thread::sleep_for(20ms);
    if (calls.load(std::memory_order_acquire) != 0) {
        return fail("embedded", "callback ran before the embedded host checkpoint");
    }

    (void)child.poll();
    if (calls.load(std::memory_order_acquire) != 1) {
        return fail("embedded", "callback did not run at the next embedded poll checkpoint");
    }
    if (wrong_thread.load(std::memory_order_acquire)) return fail("embedded", "callback ran off the host UI thread");
    return 0;
}

int owner_isolation() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) return fail("isolation", application.last_error());

    Fixture a_fixture;
    Fixture b_fixture;
    auto a = std::make_unique<ui::StandaloneWindow>(
        application,
        a_fixture.tree,
        ui::WindowDesc{.title = "NativeUI T065 owner A", .size = {260.0f, 100.0f}, .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        application,
        b_fixture.tree,
        ui::WindowDesc{.title = "NativeUI T065 owner B", .size = {260.0f, 100.0f}, .resizable = true});
    if (!valid_window(*a, "isolation-a") || !valid_window(*b, "isolation-b")) return 1;

    auto dispatcher_a = a->dispatcher();
    auto dispatcher_b = b->dispatcher();
    int a_calls = 0;
    int b_calls = 0;
    if (!dispatcher_a.post([&] { ++a_calls; })) return fail("isolation", "A post rejected");
    if (!dispatcher_b.post([&] { ++b_calls; })) return fail("isolation", "B post rejected");

    a.reset();
    if (dispatcher_a.valid()) return fail("isolation", "destroyed owner dispatcher remained valid");
    if (dispatcher_a.post([&] { ++a_calls; })) return fail("isolation", "destroyed owner accepted work");

    for (int i = 0; i < 4 && b_calls == 0; ++i) {
        if (!application.poll(0.0)) return fail("isolation-poll", application.last_error());
    }
    if (a_calls != 0) return fail("isolation", "destroyed owner callback executed");
    if (b_calls != 1) return fail("isolation", "surviving owner callback did not execute exactly once");
    if (!dispatcher_b.valid()) return fail("isolation", "surviving owner dispatcher became invalid");
    return 0;
}

int native_exception_boundary() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) return fail("native-exception", application.last_error());

    auto paint_state = std::make_shared<PaintFaultState>();
    ThrowingPaintRoot root{paint_state};
    ui::UI tree{root};
    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T128 native exception boundary",
                       .size = {260.0f, 120.0f},
                       .resizable = true}};
    if (!valid_window(window, "native-exception")) return 1;

    // The platform expose is delivered by Pugl through a noexcept foreign ABI
    // thunk. A throwing user paint callback must be contained there: the event
    // loop records the original diagnostic and leaves the view usable for a
    // later external invalidation; no C++ exception may cross Pugl's ABI.
    for (int i = 0; i < 12 && paint_state->paints == 0; ++i) {
        try {
            if (!application.poll(0.1)) {
                return fail("native-exception", "application stopped after paint fault");
            }
        } catch (const std::exception& e) {
            return fail("native-exception", e.what());
        } catch (...) {
            return fail("native-exception", "non-standard exception escaped the native callback boundary");
        }
    }

    if (paint_state->paints == 0) {
        return fail("native-exception", "throwing paint callback did not reach the native event boundary");
    }
    if (!window.valid() || window.should_close()) {
        return fail("native-exception", "retryable paint fault closed the native view");
    }
    if (window.last_error() != kNativeBoundaryFailure) {
        return fail("native-exception", window.last_error().empty()
            ? "native boundary lost the callback diagnostic"
            : window.last_error());
    }

    paint_state->throwing = false;
    if (!window.set_size({261.0f, 120.0f})) {
        return fail("native-exception", "could not request a later native expose");
    }
    for (int i = 0; i < 12 && paint_state->paints < 2; ++i) {
        if (!application.poll(0.1)) {
            return fail("native-exception", "application stopped before recovery");
        }
    }
    if (paint_state->paints < 2 || !window.valid() ||
        !window.last_error().empty()) {
        return fail("native-exception",
                    "later expose did not recover paint and diagnostic state: paints=" +
                        std::to_string(paint_state->paints) +
                        " valid=" + std::to_string(window.valid()) +
                        " error=" + std::string{window.last_error()});
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("arguments", "expected exactly one mode");
    const std::string_view mode{argv[1]};
    if (mode == "--standalone-worker-wake") return standalone_worker_wake();
    if (mode == "--embedded-host-checkpoint") return embedded_host_checkpoint();
    if (mode == "--owner-isolation") return owner_isolation();
    if (mode == "--native-exception-boundary") return native_exception_boundary();
    return fail("arguments", "unknown mode");
}
