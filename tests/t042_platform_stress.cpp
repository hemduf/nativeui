#include <nativeui/nativeui.hpp>

#include <charconv>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct ProbeStats {
    int mount{};
    int activate{};
    int deactivate{};
    int unmount{};
    int focus_in{};
    int focus_out{};
    int text_input_on{};
    int text_input_off{};
    int pointer_down{};
    int pointer_move{};
    int pointer_cancel{};
    int state_changes{};
    int clipboard_requests{};
    int stale_callbacks{};
    std::weak_ptr<int> lifetime;
};

class StressFailure final : public std::runtime_error {
public:
    StressFailure(
        std::string_view fixture,
        int cycle,
        std::string_view transition,
        std::string_view message)
        : std::runtime_error(
              std::string("fixture=") + std::string(fixture) +
              " cycle=" + std::to_string(cycle) +
              " transition=" + std::string(transition) + ": " + std::string(message)) {}
};

void require(
    bool condition,
    std::string_view fixture,
    int cycle,
    std::string_view transition,
    std::string_view message) {
    if (!condition) throw StressFailure(fixture, cycle, transition, message);
}

class StressProbeComponent final : public ui::Component {
public:
    StressProbeComponent(std::shared_ptr<ProbeStats> stats, ui::State<int>& observed)
        : stats_(std::move(stats)),
          observed_(observed),
          lifetime_(std::make_shared<int>(1)) {
        stats_->lifetime = lifetime_;
    }

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {140.0f, 64.0f};
    }

    void mount(ui::MountContext& context) override {
        ++stats_->mount;
        const auto weak_lifetime = std::weak_ptr<int>{lifetime_};
        auto invalidate = context.invalidator();
        subscription_ = observed_.observe(
            [stats = stats_, weak_lifetime, invalidate = std::move(invalidate)](const int&) {
                if (weak_lifetime.expired()) {
                    ++stats->stale_callbacks;
                    return;
                }
                ++stats->state_changes;
                invalidate();
            });
    }

    void activate(ui::LifecycleContext&) override { ++stats_->activate; }
    void deactivate(ui::LifecycleContext&) override { ++stats_->deactivate; }
    void unmount(ui::LifecycleContext&) override { ++stats_->unmount; }

    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) {
            ++stats_->focus_in;
            ++stats_->text_input_on;
            context.set_text_input(true, context.bounds());
        } else {
            ++stats_->focus_out;
            ++stats_->text_input_off;
            context.set_text_input(false);
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            ++stats_->pointer_down;
            context.capture_pointer();
            context.request_clipboard_text();
            ++stats_->clipboard_requests;
            context.invalidate();
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            ++stats_->pointer_move;
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            ++stats_->pointer_cancel;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ProbeStats> stats_;
    ui::State<int>& observed_;
    std::shared_ptr<int> lifetime_;
    ui::State<int>::Subscription subscription_;
};

class StressProbe {
public:
    StressProbe(std::shared_ptr<ProbeStats> stats, ui::State<int>& observed)
        : stats_(std::move(stats)), observed_(&observed) {}

    ui::Spec spec() && {
        auto stats = std::move(stats_);
        auto* observed = observed_;
        return ui::Spec{
            [stats = std::move(stats), observed] {
                return std::make_unique<StressProbeComponent>(stats, *observed);
            },
            {}};
    }

private:
    std::shared_ptr<ProbeStats> stats_;
    ui::State<int>* observed_{};
};

ui::InputEvent pointer(ui::InputType type, float x, float y) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    return event;
}

void validate_window(
    const ui::StandaloneWindow& window,
    std::string_view fixture,
    int cycle,
    std::string_view transition) {
    require(window.native_handle() != 0, fixture, cycle, transition, "native handle is zero");
    require(window.scale_factor() > 0.0f, fixture, cycle, transition, "invalid scale factor");
    require(window.last_error().empty(), fixture, cycle, transition, window.last_error());
}

void validate_embedded(
    const ui::EmbeddedView& view,
    std::string_view fixture,
    int cycle,
    std::string_view transition) {
    require(view.native_handle() != 0, fixture, cycle, transition, "native handle is zero");
    require(view.scale_factor() > 0.0f, fixture, cycle, transition, "invalid scale factor");
    require(view.last_error().empty(), fixture, cycle, transition, view.last_error());
}

void pump_parent(
    ui::StandaloneWindow& parent,
    std::string_view fixture,
    int cycle,
    std::string_view transition,
    int iterations = 2) {
    for (int i = 0; i < iterations && !parent.should_close(); ++i) {
        (void)parent.poll(0.0);
    }
    require(parent.last_error().empty(), fixture, cycle, transition, parent.last_error());
}

