#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/noise.hpp>

#include <iostream>

namespace {

ui::UI make_scene(const ui::Brush& left, const ui::Brush& right) {
    return ui::UI{ui::Canvas{320.0f, 180.0f,
        [left, right](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 320, 180}, {0.04f, 0.05f, 0.07f, 1.0f});
            g.fill_rounded_rect({16, 16, 136, 148}, 12.0f, left);
            g.fill_rounded_rect({168, 16, 136, 148}, 12.0f, right);
        }}};
}

int self_test() {
    const auto first = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto second = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x87654321u});
    if (!first.ok() || !second.ok()) {
        return example::fail("built-in value-noise source failed to compile");
    }
    const auto make_brush = [](const ui::NoiseSource& source) {
        return source.as_brush({0.08f, 0.12f, 0.25f, 1.0f},
                               {0.55f, 0.75f, 1.0f, 1.0f});
    };
    auto scene = make_scene(make_brush(first.noise), make_brush(second.noise));
    ui::HeadlessRenderer renderer{{320, 180}, 1.0f};
    if (!renderer.render(scene)) return example::fail("noise render failed");
    const auto left = renderer.pixel(64, 80);
    const auto right = renderer.pixel(216, 80);
    if (left.r == right.r && left.g == right.g && left.b == right.b) {
        return example::fail("different seeds produced the same sample");
    }
    const auto original = renderer.rgba_pixels();
    if (!renderer.render(scene) || renderer.rgba_pixels() != original) {
        return example::fail("repeated noise render was not deterministic");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    const auto first = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto second = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x87654321u});
    if (!first.ok() || !second.ok()) {
        std::cerr << "Value noise SkSL failed: "
                  << (first.ok() ? second.diagnostic : first.diagnostic) << '\n';
        return 1;
    }
    auto tree = make_scene(
        first.noise.as_brush({0.08f, 0.12f, 0.25f, 1.0f},
                             {0.55f, 0.75f, 1.0f, 1.0f}),
        second.noise.as_brush({0.20f, 0.08f, 0.12f, 1.0f},
                              {1.0f, 0.70f, 0.45f, 1.0f}));
    return example::run_window(tree, "NativeUI T088 value noise", {320, 180});
}
