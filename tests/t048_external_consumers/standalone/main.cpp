#include <nativeui/nativeui.hpp>

#include <exception>
#include <iostream>
#include <string_view>

namespace {
int fail(int code, std::string_view stage, std::string_view message) {
    std::cerr << "[t048 standalone] " << stage << ": " << message << " (code " << code << ")\n";
    return code;
}
}

int main(int argc, char** argv) {
    const char* stage = "arguments";
    try {
        if (argc != 2 || std::string_view{argv[1]} != "--self-test") {
            return fail(2, stage, "expected --self-test");
        }

        stage = "construct-ui";
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
        if (!window.native_handle()) return fail(3, stage, "native handle is zero");
        if (!(window.scale_factor() > 0.0f)) return fail(3, stage, "invalid scale factor");

        stage = "poll";
        for (int i = 0; i < 4; ++i) (void)window.poll(0.0);
        if (!window.last_error().empty()) return fail(4, stage, window.last_error());

        stage = "close";
        window.request_close();
        if (!window.should_close()) return fail(5, stage, "request_close did not update state");
        return 0;
    } catch (const std::exception& e) {
        return fail(10, stage, e.what());
    } catch (...) {
        return fail(11, stage, "unknown exception");
    }
}
