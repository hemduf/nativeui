#pragma once

#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace example {

inline int fail(std::string_view message) {
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

inline bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

inline ui::InputEvent key(ui::Key key, bool shift = false) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyDown;
    event.key = key;
    event.shift = shift;
    return event;
}

inline ui::InputEvent pointer(ui::InputType type, float x, float y) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    return event;
}

inline int run_window(ui::UI& ui, std::string title, ui::Size size) {
#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    (void)ui;
    (void)title;
    (void)size;
    return fail("window mode is disabled in self-test-only validation builds");
#else
    ui::Application application;
    ui::StandaloneWindow window{
        application,
        ui,
        ui::WindowDesc{.title = std::move(title),
                       .size = size,
                       .resizable = true,
                       .min_size = std::nullopt,
                       .max_size = std::nullopt}};
    return application.run();
#endif
}

} // namespace example
