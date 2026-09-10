#include <nativeui/nativeui.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[T065 platform] " << stage << ": " << message << '\n';
    return 1;
}

struct Fixture {
    ui::State<std::string> text{"T065"};
    ui::UI tree{ui::TextInput{"T065 text", text}};
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
    while (calls.load(std::memory_order_acquire) == 0 &&
           std::chrono::steady_clock::now() - start < 1500ms) {
        if (!application.poll(1.0)) return fail("standalone-poll", application.last_error());
    }
    worker.join();

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

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return fail("arguments", "expected exactly one mode");
    const std::string_view mode{argv[1]};
    if (mode == "--standalone-worker-wake") return standalone_worker_wake();
    if (mode == "--embedded-host-checkpoint") return embedded_host_checkpoint();
    if (mode == "--owner-isolation") return owner_isolation();
    return fail("arguments", "unknown mode");
}
