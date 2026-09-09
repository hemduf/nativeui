#include <nativeui/nativeui.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

namespace {
int fail(int code, std::string_view stage, std::string_view message) {
    std::cerr << "[t048 embedded] " << stage << ": " << message << " (code " << code << ")\n";
    return code;
}

int run_self_test() {
    ui::State<bool> enabled{true};
    ui::UI child_ui{
        ui::Column{
            ui::Header{"T048 embedded consumer"},
            ui::Toggle{"Enabled", enabled},
        }.padding(8.0f).gap(4.0f)};

    // Runtime-check the relocated Core package without requiring a real host
    // window. The native-smoke path remains part of this same final executable,
    // so EmbeddedView and StandaloneWindow symbols must still resolve through
    // nativeui_attach_platform() during the external consumer link.
    ui::HeadlessRenderer renderer{{4.0f, 3.0f}};
    (void)child_ui;
    if (renderer.pixel_width() != 4 || renderer.pixel_height() != 3) {
        return fail(3, "self-test", "relocated Core renderer is unavailable");
    }
    return 0;
}

int run_native_smoke() {
    const char* stage = "construct-parent-ui";
    try {
        ui::UI parent_ui{
            ui::Column{
                ui::Header{"T048 embedded host seam"},
                ui::Spacer{80.0f},
            }.padding(8.0f).gap(4.0f)};

        stage = "construct-parent";
        ui::StandaloneWindow parent{
            parent_ui,
            ui::WindowDesc{.title = "T048 embedded host seam",
                           .size = {300.0f, 180.0f},
                           .resizable = true}};
        const ui::NativeParentHandle host_parent = parent.native_handle();
        if (!host_parent) return fail(4, stage, "native parent handle is zero");

        stage = "construct-child-ui";
        ui::State<bool> enabled{true};
        ui::UI child_ui{
            ui::Column{
                ui::Header{"T048 embedded consumer"},
                ui::Toggle{"Enabled", enabled},
            }.padding(8.0f).gap(4.0f)};

        stage = "construct-child";
        {
            ui::EmbeddedView child{child_ui, host_parent, {220.0f, 100.0f}};
            if (!child.native_handle()) return fail(5, stage, "embedded native handle is zero");
            if (!(child.scale_factor() > 0.0f)) return fail(5, stage, "invalid scale factor");

            stage = "poll";
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 64; ++i) {
                (void)parent.poll(0.0);
                (void)child.poll();
            }
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
                return fail(6, stage, "non-blocking poll loop exceeded one second");
            }
            if (!parent.last_error().empty()) return fail(7, stage, parent.last_error());
            if (!child.last_error().empty()) return fail(7, stage, child.last_error());

            stage = "resize";
            if (!child.set_size({230.0f, 110.0f})) return fail(8, stage, "set_size failed");
            child.request_close();
            if (!child.should_close()) return fail(9, stage, "request_close did not update state");
        }

        stage = "close-parent";
        parent.request_close();
        if (!parent.should_close()) return fail(10, stage, "parent request_close did not update state");
        return 0;
    } catch (const std::exception& e) {
        return fail(11, stage, e.what());
    } catch (...) {
        return fail(12, stage, "unknown exception");
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
