#include "example_support.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

ui::UI make_ui(ui::State<std::string>& status) {
    return ui::UI{
        ui::Column{
            ui::Header{"T065 - UI dispatcher"},
            ui::TextInput{"Status", status},
        }.padding(18.0f).gap(12.0f)};
}

int run_self_test() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) {
        return example::fail(application.last_error().empty()
                                 ? "Application initialization failed"
                                 : application.last_error());
    }

    ui::State<std::string> status{"idle"};
    auto tree = make_ui(status);
    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T065 self-test",
                       .size = {360.0f, 150.0f},
                       .resizable = true}};
    if (!window.valid()) {
        return example::fail(window.last_error().empty()
                                 ? "window creation failed"
                                 : window.last_error());
    }

    const auto dispatcher = window.dispatcher();
    if (!dispatcher.valid()) return example::fail("window dispatcher is invalid");

    const auto ui_thread = std::this_thread::get_id();
    std::atomic<bool> accepted{false};
    bool wrong_thread = false;
    std::vector<int> order;

    std::thread worker{[dispatcher, &accepted, &wrong_thread, &order, &status, ui_thread] {
        accepted.store(
            dispatcher.post([&wrong_thread, &order, &status, ui_thread] {
                wrong_thread = std::this_thread::get_id() != ui_thread;
                order.push_back(1);
                status.set("worker");
            }),
            std::memory_order_release);
    }};
    worker.join();

    if (!accepted.load(std::memory_order_acquire)) {
        return example::fail("worker post was rejected");
    }

    const auto timer = dispatcher.schedule_after(0ms, [&] {
        order.push_back(2);
        status.set("timer");
    });
    if (!timer.valid()) return example::fail("zero-delay timer was rejected");

    int cancelled_calls = 0;
    const auto cancelled = dispatcher.schedule_after(0ms, [&] { ++cancelled_calls; });
    if (!cancelled.valid() || !dispatcher.cancel(cancelled)) {
        return example::fail("timer cancellation failed");
    }

    if (!application.poll(0.0)) {
        return example::fail(application.last_error().empty()
                                 ? "application poll failed"
                                 : application.last_error());
    }

    if (wrong_thread) return example::fail("worker callback ran off the UI thread");
    if (order != std::vector<int>{1, 2}) {
        return example::fail("task/timer FIFO order was not deterministic");
    }
    if (status.get() != "timer") return example::fail("timer state update was not applied");
    if (cancelled_calls != 0) return example::fail("cancelled timer executed");

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();

    try {
        ui::Application application;
        application.set_quit_policy(ui::QuitPolicy::OnLastWindowClosed);
        if (!application.valid()) {
            std::cerr << "Application error: " << application.last_error() << '\n';
            return 1;
        }

        ui::State<std::string> status{"waiting for dispatcher"};
        auto tree = make_ui(status);
        ui::StandaloneWindow window{
            application,
            tree,
            ui::WindowDesc{.title = "NativeUI T065 - Dispatcher",
                           .size = {420.0f, 180.0f},
                           .resizable = true}};
        if (!window.valid()) {
            std::cerr << "Window error: " << window.last_error() << '\n';
            return 1;
        }

        const auto dispatcher = window.dispatcher();
        std::thread worker{[dispatcher, &status] {
            (void)dispatcher.post([&status] { status.set("worker -> UI callback"); });
        }};
        worker.join();

        std::uint64_t ticks = 0;
        const auto timer = dispatcher.schedule_every(1s, [&status, &ticks] {
            ++ticks;
            status.set("timer tick " + std::to_string(ticks));
        });
        if (!timer.valid()) {
            std::cerr << "Failed to schedule dispatcher timer\n";
            return 1;
        }

        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "T065 example failed: " << error.what() << '\n';
        return 1;
    }
}
