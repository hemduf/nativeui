#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/noise.hpp>

#include <cstdint>
#include <iostream>
#include <utility>

namespace {

ui::UI make_scene(const ui::Brush& left, const ui::Brush& right) {
    return ui::UI{ui::Canvas{320.0f, 180.0f,
        [left, right](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 320, 180}, {0.04f, 0.05f, 0.07f, 1.0f});
            g.fill_rounded_rect({16, 16, 136, 148}, 12.0f, left);
            g.fill_rounded_rect({168, 16, 136, 148}, 12.0f, right);
        }}};
}

ui::Brush slate(const ui::NoiseSource& source) {
    return source.as_brush({0.03f, 0.05f, 0.09f, 1.0f},
                           {0.55f, 0.72f, 0.86f, 1.0f});
}

int self_test() {
    const auto first = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 72.0f, .seed = 0x31415926u});
    const auto repeat = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 72.0f, .seed = 0x31415926u});
    const auto other = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 72.0f, .seed = 0x27182818u});
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 72.0f, .seed = 0x31415926u});
    if (!first.ok() || !repeat.ok() || !other.ok() || !perlin.ok()) {
        return example::fail("built-in simplex-noise source failed to compile");
    }
    const auto render_field = [](const ui::Brush& brush) {
        ui::UI tree{ui::Canvas{320.0f, 180.0f,
            [brush](ui::CanvasContext2D& g) {
                g.fill_rect({0, 0, 320, 180}, brush);
            }}};
        ui::HeadlessRenderer renderer{{320, 180}, 1.0f};
        if (!renderer.render(tree)) {
            return std::pair<bool, std::vector<std::uint8_t>>{false, {}};
        }
        return std::pair<bool, std::vector<std::uint8_t>>{
            true, renderer.rgba_pixels()};
    };
    const auto [ok_a, a] = render_field(slate(first.noise));
    const auto [ok_b, b] = render_field(slate(repeat.noise));
    const auto [ok_c, c] = render_field(slate(other.noise));
    const auto [ok_d, d] = render_field(slate(perlin.noise));
    if (!ok_a || !ok_b || !ok_c || !ok_d)
        return example::fail("noise render failed");
    if (a != b) return example::fail("same seed did not reproduce the field");
    if (a == c) return example::fail("different seed produced the same field");
    if (a == d) return example::fail("simplex and perlin fields are aliased");
    auto scene = make_scene(slate(first.noise), slate(perlin.noise));
    ui::HeadlessRenderer renderer{{320, 180}, 1.0f};
    if (!renderer.render(scene)) return example::fail("noise render failed");
    const auto original = renderer.rgba_pixels();
    if (!renderer.render(scene) || renderer.rgba_pixels() != original) {
        return example::fail("repeated noise render was not deterministic");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    const auto simplex = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 72.0f, .seed = 0x31415926u});
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 72.0f, .seed = 0x31415926u});
    if (!simplex.ok() || !perlin.ok()) {
        std::cerr << "Noise SkSL failed: "
                  << (simplex.ok() ? perlin.diagnostic : simplex.diagnostic)
                  << '\n';
        return 1;
    }
    auto tree = make_scene(slate(simplex.noise), slate(perlin.noise));
    return example::run_window(tree, "NativeUI T090 simplex noise", {320, 180});
}
