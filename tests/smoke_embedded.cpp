#include <nativeui/nativeui.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke embedded] " << stage << ": " << message << '\n';
    return 1;
}

bool same_size(ui::Size a, ui::Size b, float epsilon = 0.0001f) {
    return std::fabs(a.w - b.w) <= epsilon && std::fabs(a.h - b.h) <= epsilon;
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
        if (!(std::isfinite(child.scale_factor()) && child.scale_factor() > 0.0f)) {
            return fail(stage, "invalid child scale factor");
        }

        stage = "t043-invalid-size";
        const ui::Size child_size_before_invalid = child.size();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        for (const ui::Size invalid : {ui::Size{0.0f, 10.0f},
                                      ui::Size{10.0f, 0.0f},
                                      ui::Size{-1.0f, 10.0f},
                                      ui::Size{10.0f, -1.0f},
                                      ui::Size{nan, 10.0f},
                                      ui::Size{10.0f, inf}}) {
            if (child.set_size(invalid)) {
                return fail(stage, "invalid logical child size was accepted");
            }
            if (!same_size(child.size(), child_size_before_invalid)) {
                return fail(stage, "invalid logical child size changed authoritative viewport");
            }
        }

        stage = "t043-preferred-size";
        const ui::Size parent_size_before_grant = parent.size();
        int preferred_calls = 0;
        bool inside_preferred_callback = false;
        bool recursive_preferred_callback = false;
        bool preferred_grant_ok = true;
        child.set_preferred_size_callback([&](ui::Size preferred) {
            if (!std::isfinite(preferred.w) || !std::isfinite(preferred.h) ||
                preferred.w < 0.0f || preferred.h < 0.0f) {
                preferred_grant_ok = false;
            }
            if (inside_preferred_callback) recursive_preferred_callback = true;
            inside_preferred_callback = true;
            ++preferred_calls;
            if (preferred_calls == 1) {
                preferred_grant_ok = preferred_grant_ok && child.set_size({286.0f, 154.0f});
            }
            inside_preferred_callback = false;
        });
        if (preferred_calls != 1 || recursive_preferred_callback || !preferred_grant_ok) {
            return fail(stage, "preferred callback first/reentrant grant contract failed");
        }

        stage = "embedded-nonblocking-poll";
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 64 && !child.should_close(); ++i) {
            (void)application.poll(0.0);
            (void)child.poll();
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(1)) {
            return fail(stage, "64 non-blocking polls took more than one second");
        }
        if (!child.last_error().empty()) return fail(stage, child.last_error());
        if (recursive_preferred_callback || !preferred_grant_ok) {
            return fail(stage, "preferred callback recursively re-entered during configure");
        }
        if (!same_size(parent.size(), parent_size_before_grant)) {
            return fail(stage, "embedded child resize attempted to resize native parent");
        }
        if (!(std::isfinite(child.scale_factor()) && child.scale_factor() > 0.0f)) {
            return fail(stage, "child scale became invalid after configure");
        }

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
        if (!same_size(parent.size(), parent_size_before_grant)) {
            return fail(stage, "embedded resize changed native parent size");
        }

        stage = "teardown";
        child.set_preferred_size_callback({});
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
