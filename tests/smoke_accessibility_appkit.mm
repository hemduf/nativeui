#include "smoke_accessibility_appkit.hpp"

#include <nativeui/nativeui.hpp>

#import <AppKit/AppKit.h>
#import <objc/message.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke accessibility appkit] " << stage << ": "
              << message << '\n';
    return 1;
}

bool near(double actual, double expected) {
    return std::fabs(actual - expected) <= 0.0001;
}

} // namespace

int nativeui_smoke_accessibility::run_appkit_query_fixture() {
    const char* stage = "appkit-construct";
    ui::Application application;
    if (!application.valid()) return fail(stage, application.last_error());

    ui::State<float> value{0.25f};
    ui::UI app_ui{
        ui::Column{ui::Slider{value}.range(0.0f, 1.0f)}.padding(12.0f)};
    auto window = std::make_unique<ui::StandaloneWindow>(
        application,
        app_ui,
        ui::WindowDesc{.title = "NativeUI accessibility appkit fixture",
                       .size = {320.0f, 180.0f},
                       .resizable = true});
    if (!window->valid()) return fail(stage, window->last_error());

    stage = "appkit-initial-publication";
    for (int i = 0; i < 6 && !window->should_close(); ++i) {
        (void)application.poll(0.0);
        if (!std::string_view{window->last_error()}.empty()) break;
    }
    if (!std::string_view{window->last_error()}.empty()) {
        return fail(stage, window->last_error());
    }

    stage = "appkit-root-exposure";
    NSView* const view =
        reinterpret_cast<NSView*>(static_cast<std::uintptr_t>(window->native_handle()));
    if (!view) return fail(stage, "native handle is zero");
    NSArray* const children = [view accessibilityChildren];
    if (!children || [children count] != 1U) {
        return fail(stage, "production bridge did not expose exactly one root child");
    }
    id const root = [children objectAtIndex:0U];
    if (![root isAccessibilityElement]) {
        return fail(stage, "exposed root is not an accessibility element");
    }
    if (![[root accessibilityRole] isEqualToString:NSAccessibilitySliderRole]) {
        return fail(stage, "exposed root role is not the slider role");
    }

    stage = "appkit-initial-value";
    id const initial_value = [root accessibilityValue];
    if (![initial_value isKindOfClass:[NSNumber class]] ||
        !near([initial_value doubleValue], 0.25)) {
        return fail(stage, "exposed root value does not match the initial slider value");
    }

    // A committed UI-thread value update must reach AppKit through the same
    // proxy identity: the retained root element stays valid and reports the new
    // value on the next query.
    stage = "appkit-posted-update";
    if (!window->dispatcher().post([&value] { value.set(0.75f); })) {
        return fail(stage, "dispatcher rejected the slider value update");
    }
    (void)application.poll(0.0);
    if (!std::string_view{window->last_error()}.empty()) {
        return fail(stage, window->last_error());
    }
    if ([view accessibilityChildren] != nil &&
        [[view accessibilityChildren] count] == 1U &&
        [[view accessibilityChildren] objectAtIndex:0U] != root) {
        return fail(stage, "committed value update replaced the exposed root proxy");
    }
    id const updated_value = [root accessibilityValue];
    if (![updated_value isKindOfClass:[NSNumber class]] ||
        !near([updated_value doubleValue], 0.75)) {
        return fail(stage, "exposed root value did not observe the committed update");
    }

    window.reset();
    (void)application.poll(0.0);
    return 0;
}
