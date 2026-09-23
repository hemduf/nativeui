#include "src/detail/scene_extent.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using ui::detail::validate_scene_extent;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const ui::Size invalid : {ui::Size{0.0f, 2.0f},
                                   ui::Size{2.0f, -1.0f},
                                   ui::Size{nan, 2.0f},
                                   ui::Size{2.0f, inf},
                                   ui::Size{2147483648.0f, 1.0f},
                                   ui::Size{10000.0f, 10000.0f}}) {
        if (validate_scene_extent(invalid, 1.0f, 16384)) {
            std::cerr << "T095 accepted an invalid or over-budget scene extent\n";
            return 1;
        }
    }
    for (const float invalid : {0.0f, -1.0f, nan, inf}) {
        if (validate_scene_extent({64.0f, 64.0f}, invalid, 16384)) {
            std::cerr << "T095 accepted an invalid scale\n";
            return 1;
        }
    }
    if (validate_scene_extent({64.0f, 64.0f}, 1.0f, 63)) {
        std::cerr << "T095 exceeded backend render-target cap\n";
        return 1;
    }
    const auto valid = validate_scene_extent({47.5f, 32.49f}, 2.0f, 16384);
    if (!valid || valid->width != 48 || valid->height != 32) {
        std::cerr << "T095 rounded a valid extent incorrectly\n";
        return 1;
    }
    return 0;
}