void pump_child(
    ui::StandaloneWindow& parent,
    ui::EmbeddedView& child,
    std::string_view fixture,
    int cycle,
    std::string_view transition,
    int iterations = 2) {
    for (int i = 0; i < iterations && !child.should_close(); ++i) {
        (void)parent.poll(0.0);
        (void)child.poll();
    }
    require(parent.last_error().empty(), fixture, cycle, transition, parent.last_error());
    require(child.last_error().empty(), fixture, cycle, transition, child.last_error());
}

struct ParentHarness {
    ui::UI ui{ui::Column{ui::Header{"T042 stress host"}, ui::Spacer{220.0f}}.padding(8.0f).gap(4.0f)};
    ui::StandaloneWindow window{
        ui,
        ui::WindowDesc{
            .title = "NativeUI T042 stress host",
            .size = {420.0f, 300.0f},
            .resizable = true}};

    ParentHarness(std::string_view fixture) {
        validate_window(window, fixture, -1, "construct-parent");
        pump_parent(window, fixture, -1, "realize-parent", 3);
    }
};

int run_embedded_sequential_100() {
    constexpr std::string_view fixture = "embedded_sequential_100";
    ParentHarness parent{fixture};

    for (int cycle = 0; cycle < 100; ++cycle) {
        ui::State<int> observed{7};
        auto stats = std::make_shared<ProbeStats>();
        {
            ui::UI child_ui{StressProbe{stats, observed}};
            {
                ui::EmbeddedView child{child_ui, parent.window.native_handle(), {220.0f, 100.0f}};
                validate_embedded(child, fixture, cycle, "construct");
                child_ui.activate(child);
                require(stats->focus_in == 1 && stats->text_input_on == 1,
                        fixture, cycle, "activate", "focus/text input did not activate exactly once");

                child.set_clipboard_text("NativeUI T042 shared clipboard value");
                const auto result = child_ui.dispatch(
                    pointer(ui::InputType::PointerDown, 10.0f, 10.0f), child);
                require(result == ui::EventResult::Handled,
                        fixture, cycle, "pointer-down", "pointer capture probe was not handled");
                require(stats->clipboard_requests == 1,
                        fixture, cycle, "clipboard-request", "clipboard request count != 1");

                observed.set(8);
                require(stats->state_changes == 1,
                        fixture, cycle, "state-change", "state observer count != 1");
                pump_child(parent.window, child, fixture, cycle, "poll", 2);
            }

            require(stats->pointer_cancel == 1,
                    fixture, cycle, "destroy-view", "capture was not cancelled exactly once");
            require(stats->focus_out == 1 && stats->text_input_off == 1,
                    fixture, cycle, "destroy-view", "focus/text input teardown count != 1");
            require(stats->deactivate == 1,
                    fixture, cycle, "destroy-view", "component deactivation count != 1");

            // The retained UI intentionally outlives the native child for this
            // transition. Its state subscription may still run, but ViewCore's
            // invalidation callback has already been cleared and must be inert.
            observed.set(9);
            require(stats->state_changes == 2,
                    fixture, cycle, "post-view-state", "retained UI state callback did not remain local");
        }

        require(stats->unmount == 1, fixture, cycle, "destroy-ui", "unmount count != 1");
        require(stats->lifetime.expired(), fixture, cycle, "destroy-ui", "lifetime sentinel survived UI destruction");
        const int state_changes = stats->state_changes;
        observed.set(10);
        require(stats->state_changes == state_changes && stats->stale_callbacks == 0,
                fixture, cycle, "post-ui-state", "callback executed after UI owner destruction");
    }

    parent.window.request_close();
    return 0;
}

