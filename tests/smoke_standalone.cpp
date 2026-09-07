#include <nativeui/nativeui.hpp>

#include <exception>
#include <iostream>
#include <memory>
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

        // CODE_REVIEW.md requires platform/lifecycle changes to prove real
        // instance isolation. Model multiple plug-in editors as independent
        // PUGL_MODULE children under one host-owned native parent. Destroy one
        // while its sibling remains active, then repeat construction/destruction.
        stage = "embedded-multi-instance";
        ui::State<std::string> child_a_text{"Child A"};
        ui::State<std::string> child_b_text{"Child B"};
        ui::UI child_a_ui{ui::TextInput{"Child A text", child_a_text}};
        ui::UI child_b_ui{ui::TextInput{"Child B text", child_b_text}};
        auto child_a = std::make_unique<ui::EmbeddedView>(
            child_a_ui, window.native_handle(), ui::Size{250.0f, 100.0f});
        ui::EmbeddedView child_b{
            child_b_ui, window.native_handle(), ui::Size{250.0f, 100.0f}};

        if (!child_a->native_handle() || !child_b.native_handle()) {
            return fail(stage, "embedded native handle is zero");
        }
        for (int i = 0; i < 4; ++i) {
            (void)window.poll(0.0);
            (void)child_a->poll();
            (void)child_b.poll();
        }
        if (!child_a->last_error().empty()) return fail(stage, child_a->last_error());
        if (!child_b.last_error().empty()) return fail(stage, child_b.last_error());

        stage = "embedded-survivor";
        child_a.reset();
        if (!child_b.set_size({260.0f, 110.0f})) {
            return fail(stage, "surviving embedded view set_size failed");
        }
        for (int i = 0; i < 4; ++i) {
            (void)window.poll(0.0);
            (void)child_b.poll();
        }
        if (!child_b.last_error().empty()) return fail(stage, child_b.last_error());

        stage = "embedded-repeat-create-destroy";
        for (int iteration = 0; iteration < 3; ++iteration) {
            (void)iteration;
            ui::State<std::string> text{"Cycle"};
            ui::UI cycle_ui{ui::TextInput{"Cycle text", text}};
            ui::EmbeddedView cycle{
                cycle_ui, window.native_handle(), ui::Size{240.0f, 96.0f}};
            if (!cycle.native_handle()) return fail(stage, "cycle native handle is zero");
            for (int i = 0; i < 3; ++i) {
                (void)window.poll(0.0);
                (void)cycle.poll();
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
