#include <nativeui/nativeui.hpp>

#include <exception>
#include <iostream>
#include <string_view>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke standalone] " << stage << ": " << message << '\n';
    return 1;
}

} // namespace

int main() {
    const char* stage = "font-manager";
    try {
        ui::TextStyle font_style{};
        const auto default_face = ui::FontManager::match(font_style, U'A');
        if (!default_face || !default_face.glyph_available) {
            return fail(stage, "platform font manager could not resolve Latin text");
        }

        stage = "construct-ui";
        ui::State<bool> enabled{true};
        ui::UI app{
            ui::Column{
                ui::Header{"NativeUI standalone smoke"},
                ui::Toggle{"Enabled", enabled},
            }.padding(16.0f).gap(12.0f)};

        stage = "construct-window";
        ui::StandaloneWindow window{
            app,
            ui::WindowDesc{.title = "NativeUI standalone smoke",
                           .size = {320.0f, 180.0f},
                           .resizable = true}};

        stage = "validate-native-handle";
        if (!window.native_handle()) return fail(stage, "native handle is zero");
        if (!(window.scale_factor() > 0.0f)) return fail(stage, "invalid scale factor");

        // Exercise the native clipboard bridge directly. This is intentionally
        // part of the real platform smoke test rather than a mocked TextArea
        // test, since the macOS regression occurred inside Pugl/NSPasteboard.
        stage = "clipboard";
        window.set_clipboard_text("NativeUI clipboard smoke");

        stage = "poll";
        for (int i = 0; i < 8 && !window.should_close(); ++i) {
            if (!window.poll(0.0) && !window.should_close()) {
                return fail(stage, window.last_error());
            }
        }
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        stage = "resize";
        if (!window.set_size({360.0f, 200.0f})) return fail(stage, "set_size failed");
        for (int i = 0; i < 4 && !window.should_close(); ++i) {
            (void)window.poll(0.0);
        }
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        stage = "close";
        window.request_close();
        if (!window.should_close()) return fail(stage, "request_close did not update state");
        return 0;
    } catch (const std::exception& e) {
        return fail(stage, e.what());
    } catch (...) {
        return fail(stage, "unknown exception");
    }
}