int run_embedded_two_live_50() {
    constexpr std::string_view fixture = "embedded_two_live_50";
    ParentHarness parent{fixture};

    for (int cycle = 0; cycle < 50; ++cycle) {
        ui::State<int> state_a{11};
        ui::State<int> state_b{11};
        auto stats_a = std::make_shared<ProbeStats>();
        auto stats_b = std::make_shared<ProbeStats>();

        {
            ui::UI ui_a{StressProbe{stats_a, state_a}};
            ui::UI ui_b{StressProbe{stats_b, state_b}};
            auto a = std::make_unique<ui::EmbeddedView>(
                ui_a, parent.window.native_handle(), ui::Size{220.0f, 100.0f});
            auto b = std::make_unique<ui::EmbeddedView>(
                ui_b, parent.window.native_handle(), ui::Size{220.0f, 100.0f});
            validate_embedded(*a, fixture, cycle, "construct-a");
            validate_embedded(*b, fixture, cycle, "construct-b");

            ui_a.activate(*a);
            ui_b.activate(*b);
            a->set_clipboard_text("NativeUI T042 same-looking clipboard");
            b->set_clipboard_text("NativeUI T042 same-looking clipboard");
            ui_a.dispatch(pointer(ui::InputType::PointerDown, 12.0f, 12.0f), *a);
            ui_b.dispatch(pointer(ui::InputType::PointerDown, 12.0f, 12.0f), *b);
            require(stats_a->clipboard_requests == 1 && stats_b->clipboard_requests == 1,
                    fixture, cycle, "clipboard-request", "A/B clipboard request bookkeeping diverged");

            state_a.set(12);
            state_b.set(12);
            require(stats_a->state_changes == 1 && stats_b->state_changes == 1,
                    fixture, cycle, "state-change", "A/B state observers did not fire independently");

            for (int i = 0; i < 2; ++i) {
                (void)parent.window.poll(0.0);
                (void)a->poll();
                (void)b->poll();
            }
            require(a->last_error().empty(), fixture, cycle, "poll-a", a->last_error());
            require(b->last_error().empty(), fixture, cycle, "poll-b", b->last_error());

            a.reset();
            require(stats_a->pointer_cancel == 1 && stats_a->focus_out == 1,
                    fixture, cycle, "destroy-a", "A capture/focus teardown count != 1");
            require(stats_b->pointer_cancel == 0 && stats_b->focus_out == 0,
                    fixture, cycle, "destroy-a", "destroying A cancelled or unfocused B");

            const int b_changes_before = stats_b->state_changes;
            state_a.set(13);
            state_b.set(13);
            require(stats_a->state_changes == 2,
                    fixture, cycle, "continue-b", "A retained state callback became stale after view destruction");
            require(stats_b->state_changes == b_changes_before + 1,
                    fixture, cycle, "continue-b", "B invalidation/state path stopped after A destruction");

            const auto move_result = ui_b.dispatch(
                pointer(ui::InputType::PointerMove, 500.0f, 500.0f), *b);
            require(move_result == ui::EventResult::Handled && stats_b->pointer_move == 1,
                    fixture, cycle, "continue-b", "B pointer capture did not survive A destruction");
            b->set_clipboard_text("NativeUI T042 surviving B clipboard");
            pump_child(parent.window, *b, fixture, cycle, "continue-b-poll", 2);
            require(stats_b->pointer_cancel == 0 && stats_b->focus_out == 0,
                    fixture, cycle, "continue-b-poll", "B interaction state changed unexpectedly");

            b.reset();
            require(stats_b->pointer_cancel == 1 && stats_b->focus_out == 1,
                    fixture, cycle, "destroy-b", "B capture/focus teardown count != 1");
        }

        require(stats_a->lifetime.expired() && stats_b->lifetime.expired(),
                fixture, cycle, "destroy-ui", "A/B component lifetime sentinel survived");
        const int a_changes = stats_a->state_changes;
        const int b_changes = stats_b->state_changes;
        state_a.set(14);
        state_b.set(14);
        require(stats_a->state_changes == a_changes && stats_b->state_changes == b_changes,
                fixture, cycle, "post-ui-state", "state callback survived A/B UI destruction");
        require(stats_a->stale_callbacks == 0 && stats_b->stale_callbacks == 0,
                fixture, cycle, "post-ui-state", "weak stale-callback sentinel fired");
    }

    parent.window.request_close();
    return 0;
}

