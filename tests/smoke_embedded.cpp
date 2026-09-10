#include <nativeui/nativeui.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke embedded] " << stage << ": " << message << '\n';
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

        stage = "construct-parent-ui";
        ui::UI parent_ui{
            ui::Column{
                ui::Header{"NativeUI embedded smoke parent"},
                ui::Spacer{120.0f},
            }.padding(12.0f).gap(8.0f)};

        stage = "construct-parent-window";
        ui::Application application;
        ui::StandaloneWindow parent{
            application,
            parent_ui,
            ui::WindowDesc{.title = "NativeUI embedded smoke parent",
                           .size = {420.0f, 280.0f},
                           .resizable = true}};
        if (!parent.native_handle()) return fail(stage, "parent native handle is zero");

        ui::State<bool> child_enabled{false};
        ui::UI child_ui{
            ui::Column{
                ui::Header{"Embedded child"},
                ui::Toggle{"Child enabled", child_enabled},
            }.padding(10.0f).gap(8.0f)};

        stage = "prepare-embedded-overlay";
        ui::OverlaySpec child_overlay;
        child_overlay.placement = ui::OverlayPlacement::Center;
        child_overlay.content = ui::make_spec(ui::Spacer{24.0f, 16.0f});
        const auto child_overlay_handle = child_ui.show_overlay(std::move(child_overlay));
        if (!child_overlay_handle.valid()) {
            return fail(stage, "show_overlay rejected a valid embedded overlay");
        }

        stage = "construct-embedded-view";
        ui::EmbeddedView child{child_ui, parent.native_handle(), {260.0f, 140.0f}};
        if (!child.native_handle()) return fail(stage, "embedded native handle is zero");
        if (!(child.scale_factor() > 0.0f)) return fail(stage, "invalid child scale factor");

        stage = "embedded-nonblocking-poll";
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 64 && !child.should_close(); ++i) {
            (void)child.poll();
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(1)) {
            return fail(stage, "64 non-blocking polls took more than one second");
        }
        if (!child.last_error().empty()) return fail(stage, child.last_error());

        stage = "embedded-overlay";
        if (!child_ui.close_overlay(child_overlay_handle) || child_overlay_handle.valid()) {
            return fail(stage, "embedded overlay close/stale-handle contract failed");
        }
        for (int i = 0; i < 4; ++i) {
            (void)application.poll(0.0);
            (void)child.poll();
        }
        if (!child.last_error().empty()) return fail(stage, child.last_error());

        stage = "embedded-resize";
        if (!child.set_size({300.0f, 160.0f})) return fail(stage, "child set_size failed");
        for (int i = 0; i < 4; ++i) {
            (void)application.poll(0.0);
            (void)child.poll();
        }
        if (!parent.last_error().empty()) return fail(stage, parent.last_error());
        if (!child.last_error().empty()) return fail(stage, child.last_error());

        stage = "teardown";
        child.request_close();
        parent.request_close();
        if (!child.should_close() || !parent.should_close()) {
            return fail(stage, "close state not propagated");
        }
        return 0;
    } catch (const std::exception& e) {
        return fail(stage, e.what());
    } catch (...) {
        return fail(stage, "unknown exception");
    }
}
