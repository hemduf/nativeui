#include "test_support.hpp"

#include "src/detail/scene_damage.hpp"

#include <limits>

namespace {

using ui::detail::map_device_damage;

void fractional_scale_maps_outward_with_antialias_guard() {
    const auto fractional = map_device_damage(
        {4.25f, 5.5f, 7.75f, 9.25f}, 1.5f, 200, 160);
    NUI_CHECK(fractional.has_value());
    NUI_CHECK(fractional->left == 4 && fractional->top == 6);
    NUI_CHECK(fractional->right == 20 && fractional->bottom == 25);
    NUI_CHECK(!fractional->covers_scene);

    const auto doubled = map_device_damage(
        {10.25f, 5.125f, 2.25f, 3.75f}, 2.0f, 400, 200);
    NUI_CHECK(doubled.has_value());
    NUI_CHECK(doubled->left == 18 && doubled->top == 8);
    NUI_CHECK(doubled->right == 27 && doubled->bottom == 20);
}

void logical_query_covers_the_actual_device_clip() {
    for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        const auto mapped = map_device_damage(
            {7.125f, 9.375f, 11.25f, 6.5f}, scale, 256, 192);
        NUI_CHECK(mapped.has_value());
        const double query_left = mapped->logical_query.x;
        const double query_top = mapped->logical_query.y;
        const double query_right = query_left + mapped->logical_query.w;
        const double query_bottom = query_top + mapped->logical_query.h;
        constexpr double epsilon = 1.0e-5;
        NUI_CHECK(query_left * scale <= mapped->left + epsilon);
        NUI_CHECK(query_top * scale <= mapped->top + epsilon);
        NUI_CHECK(query_right * scale >= mapped->right - epsilon);
        NUI_CHECK(query_bottom * scale >= mapped->bottom - epsilon);
    }
}

void clipping_and_full_scene_detection() {
    const auto clipped = map_device_damage({-1.0f, 4.0f, 3.0f, 4.0f},
                                           1.0f, 20, 20);
    NUI_CHECK(clipped.has_value());
    NUI_CHECK(clipped->left == 0 && clipped->top == 2);
    NUI_CHECK(clipped->right == 4 && clipped->bottom == 10);

    const auto full = map_device_damage({0.0f, 0.0f, 100.0f, 80.0f},
                                        2.0f, 200, 160);
    NUI_CHECK(full.has_value());
    NUI_CHECK(full->left == 0 && full->top == 0);
    NUI_CHECK(full->right == 200 && full->bottom == 160);
    NUI_CHECK(full->covers_scene);

    NUI_CHECK(!map_device_damage({120.0f, 0.0f, 1.0f, 1.0f},
                                 1.0f, 100, 100));
}

void invalid_and_unrepresentable_inputs_fail_closed() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    NUI_CHECK(!map_device_damage({nan, 0.0f, 1.0f, 1.0f}, 1.0f, 100, 100));
    NUI_CHECK(!map_device_damage({0.0f, 0.0f, 0.0f, 1.0f}, 1.0f, 100, 100));
    NUI_CHECK(!map_device_damage({0.0f, 0.0f, 1.0f, 1.0f}, 0.0f, 100, 100));
    NUI_CHECK(!map_device_damage({0.0f, 0.0f, 1.0f, 1.0f}, infinity, 100, 100));
    NUI_CHECK(!map_device_damage({8388608.0f, 0.0f, 10.0f, 10.0f},
                                 2.0f, 100, 100));
    NUI_CHECK(!map_device_damage({0.0f, 0.0f, 1.0f, 1.0f},
                                 1.0f, 0, 100));
}

void suite() {
    fractional_scale_maps_outward_with_antialias_guard();
    logical_query_covers_the_actual_device_clip();
    clipping_and_full_scene_detection();
    invalid_and_unrepresentable_inputs_fail_closed();
}

} // namespace

int main() {
    return test::run("t096_damage_mapping", &suite);
}