int run_embedded_capture_focus_teardown() {
    constexpr std::string_view fixture = "embedded_capture_focus_teardown";
    ParentHarness parent{fixture};

    for (int cycle = 0; cycle < 50; ++cycle) {
        ui::State<int> observed{21};
        auto stats = std::make_shared<ProbeStats>();
        {
            ui::UI child_ui{StressProbe{stats, observed}};
            {
                auto child = std::make_unique<ui::EmbeddedView>(
                    child_ui, parent.window.native_handle(), ui::Size{220.0f, 100.0f});
                validate_embedded(*child, fixture, cycle, "construct");
                child_ui.activate(*child);
                require(stats->focus_in == 1 && stats->text_input_on == 1,
                        fixture, cycle, "activate", "focus/text input did not activate");

                const auto down = child_ui.dispatch(
                    pointer(ui::InputType::PointerDown, 8.0f, 8.0f), *child);
                require(down == ui::EventResult::Handled && stats->pointer_down == 1,
                        fixture, cycle, "capture", "pointer was not captured");
                observed.set(22);
                pump_child(parent.window, *child, fixture, cycle, "active-poll", 1);

                // Deliberately destroy while focus, text input, pointer capture,
                // state observation and a native invalidation callback are live.
                child.reset();
            }

            require(stats->pointer_cancel == 1,
                    fixture, cycle, "destroy-active", "PointerCancel count != 1");
            require(stats->focus_out == 1,
                    fixture, cycle, "destroy-active", "focus-out count != 1");
            require(stats->text_input_off == 1,
                    fixture, cycle, "destroy-active", "text input was not deactivated exactly once");
            require(stats->deactivate == 1,
                    fixture, cycle, "destroy-active", "component deactivate count != 1");

            observed.set(23);
            require(stats->state_changes == 2,
                    fixture, cycle, "post-view-state", "retained callback path stopped unexpectedly");
        }

        require(stats->lifetime.expired(), fixture, cycle, "destroy-ui", "lifetime sentinel survived");
        const int changes = stats->state_changes;
        observed.set(24);
        require(stats->state_changes == changes && stats->stale_callbacks == 0,
                fixture, cycle, "post-ui-state", "stale callback executed after UI destruction");
    }

    parent.window.request_close();
    return 0;
}

int run_standalone_once(int cycle) {
    constexpr std::string_view fixture = "standalone_sequential_50";

    // #64 Decision B only guarantees one PROGRAM owner lifetime per process
    // until T060 introduces ui::Application. This process-isolated fixture
    // therefore exercises exactly create/poll/resize/close/destroy; active
    // focus/capture/text-input teardown belongs to the embedded teardown
    // fixture where multi-instance ownership is currently supported.
    ui::UI app{
        ui::Column{
            ui::Header{"NativeUI T042 standalone isolated cycle"},
            ui::Spacer{120.0f},
        }.padding(8.0f).gap(4.0f)};
    auto window = std::make_unique<ui::StandaloneWindow>(
        app,
        ui::WindowDesc{
            .title = "NativeUI T042 standalone isolated cycle",
            .size = {300.0f, 140.0f},
            .resizable = true});
    validate_window(*window, fixture, cycle, "construct");
    pump_parent(*window, fixture, cycle, "poll", 3);
    require(window->set_size({310.0f, 150.0f}),
            fixture, cycle, "resize", "set_size failed");
    pump_parent(*window, fixture, cycle, "resize-poll", 2);
    window->request_close();
    require(window->should_close(), fixture, cycle, "close", "request_close was not observed");
    window.reset();
    return 0;
}

int run_standalone_contract() {
    // Issue #64 froze Decision B: simultaneous independent StandaloneWindow /
    // PUGL_PROGRAM worlds are not a supported current contract. T060 owns the
    // future one-Application/one-PROGRAM-world multi-window fixture. This mode
    // is deliberately executable evidence that T042 does not hide a singleton
    // or create unsupported A+B PROGRAM worlds merely to satisfy a stress test.
    std::cout
        << "PASS standalone_supported_multi_instance: Decision B; "
           "simultaneous top-level windows deferred to T060\n";
    return 0;
}

int parse_cycle(std::string_view value) {
    int cycle = -1;
    const char* first = value.data();
    const char* last = value.data() + value.size();
    const auto [ptr, error] = std::from_chars(first, last, cycle);
    if (error != std::errc{} || ptr != last || cycle < 0) {
        throw std::invalid_argument("invalid non-negative cycle argument");
    }
    return cycle;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2) {
            const std::string_view mode{argv[1]};
            if (mode == "--embedded-sequential") return run_embedded_sequential_100();
            if (mode == "--embedded-two-live") return run_embedded_two_live_50();
            if (mode == "--embedded-teardown") return run_embedded_capture_focus_teardown();
            if (mode == "--standalone-contract") return run_standalone_contract();
        }
        if (argc == 3 && std::string_view{argv[1]} == "--standalone-once") {
            return run_standalone_once(parse_cycle(argv[2]));
        }

        std::cerr
            << "[nativeui t042 stress] expected one of --embedded-sequential, "
               "--embedded-two-live, --embedded-teardown, --standalone-contract, "
               "or --standalone-once <cycle>\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "[nativeui t042 stress] " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[nativeui t042 stress] unknown exception\n";
        return 1;
    }
}
