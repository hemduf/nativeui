#include <nativeui/nativeui.hpp>

#include <exception>
#include <iostream>
#include <string>
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

        // CODE_REVIEW.md requires platform changes to prove instance isolation.
        // Keep the primary window alive while a second independent window owns
        // active text-input state, uses the clipboard, and is then destroyed.
        // The surviving window must remain fully functional afterwards.
        stage = "multi-instance";
        {
            ui::State<std::string> sibling_text{"Sibling"};
            ui::UI sibling_ui{ui::TextInput{"Sibling text", sibling_text}};
            ui::StandaloneWindow sibling{
                sibling_ui,
                ui::WindowDesc{.title = "NativeUI sibling smoke",
                               .size = {300.0f, 120.0f},
                               .resizable = true}};
            if (!sibling.native_handle()) return fail(stage, "sibling native handle is zero");
            sibling.set_clipboard_text("NativeUI sibling clipboard smoke");
            for (int i = 0; i < 4 && !sibling.should_close(); ++i) {
                (void)sibling.poll(0.0);
            }
            if (!sibling.last_error().empty()) return fail(stage, sibling.last_error());
        }

        stage = "surviving-instance";
        window.set_clipboard_text("NativeUI clipboard after sibling destruction");
        if (!window.set_size({340.0f, 190.0f})) {
            return fail(stage, "surviving window set_size failed");
        }
        (void)window.poll(0.0);
        if (!window.last_error().empty()) return fail(stage, window.last_error());

        // Repeated construction/destruction catches process-global or teardown
        // coupling. TextInput ensures active text-input/timer lifecycle is part
        // of the exercised platform path rather than only inert windows.
        stage = "repeat-create-destroy";
        for (int iteration = 0; iteration < 3; ++iteration) {
            ui::State<std::string> text{"Cycle"};
            ui::UI cycle_ui{ui::TextInput{"Cycle text", text}};
            ui::StandaloneWindow cycle{
                cycle_ui,
                ui::WindowDesc{.title = "NativeUI lifecycle smoke",
                               .size = {280.0f, 110.0f},
                               .resizable = false}};
            if (!cycle.native_handle()) return fail(stage, "cycle native handle is zero");
            cycle.set_clipboard_text("NativeUI lifecycle clipboard " + std::to_string(iteration));
            for (int i = 0; i < 3 && !cycle.should_close(); ++i) {
                (void)cycle.poll(0.0);
            }
            if (!cycle.last_error().empty()) return fail(stage, cycle.last_error());
        }

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
