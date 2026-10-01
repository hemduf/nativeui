#include "../../src/detail/semantic_macos_frame.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

void check_near(double actual, double expected) {
    constexpr double epsilon = 1.0e-9;
    if (std::fabs(actual - expected) > epsilon) {
        throw std::runtime_error("check_near failed");
    }
}

void physical_top_down_bounds_convert_to_cocoa_screen_points() {
    const auto frame = ui::detail::macos_accessibility_screen_frame(
        ui::Rect{-118.0f, 68.0f, 60.0f, 24.0f},
        2.0f,
        900.0);

    CHECK(frame.has_value());
    check_near(frame->x, -59.0);
    check_near(frame->y, 854.0);
    check_near(frame->width, 30.0);
    check_near(frame->height, 12.0);
}

void multi_monitor_coordinates_remain_representable() {
    const auto frame = ui::detail::macos_accessibility_screen_frame(
        ui::Rect{-800.0f, -200.0f, 100.0f, 50.0f},
        1.25f,
        900.0);

    CHECK(frame.has_value());
    check_near(frame->x, -640.0);
    check_near(frame->y, 1020.0);
    check_near(frame->width, 80.0);
    check_near(frame->height, 40.0);
}

void invalid_coordinate_inputs_fail_closed() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const double double_nan = std::numeric_limits<double>::quiet_NaN();

    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, 10.0f}, 0.0f, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, 10.0f}, nan, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, 10.0f}, infinity, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{nan, 0.0f, 10.0f, 10.0f}, 1.0f, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, infinity, 10.0f, 10.0f}, 1.0f, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, -1.0f, 10.0f}, 1.0f, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, -1.0f}, 1.0f, 900.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, 10.0f}, 1.0f, 0.0));
    CHECK(!ui::detail::macos_accessibility_screen_frame(
        ui::Rect{0.0f, 0.0f, 10.0f, 10.0f}, 1.0f, double_nan));
}

} // namespace

int main() {
    try {
        physical_top_down_bounds_convert_to_cocoa_screen_points();
        multi_monitor_coordinates_remain_representable();
        invalid_coordinate_inputs_fail_closed();
        std::cout << "PASS semantic macOS frame conversion\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic macOS frame conversion: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}
