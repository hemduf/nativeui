#include <nativeui/noise.hpp>

#include "test_support.hpp"
#include "src/detail/shader_brush_access.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

static_assert(noexcept(std::declval<ui::FractalNoiseOptions&>().set_octaves(std::uint8_t{})));
static_assert(noexcept(std::declval<ui::FractalNoiseOptions&>().set_lacunarity(0.0f)));
static_assert(noexcept(std::declval<ui::FractalNoiseOptions&>().set_gain(0.0f)));
static_assert(noexcept(std::declval<ui::FractalNoiseOptions&>().set_mode(ui::FractalNoiseMode::FBm)));
static_assert(noexcept(std::declval<const ui::FractalNoiseOptions&>().octaves()));
static_assert(noexcept(std::declval<const ui::FractalNoiseOptions&>().lacunarity()));
static_assert(noexcept(std::declval<const ui::FractalNoiseOptions&>().gain()));
static_assert(noexcept(std::declval<const ui::FractalNoiseOptions&>().mode()));

void option_contract() {
    ui::FractalNoiseOptions options;
    NUI_CHECK(options.octaves() == 4);
    NUI_CHECK(options.lacunarity() == 2.0f);
    NUI_CHECK(options.gain() == 0.5f);
    NUI_CHECK(options.mode() == ui::FractalNoiseMode::FBm);

    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    NUI_CHECK(&options.set_octaves(6) == &options);
    NUI_CHECK(&options.set_lacunarity(inf) == &options);
    NUI_CHECK(&options.set_gain(-inf) == &options);
    NUI_CHECK(&options.set_mode(ui::FractalNoiseMode::Ridged) == &options);
    NUI_CHECK(options.octaves() == 6);
    NUI_CHECK(options.lacunarity() == inf);
    NUI_CHECK(options.gain() == -inf);
    NUI_CHECK(options.mode() == ui::FractalNoiseMode::Ridged);
    options.set_lacunarity(nan);
    NUI_CHECK(std::isnan(options.lacunarity()));
}

void validation_contract() {
    auto valid = ui::FractalNoiseOptions{};
    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        const auto result = ui::NoiseSource::create_fractal(
            base, {.feature_size = 48.0f, .seed = 0x12345678u}, valid);
        NUI_CHECK(result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::None);
        NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(
            result.noise.as_brush()));
    }

    for (auto base : {ui::NoiseType::WorleyF1, ui::NoiseType::WorleyF2,
                      static_cast<ui::NoiseType>(255)}) {
        const auto result = ui::NoiseSource::create_fractal(base);
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    for (std::uint8_t octaves : {std::uint8_t{0}, std::uint8_t{7}}) {
        auto options = ui::FractalNoiseOptions{};
        options.set_octaves(octaves);
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Value, {}, options);
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float lacunarity : {0.999f, 4.001f, nan, inf, -inf}) {
        auto options = ui::FractalNoiseOptions{};
        options.set_lacunarity(lacunarity);
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Perlin, {}, options);
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }
    for (float gain : {-0.001f, 1.001f, nan, inf, -inf}) {
        auto options = ui::FractalNoiseOptions{};
        options.set_gain(gain);
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Simplex, {}, options);
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    auto bad_mode = ui::FractalNoiseOptions{};
    bad_mode.set_mode(static_cast<ui::FractalNoiseMode>(255));
    const auto bad = ui::NoiseSource::create_fractal(
        ui::NoiseType::Value, {}, bad_mode);
    NUI_CHECK(!bad.ok());
    NUI_CHECK(bad.error == ui::NoiseCreateError::InvalidArgument);

    for (float size : {0.0f, -1.0f, nan, inf}) {
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Value, {.feature_size = size});
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }
}

} // namespace

int main() {
    try {
        option_contract();
        validation_contract();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T092 fractal noise: " << e.what() << '\n';
        return 1;
    }
}
