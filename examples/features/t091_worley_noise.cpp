#include "example_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/noise.hpp>

#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {

ui::UI make_scene(const ui::Brush& f1, const ui::Brush& f2) {
    return ui::UI{ui::Canvas{320.0f, 180.0f,
        [f1, f2](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 320, 180}, {0.04f, 0.05f, 0.07f, 1.0f});
            g.fill_rounded_rect({16, 16, 136, 148}, 12.0f, f1);
            g.fill_rounded_rect({168, 16, 136, 148}, 12.0f, f2);
        }}};
}

ui::Brush colorize(const ui::NoiseSource& source) {
    return source.as_brush({0.02f, 0.04f, 0.08f, 1.0f},
                           {0.72f, 0.86f, 0.96f, 1.0f});
}

std::pair<bool, std::vector<std::uint8_t>> render(const ui::Brush& brush) {
    ui::UI tree{ui::Canvas{160.0f, 160.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 160, 160}, brush);
        }}};
    ui::HeadlessRenderer renderer{{160, 160}, 1.0f};
    if (!renderer.render(tree)) return {false, {}};
    return {true, renderer.rgba_pixels()};
}

int self_test() {
    constexpr ui::NoiseOptions options{
        .feature_size = 28.0f, .seed = 0xc311c0deu};
    const auto f1 = ui::NoiseSource::create(ui::NoiseType::WorleyF1, options);
    const auto f1_repeat =
        ui::NoiseSource::create(ui::NoiseType::WorleyF1, options);
    const auto f2 = ui::NoiseSource::create(ui::NoiseType::WorleyF2, options);
    const auto other = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1,
        {.feature_size = 28.0f, .seed = 0x12345678u});
    if (!f1.ok() || !f1_repeat.ok() || !f2.ok() || !other.ok()) {
        return example::fail("built-in Worley source failed to compile");
    }
    const auto [ok1, a] = render(colorize(f1.noise));
    const auto [ok2, b] = render(colorize(f1_repeat.noise));
    const auto [ok3, c] = render(colorize(f2.noise));
    const auto [ok4, d] = render(colorize(other.noise));
    if (!ok1 || !ok2 || !ok3 || !ok4)
        return example::fail("Worley render failed");
    if (a != b) return example::fail("same seed did not reproduce Worley F1");
    if (a == c) return example::fail("Worley F1 and F2 fields are aliased");
    if (a == d) return example::fail("changed seed did not change Worley F1");

    auto scene = make_scene(colorize(f1.noise), colorize(f2.noise));
    ui::HeadlessRenderer renderer{{320, 180}, 1.0f};
    if (!renderer.render(scene)) return example::fail("Worley scene failed");
    const auto original = renderer.rgba_pixels();
    if (!renderer.render(scene) || renderer.rgba_pixels() != original)
        return example::fail("repeated Worley scene was not deterministic");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    constexpr ui::NoiseOptions options{
        .feature_size = 28.0f, .seed = 0xc311c0deu};
    const auto f1 = ui::NoiseSource::create(ui::NoiseType::WorleyF1, options);
    const auto f2 = ui::NoiseSource::create(ui::NoiseType::WorleyF2, options);
    if (!f1.ok() || !f2.ok()) {
        std::cerr << "Worley SkSL failed: "
                  << (f1.ok() ? f2.diagnostic : f1.diagnostic) << '\n';
        return 1;
    }
    auto tree = make_scene(colorize(f1.noise), colorize(f2.noise));
    return example::run_window(tree, "NativeUI T091 Worley F1 / F2", {320, 180});
}
