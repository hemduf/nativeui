#include <nativeui/nativeui.hpp>

#include <exception>
#include <iostream>
#include <string_view>

namespace {
int fail(int code, std::string_view stage, std::string_view message) {
    std::cerr << "[t048 standalone] " << stage << ": " << message << " (code " << code << ")\n";
    return code;
}

int run_self_test() {
    ui::State<bool> enabled{true};
    ui::UI ui_tree{
        ui::Column{
            ui::Header{"T048 standalone consumer"},
            ui::Toggle{"Enabled", enabled},
        }.padding(8.0f).gap(4.0f)};

    // Exercise the relocated Core package at runtime while remaining
    // deterministic on hosted workers that have no interactive desktop. The
    // separately compiled native-smoke path below forces StandaloneWindow
    // symbols to resolve from the attached package in this same executable.
    ui::HeadlessRenderer renderer{{160.0f, 80.0f}};
    if (!renderer.render(ui_tree)) return fail(3, "self-test", "headless render failed");
    if (renderer.pixel_width() != 160 || renderer.pixel_height() != 80) {
        return fail(3, "self-test", "unexpected headless surface size");
    }
    if (renderer.rgba_pixels().empty()) {
        return fail(3, "self-test", "headless render produced no pixels");
    }
    return 0;
}

int run_native_smoke() {
    const char* stage = "construct-ui";
    try {
        ui::State<bool> enabled{true};
        ui::UI ui_tree{
            ui::Column{
                ui::Header{"T048 standalone consumer"},
                ui::Toggle{"Enabled", enabled},
            }.padding(8.0f).gap(4.0f)};

        stage = "construct-window";
        ui::StandaloneWindow window{
            ui_tree,
            ui::WindowDesc{.title = "T048 standalone consumer",
                           .size = {240.0f, 120.0f},
                           .resizable = true}};
        if (!window.native_handle()) return fail(4, stage, "native handle is zero");
        if (!(window.scale_factor() > 0.0f)) return fail(4, stage, "invalid scale factor");

        stage = "poll";
        for (int i = 0; i < 4; ++i) (void)window.poll(0.0);
        if (!window.last_error().empty()) return fail(5, stage, window.last_error());

        stage = "close";
        window.request_close();
        if (!window.should_close()) return fail(6, stage, "request_close did not update state");
        return 0;
    } catch (const std::exception& e) {
        return fail(10, stage, e.what());
    } catch (...) {
        return fail(11, stage, "unknown exception");
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) return fail(2, "arguments", "expected --self-test or --native-smoke");

    const std::string_view mode{argv[1]};
    if (mode == "--self-test") return run_self_test();
    if (mode == "--native-smoke") return run_native_smoke();
    return fail(2, "arguments", "expected --self-test or --native-smoke");
}
