#include "test_support.hpp"

namespace {
struct Trace {
    std::string events;
    ui::EditSource source{};
    template<class T> ui::EditCallbacks<T> callbacks() {
        return {
            [this](ui::EditSource s) { source=s; events+='B'; },
            [this](const T&, ui::EditSource s) { NUI_CHECK(s==source); events+='C'; },
            [this](ui::EditSource s) { NUI_CHECK(s==source); events+='E'; },
            [this](ui::EditSource s) { NUI_CHECK(s==source); events+='X'; }};
    }
};
ui::InputEvent wheel() {
    auto e=test::pointer(ui::InputType::PointerWheel,100,30);
    e.delta.y=1; return e;
}
void continuous_controls() {
    for (bool slider : {false,true}) {
        ui::State<float> value{0.5f};
        ui::State<bool> enabled{true};
        ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
        Trace trace;
        auto control = slider
            ? ui::make_spec(ui::Slider{value}.on_edit(trace.callbacks<float>()).wheel_enabled())
            : ui::make_spec(ui::Knob{"Value",value}.on_edit(trace.callbacks<float>()).wheel_enabled());
        ui::UI tree{ui::Enabled{enabled,ui::Visibility{visibility,std::move(control)}}};
        test::MockPlatform platform;
        tree.resize({200,60}); tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown,100,30),platform);
        NUI_CHECK(trace.events=="B");
        tree.dispatch(test::pointer(ui::InputType::PointerMove,120,10),platform);
        NUI_CHECK(trace.events=="BC");
        tree.dispatch(test::pointer(ui::InputType::PointerUp,120,10),platform);
        NUI_CHECK(trace.events=="BCE");
        NUI_CHECK(trace.source==ui::EditSource::Pointer);
        trace.events.clear(); value.set(0.5f); NUI_CHECK(trace.events.empty());
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(trace.events=="BCE"); NUI_CHECK(trace.source==ui::EditSource::Keyboard);
        trace.events.clear(); tree.dispatch(wheel(),platform);
        NUI_CHECK(trace.events=="BCE"); NUI_CHECK(trace.source==ui::EditSource::Wheel);
        trace.events.clear(); value.set(1); tree.dispatch(wheel(),platform);
        NUI_CHECK(trace.events.empty());
        for(int cancellation=0;cancellation<5;++cancellation) {
            value.set(0.5f); trace.events.clear();
            tree.dispatch(test::pointer(ui::InputType::PointerDown,100,30),platform);
            NUI_CHECK(trace.events=="B");
            if(cancellation==0) tree.dispatch(test::key(ui::Key::Escape),platform);
            if(cancellation==1) tree.cancel_pointer(platform);
            if(cancellation==2) enabled.set(false);
            if(cancellation==3) visibility.set(ui::VisibilityMode::Hidden);
            if(cancellation==4) tree.deactivate(platform);
            NUI_CHECK(trace.events=="BX");
            tree.cancel_pointer(platform); NUI_CHECK(trace.events=="BX");
            enabled.set(true); visibility.set(ui::VisibilityMode::Visible); tree.activate(platform);
        }
        trace.events.clear();
        tree.dispatch(test::pointer(ui::InputType::PointerDown,100,30),platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp,100,30),platform);
        NUI_CHECK(trace.events=="BE");
    }
}
void toggle_and_opt_in() {
    ui::State<bool> value{false}; Trace trace;
    ui::UI tree{ui::Toggle{"Switch",value}.on_edit(trace.callbacks<bool>())};
    test::MockPlatform platform; tree.resize({200,60}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Space),platform);
    tree.dispatch(test::key(ui::Key::Space),platform);
    NUI_CHECK(trace.events=="BCE" && value.get());
    NUI_CHECK(trace.source==ui::EditSource::Keyboard);
    auto up=test::key(ui::Key::Space); up.type=ui::InputType::KeyUp;
    tree.dispatch(up,platform); tree.dispatch(test::key(ui::Key::Space),platform);
    NUI_CHECK(trace.events=="BCEBCE" && !value.get());
    trace.events.clear();
    tree.dispatch(test::pointer(ui::InputType::PointerDown,100,30),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,100,30),platform);
    NUI_CHECK(trace.events=="BCE" && value.get());
    NUI_CHECK(trace.source==ui::EditSource::Pointer);
    trace.events.clear(); value.set(false); NUI_CHECK(trace.events.empty());
    for(bool slider:{false,true}) {
        ui::State<float> v{0.5f};
        auto control=slider ? ui::make_spec(ui::Slider{v}.on_edit(trace.callbacks<float>()))
                            : ui::make_spec(ui::Knob{"v",v}.on_edit(trace.callbacks<float>()));
        ui::UI inert{std::move(control)}; inert.resize({200,60}); inert.activate(platform);
        inert.dispatch(wheel(),platform); NUI_CHECK(trace.events.empty()); NUI_CHECK(v.get()==0.5f);
    }
}
void unmount_and_reentrancy() {
    ui::State<float> value{0.5f}; Trace trace; test::MockPlatform platform;
    {
        ui::UI tree{ui::Knob{"v",value}.on_edit(trace.callbacks<float>())};
        tree.resize({200,60}); tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown,100,30),platform);
    }
    NUI_CHECK(trace.events=="BX");
    for(bool slider:{false,true}) {
        trace.events.clear(); value.set(0.5f); ui::State<bool> enabled{true};
        auto observation=value.observe([&](const float&){enabled.set(false);});
        auto control=slider ? ui::make_spec(ui::Slider{value}.on_edit(trace.callbacks<float>()))
                            : ui::make_spec(ui::Knob{"v",value}.on_edit(trace.callbacks<float>()));
        ui::UI tree{ui::Enabled{enabled,std::move(control)}};
        tree.resize({200,60}); tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Right),platform);
        // Retained availability reconciliation happens after this atomic command.
        NUI_CHECK(trace.events=="BCE");
        tree.dispatch(test::key(ui::Key::Right),platform);
        NUI_CHECK(trace.events=="BCE");
    }
}
struct CallbackFailure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void callback_failure_releases_gesture_and_recovers() {
    for (const bool slider : {false, true}) {
        for (int failure = 0; failure < 3; ++failure) {
            ui::State<float> value{0.5f};
            std::string trace;
            bool throwing = true;
            ui::EditCallbacks<float> callbacks{
                [&](ui::EditSource) {
                    trace += 'B';
                    if (throwing && failure == 0) throw CallbackFailure{"begin"};
                },
                [&](const float&, ui::EditSource) {
                    trace += 'C';
                    if (throwing && failure == 1) throw CallbackFailure{"change"};
                },
                [&](ui::EditSource) {
                    trace += 'E';
                    if (throwing && failure == 2) throw CallbackFailure{"end"};
                },
                [&](ui::EditSource) { trace += 'X'; }};
            auto control = slider
                ? ui::make_spec(ui::Slider{value}.on_edit(callbacks))
                : ui::make_spec(ui::Knob{"v", value}.on_edit(callbacks));
            ui::UI tree{std::move(control)};
            test::MockPlatform platform;
            tree.resize({200, 60});
            tree.activate(platform);
            bool caught = false;
            try {
                tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
                tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10), platform);
                tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10), platform);
            } catch (const CallbackFailure&) {
                caught = true;
            }
            NUI_CHECK(caught);
            NUI_CHECK(trace == (failure == 0 ? "BX" : failure == 1 ? "BCX" : "BCE"));
            NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
            NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
            const auto after_failure = trace;
            const auto committed = value.get();
            tree.dispatch(test::pointer(ui::InputType::PointerMove, 140, 0), platform);
            tree.dispatch(test::pointer(ui::InputType::PointerUp, 140, 0), platform);
            NUI_CHECK(value.get() == committed);
            NUI_CHECK(trace == after_failure);

            throwing = false;
            trace.clear();
            value.set(0.5f);
            tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
            tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10), platform);
            tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10), platform);
            NUI_CHECK(trace == "BCE");
            NUI_CHECK(value.get() > 0.5f);
            NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
        }
    }

    for (int failure = 0; failure < 3; ++failure) {
        ui::State<bool> value{false};
        std::string trace;
        bool throwing = true;
        ui::UI tree{ui::Toggle{"v", value}.on_edit({
            [&](ui::EditSource) {
                trace += 'B';
                if (throwing && failure == 0) throw CallbackFailure{"begin"};
            },
            [&](const bool&, ui::EditSource) {
                trace += 'C';
                if (throwing && failure == 1) throw CallbackFailure{"change"};
            },
            [&](ui::EditSource) {
                trace += 'E';
                if (throwing && failure == 2) throw CallbackFailure{"end"};
            },
            [&](ui::EditSource) { trace += 'X'; }})};
        test::MockPlatform platform;
        tree.resize({200, 60});
        tree.activate(platform);
        bool caught = false;
        try {
            tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
        } catch (const CallbackFailure&) {
            caught = true;
        }
        NUI_CHECK(caught);
        NUI_CHECK(trace == (failure == 0 ? "BX" : failure == 1 ? "BCX" : "BCE"));
        NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        throwing = false;
        trace.clear();
        const auto before = value.get();
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 30), platform);
        NUI_CHECK(trace == "BCE" && value.get() != before);
        NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);

        // A failed keyboard activation must also clear repeat suppression.
        throwing = true;
        trace.clear();
        caught = false;
        try {
            tree.dispatch(test::key(ui::Key::Space), platform);
        } catch (const CallbackFailure&) {
            caught = true;
        }
        NUI_CHECK(caught);
        NUI_CHECK(trace == (failure == 0 ? "BX" : failure == 1 ? "BCX" : "BCE"));
        throwing = false;
        trace.clear();
        const auto before_retry = value.get();
        tree.dispatch(test::key(ui::Key::Space), platform);
        NUI_CHECK(trace == "BCE" && value.get() != before_retry);
    }
}

void run() {
    continuous_controls();
    toggle_and_opt_in();
    unmount_and_reentrancy();
    callback_failure_releases_gesture_and_recovers();
}
}
int main() { return test::run("widget edits", run); }
