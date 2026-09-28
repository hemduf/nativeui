#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/noise.hpp>

#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

ui::FractalNoiseOptions options(ui::FractalNoiseMode mode) {
    ui::FractalNoiseOptions value;
    value.set_octaves(5)
         .set_lacunarity(2.0f)
         .set_gain(0.5f)
         .set_mode(mode);
    return value;
}

ui::Brush colorize(const ui::NoiseSource& source) {
    return source.as_brush({0.02f, 0.04f, 0.08f, 1.0f},
                           {0.78f, 0.90f, 0.98f, 1.0f});
}

std::pair<bool, std::vector<std::uint8_t>> render(const ui::Brush& brush) {
    ui::UI tree{ui::Canvas{144.0f, 144.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 144, 144}, brush);
        }}};
    ui::HeadlessRenderer renderer{{144, 144}, 1.0f};
    if (!renderer.render(tree)) return {false, {}};
    return {true, renderer.rgba_pixels()};
}

ui::UI make_scene(const ui::Brush& fbm,
                  const ui::Brush& turbulence,
                  const ui::Brush& ridged) {
    return ui::UI{ui::Canvas{480.0f, 180.0f,
        [fbm, turbulence, ridged](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 480, 180}, {0.04f, 0.05f, 0.07f, 1.0f});
            g.fill_rounded_rect({12, 16, 144, 148}, 10.0f, fbm);
            g.fill_rounded_rect({168, 16, 144, 148}, 10.0f, turbulence);
            g.fill_rounded_rect({324, 16, 144, 148}, 10.0f, ridged);
        }}};
}

int self_test() {
    constexpr ui::NoiseOptions base{
        .feature_size = 48.0f, .seed = 0x12345678u};
    const auto fbm = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base, options(ui::FractalNoiseMode::FBm));
    const auto fbm_repeat = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base, options(ui::FractalNoiseMode::FBm));
    const auto turbulence = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base,
        options(ui::FractalNoiseMode::Turbulence));
    const auto ridged = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base, options(ui::FractalNoiseMode::Ridged));
    if (!fbm.ok() || !fbm_repeat.ok() || !turbulence.ok() || !ridged.ok()) {
        return example::fail("fractal noise source failed to compile");
    }

    const auto [ok1, a] = render(colorize(fbm.noise));
    const auto [ok2, b] = render(colorize(fbm_repeat.noise));
    const auto [ok3, c] = render(colorize(turbulence.noise));
    const auto [ok4, d] = render(colorize(ridged.noise));
    if (!ok1 || !ok2 || !ok3 || !ok4) {
        return example::fail("fractal noise render failed");
    }
    if (a != b) return example::fail("same fractal configuration was not deterministic");
    if (a == c || a == d || c == d) {
        return example::fail("fractal modes are aliased");
    }

    auto scene = make_scene(
        colorize(fbm.noise), colorize(turbulence.noise), colorize(ridged.noise));
    ui::HeadlessRenderer renderer{{480, 180}, 1.0f};
    if (!renderer.render(scene)) return example::fail("fractal scene failed");
    const auto original = renderer.rgba_pixels();
    if (!renderer.render(scene) || renderer.rgba_pixels() != original) {
        return example::fail("repeated fractal scene was not deterministic");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    constexpr ui::NoiseOptions base{
        .feature_size = 48.0f, .seed = 0x12345678u};
    const auto fbm = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base, options(ui::FractalNoiseMode::FBm));
    const auto turbulence = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base,
        options(ui::FractalNoiseMode::Turbulence));
    const auto ridged = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, base, options(ui::FractalNoiseMode::Ridged));
    if (!fbm.ok() || !turbulence.ok() || !ridged.ok()) {
        std::cerr << "Fractal noise SkSL compilation failed\n";
        return 1;
    }

    auto tree = make_scene(
        colorize(fbm.noise), colorize(turbulence.noise), colorize(ridged.noise));
    return example::run_window(
        tree, "NativeUI T092 fBm / Turbulence / Ridged", {480, 180});
}
