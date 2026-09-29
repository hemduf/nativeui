#include "example_support.hpp"

#include <iostream>
#include <memory>

namespace {

std::unique_ptr<ui::UI> make_parent_ui() {
    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T041 / STANDALONE + EMBEDDED SMOKE"},
            ui::Canvas{560.0f, 120.0f, [](ui::CanvasContext2D& g) {
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, ui::colors::panel);
                g.text({16.0f, 34.0f}, "Parent: PUGL_PROGRAM", 16.0f, ui::colors::text);
                g.text({16.0f, 66.0f}, "Child: PUGL_MODULE embedded in the native parent view", 12.0f, ui::colors::textMuted);
                g.text({16.0f, 94.0f}, "Close this window to finish the lifecycle example", 11.0f, ui::colors::textMuted);
            }}
        }.padding(18.0f).gap(12.0f));
}

std::unique_ptr<ui::UI> make_child_ui(ui::State<bool>& enabled) {
    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"Embedded child"},
            ui::Toggle{"Enabled", enabled},
        }.padding(10.0f).gap(8.0f));
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        ui::State<bool> state{false};
        auto parent = make_parent_ui();
        auto child = make_child_ui(state);
        if (!parent || !child) return example::fail("failed to construct smoke UIs");
        state.set(true);
        if (!state.get()) return example::fail("state mutation failed");
        return 0;
    }

#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail("window mode is disabled in self-test-only validation builds");
#else
    try {
        auto parent_ui = make_parent_ui();
        ui::Application application;
        ui::StandaloneWindow parent{
            application,
            *parent_ui,
            ui::WindowDesc{.title = "NativeUI T041 - Smoke Harness",
                           .size = {640.0f, 360.0f},
                           .resizable = true}};

        ui::State<bool> child_enabled{false};
        auto child_ui = make_child_ui(child_enabled);
        ui::EmbeddedView child{*child_ui, parent.native_handle(), {300.0f, 140.0f}};

        std::cout << "parent handle=" << parent.native_handle()
                  << " child handle=" << child.native_handle() << '\n';

        while (!parent.should_close()) {
            (void)application.poll(0.016);
            (void)child.poll();
            if (!parent.last_error().empty()) {
                std::cerr << "parent error: " << parent.last_error() << '\n';
                return 1;
            }
            if (!child.last_error().empty()) {
                std::cerr << "child error: " << child.last_error() << '\n';
                return 1;
            }
        }
        child.request_close();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "T041 smoke example failed: " << e.what() << '\n';
        return 1;
    }
#endif
}
