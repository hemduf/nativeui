#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/scalar_source.hpp>

#include <iostream>
#include <utility>
#include <vector>

namespace {

ui::UI make_scene(const ui::Brush& gradient, const ui::Brush& noise) {
    return ui::UI{ui::Canvas{320.0f, 180.0f,
        [gradient, noise](ui::CanvasContext2D& g) {
            g.fill_rect({0.0f, 0.0f, 320.0f, 180.0f},
                        {0.04f, 0.05f, 0.07f, 1.0f});
            g.fill_rounded_rect({20.0f, 20.0f, 130.0f, 140.0f},
                                12.0f, gradient);
            g.fill_rounded_rect({170.0f, 20.0f, 130.0f, 140.0f},
                                12.0f, noise);
        }}};
}

int self_test() {
    const ui::Brush gradient{ui::LinearGradient{
        {0.0f, 0.0f},
        {130.0f, 0.0f},
        ui::Color{0.10f, 0.25f, 0.75f, 1.0f},
        ui::Color{0.85f, 0.35f, 0.10f, 1.0f}}};

    auto channel = ui::ScalarSource::from_brush(
        gradient, ui::ScalarChannel::Green);
    ui::ScalarSource constant{8.0f};
    auto copy = channel;
    ui::ScalarSource moved{std::move(copy)};
    constant = moved;

    const auto created = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 32.0f, .seed = 0x13579bdfu});
    if (!created.ok()) {
        return example::fail("ScalarSource noise input failed to compile");
    }
    auto noise_scalar = ui::ScalarSource::from_noise(created.noise);
    (void)noise_scalar;

    const auto noise_brush = created.noise.as_brush(
        {0.02f, 0.04f, 0.08f, 1.0f},
        {0.75f, 0.85f, 0.95f, 1.0f});
    auto scene = make_scene(gradient, noise_brush);

    ui::HeadlessRenderer renderer{{320, 180}, 1.0f};
    if (!renderer.render(scene)) {
        return example::fail("ScalarSource example scene failed to render");
    }
    const auto first = renderer.rgba_pixels();
    if (!renderer.render(scene) || renderer.rgba_pixels() != first) {
        return example::fail("ScalarSource example scene was not deterministic");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    const ui::Brush gradient{ui::LinearGradient{
        {0.0f, 0.0f},
        {130.0f, 0.0f},
        ui::Color{0.10f, 0.25f, 0.75f, 1.0f},
        ui::Color{0.85f, 0.35f, 0.10f, 1.0f}}};

    const auto created = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 32.0f, .seed = 0x13579bdfu});
    if (!created.ok()) {
        std::cerr << "ScalarSource noise input failed: "
                  << created.diagnostic << '\n';
        return 1;
    }

    const auto gradient_channel = ui::ScalarSource::from_brush(
        gradient, ui::ScalarChannel::Green);
    const auto noise_channel = ui::ScalarSource::from_noise(created.noise);
    const ui::ScalarSource emissive_intensity{8.0f};
    (void)gradient_channel;
    (void)noise_channel;
    (void)emissive_intensity;

    const auto noise_brush = created.noise.as_brush(
        {0.02f, 0.04f, 0.08f, 1.0f},
        {0.75f, 0.85f, 0.95f, 1.0f});
    auto tree = make_scene(gradient, noise_brush);
    return example::run_window(
        tree, "NativeUI ScalarSource inputs", {320, 180});
}
