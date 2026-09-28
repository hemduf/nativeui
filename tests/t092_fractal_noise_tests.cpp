#include <nativeui/headless.hpp>
#include <nativeui/noise.hpp>

#include "test_support.hpp"
#include "src/detail/noise_math.hpp"
#include "src/detail/shader_brush_access.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>
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

ui::FractalNoiseOptions fractal_options(
    ui::FractalNoiseMode mode,
    std::uint8_t octaves = 4,
    float lacunarity = 2.0f,
    float gain = 0.5f) {
    ui::FractalNoiseOptions options;
    options.set_mode(mode)
           .set_octaves(octaves)
           .set_lacunarity(lacunarity)
           .set_gain(gain);
    return options;
}

double base_reference(ui::NoiseType base,
                      std::uint32_t seed,
                      double feature_size,
                      double x,
                      double y) {
    switch (base) {
        case ui::NoiseType::Value:
            return ui::detail::value_noise_reference(
                seed, feature_size, x, y);
        case ui::NoiseType::Perlin:
            return ui::detail::perlin_noise_reference(
                seed, feature_size, x, y);
        case ui::NoiseType::Simplex:
            return ui::detail::simplex_noise_reference(
                seed, feature_size, x, y);
        case ui::NoiseType::WorleyF1:
        case ui::NoiseType::WorleyF2:
            break;
    }
    return 0.5;
}

double fractal_reference(ui::NoiseType base,
                         ui::NoiseOptions base_options,
                         const ui::FractalNoiseOptions& options,
                         double x,
                         double y) {
    double frequency = 1.0;
    double amplitude = 1.0;
    double weight_sum = 0.0;
    double total = 0.0;

    for (std::uint8_t octave = 0; octave < options.octaves(); ++octave) {
        const double n = base_reference(
            base, base_options.seed, double(base_options.feature_size),
            x * frequency, y * frequency);
        const double s = 2.0 * n - 1.0;
        switch (options.mode()) {
            case ui::FractalNoiseMode::FBm:
                total += amplitude * s;
                break;
            case ui::FractalNoiseMode::Turbulence:
                total += amplitude * std::abs(s);
                break;
            case ui::FractalNoiseMode::Ridged: {
                const double ridge = 1.0 - std::abs(s);
                total += amplitude * ridge * ridge;
                break;
            }
        }
        weight_sum = weight_sum + amplitude;
        frequency = frequency * double(options.lacunarity());
        amplitude = amplitude * double(options.gain());
    }

    double value = options.mode() == ui::FractalNoiseMode::FBm
        ? 0.5 + 0.5 * (total / weight_sum)
        : total / weight_sum;
    return std::clamp(value, 0.0, 1.0);
}

ui::Rgba8 render_pixel(const ui::Brush& brush, int x, int y) {
    ui::UI tree{ui::Canvas{64.0f, 64.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 64, 64}, brush);
        }}};
    ui::HeadlessRenderer renderer{{64, 64}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    return renderer.pixel(x, y);
}

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

    // Setters are value-only: representative finite payloads round-trip as
    // exact float values without validation or canonicalization.
    constexpr std::uint32_t lacunarity_bits = 0x3f123456u;
    constexpr std::uint32_t gain_bits = 0xbf654321u;
    const float finite_lacunarity = std::bit_cast<float>(lacunarity_bits);
    const float finite_gain = std::bit_cast<float>(gain_bits);
    options.set_lacunarity(finite_lacunarity).set_gain(finite_gain);
    NUI_CHECK(std::bit_cast<std::uint32_t>(options.lacunarity()) ==
              lacunarity_bits);
    NUI_CHECK(std::bit_cast<std::uint32_t>(options.gain()) == gain_bits);
    options.set_gain(-0.0f);
    NUI_CHECK(std::bit_cast<std::uint32_t>(options.gain()) ==
              std::bit_cast<std::uint32_t>(-0.0f));
}

void validation_contract() {
    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            for (auto options : {
                     fractal_options(mode, 1, 1.0f, 0.0f),
                     fractal_options(mode, 6, 4.0f, 1.0f),
                     fractal_options(mode, 4, 2.0f, 0.5f)}) {
                const auto result = ui::NoiseSource::create_fractal(
                    base, {.feature_size = 48.0f, .seed = 0x12345678u},
                    options);
                NUI_CHECK(result.ok());
                NUI_CHECK(result.error == ui::NoiseCreateError::None);
                NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(
                    result.noise.as_brush()));
            }
        }
    }

    for (auto base : {ui::NoiseType::WorleyF1, ui::NoiseType::WorleyF2,
                      static_cast<ui::NoiseType>(255)}) {
        const auto result = ui::NoiseSource::create_fractal(base);
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    for (std::uint8_t octaves : {std::uint8_t{0}, std::uint8_t{7}}) {
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Value, {},
            fractal_options(ui::FractalNoiseMode::FBm, octaves));
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float lacunarity : {0.999f, 4.001f, nan, inf, -inf}) {
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Perlin, {},
            fractal_options(ui::FractalNoiseMode::FBm, 4, lacunarity));
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }
    for (float gain : {-0.001f, 1.001f, nan, inf, -inf}) {
        const auto result = ui::NoiseSource::create_fractal(
            ui::NoiseType::Simplex, {},
            fractal_options(ui::FractalNoiseMode::FBm, 4, 2.0f, gain));
        NUI_CHECK(!result.ok());
        NUI_CHECK(result.error == ui::NoiseCreateError::InvalidArgument);
    }

    auto bad_mode = fractal_options(ui::FractalNoiseMode::FBm);
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

void recurrence_contract() {
    double frequency = 1.0;
    double amplitude = 1.0;
    double weight_sum = 0.0;
    constexpr std::array<double, 4> expected_frequency{
        1.0, 2.5, 6.25, 15.625};
    constexpr std::array<double, 4> expected_amplitude{
        1.0, 0.25, 0.0625, 0.015625};
    constexpr std::array<double, 4> expected_weight{
        1.0, 1.25, 1.3125, 1.328125};

    for (std::size_t i = 0; i < expected_frequency.size(); ++i) {
        NUI_CHECK(frequency == expected_frequency[i]);
        NUI_CHECK(amplitude == expected_amplitude[i]);
        weight_sum = weight_sum + amplitude;
        NUI_CHECK(weight_sum == expected_weight[i]);
        frequency = frequency * 2.5;
        amplitude = amplitude * 0.25;
    }
}

void renderer_float_recurrence_contract() {
    // These bit patterns freeze iterative float multiplication at values where
    // exponentiation/reassociation is observably different by one or more ULPs.
    constexpr std::array<std::uint32_t, 6> frequency_bits{
        0x3f800000u, 0x3fd9999au, 0x4038f5c3u,
        0x409d374cu, 0x4105a234u, 0x41632d59u};
    constexpr std::array<std::uint32_t, 6> amplitude_bits{
        0x3f800000u, 0x3e99999au, 0x3db851ecu,
        0x3cdd2f1cu, 0x3c04b5deu, 0x3b1f40a4u};

    constexpr float lacunarity = 1.7f;
    constexpr float gain = 0.3f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    for (std::size_t i = 0; i < frequency_bits.size(); ++i) {
        NUI_CHECK(std::bit_cast<std::uint32_t>(frequency) ==
                  frequency_bits[i]);
        NUI_CHECK(std::bit_cast<std::uint32_t>(amplitude) ==
                  amplitude_bits[i]);
        frequency = frequency * lacunarity;
        amplitude = amplitude * gain;
    }

    const float pow_frequency =
        static_cast<float>(std::pow(double(lacunarity), 5.0));
    const float pow_amplitude =
        static_cast<float>(std::pow(double(gain), 5.0));
    NUI_CHECK(std::bit_cast<std::uint32_t>(pow_frequency) !=
              frequency_bits[5]);
    NUI_CHECK(std::bit_cast<std::uint32_t>(pow_amplitude) !=
              amplitude_bits[5]);
}

void same_seed_octave_contract() {
    constexpr ui::NoiseOptions base_options{
        .feature_size = 48.0f, .seed = 0x12345678u};
    const auto two = fractal_options(
        ui::FractalNoiseMode::FBm, 2, 2.0f, 0.5f);
    constexpr double x = 13.0;
    constexpr double y = 7.0;

    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        const double n0 = base_reference(
            base, base_options.seed, base_options.feature_size, x, y);
        const double n1 = base_reference(
            base, base_options.seed, base_options.feature_size,
            x * 2.0, y * 2.0);
        const double expected =
            0.5 + 0.5 * (((2.0 * n0 - 1.0) +
                           0.5 * (2.0 * n1 - 1.0)) / 1.5);
        NUI_CHECK(std::abs(fractal_reference(
            base, base_options, two, x, y) - expected) < 1e-15);

        // Raster parity against the same-seed oracle makes an undocumented
        // per-octave seed salt observable.
        const auto source = ui::NoiseSource::create_fractal(
            base, base_options, two);
        NUI_CHECK(source.ok());
        const auto pixel = render_pixel(source.noise.as_brush(), 12, 6);
        const double raster_expected = fractal_reference(
            base, base_options, two, 12.5, 6.5);
        NUI_CHECK(std::abs(double(pixel.r) / 255.0 - raster_expected) < 0.015);
    }
}

void reference_contract() {
    constexpr ui::NoiseOptions base_options{
        .feature_size = 48.0f, .seed = 0x12345678u};
    struct Vector {
        ui::NoiseType base;
        ui::FractalNoiseMode mode;
        double expected;
    };
    const Vector vectors[]{
        {ui::NoiseType::Value, ui::FractalNoiseMode::FBm,
         0.5312239904863917},
        {ui::NoiseType::Value, ui::FractalNoiseMode::Turbulence,
         0.25212095437044274},
        {ui::NoiseType::Value, ui::FractalNoiseMode::Ridged,
         0.5871872840482263},
        {ui::NoiseType::Perlin, ui::FractalNoiseMode::FBm,
         0.48019097844723124},
        {ui::NoiseType::Perlin, ui::FractalNoiseMode::Turbulence,
         0.0700182917340832},
        {ui::NoiseType::Perlin, ui::FractalNoiseMode::Ridged,
         0.8686062383547771},
        {ui::NoiseType::Simplex, ui::FractalNoiseMode::FBm,
         0.4773825473259691},
        {ui::NoiseType::Simplex, ui::FractalNoiseMode::Turbulence,
         0.29721275441114403},
        {ui::NoiseType::Simplex, ui::FractalNoiseMode::Ridged,
         0.5119515608898592},
    };
    for (const auto& v : vectors) {
        const double actual = fractal_reference(
            v.base, base_options, fractal_options(v.mode), 13.0, 7.0);
        NUI_CHECK(std::abs(actual - v.expected) < 1e-12);
    }

    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        const double n = base_reference(
            base, base_options.seed, base_options.feature_size, 13.0, 7.0);
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const double s = 2.0 * n - 1.0;
            const double expected =
                mode == ui::FractalNoiseMode::FBm ? n :
                mode == ui::FractalNoiseMode::Turbulence ? std::abs(s) :
                (1.0 - std::abs(s)) * (1.0 - std::abs(s));
            const auto one = fractal_options(mode, 1, 3.0f, 0.75f);
            NUI_CHECK(std::abs(fractal_reference(
                base, base_options, one, 13.0, 7.0) - expected) < 1e-15);

            const auto zero_gain = fractal_options(mode, 6, 4.0f, 0.0f);
            NUI_CHECK(std::abs(fractal_reference(
                base, base_options, zero_gain, 13.0, 7.0) - expected) < 1e-15);
        }
    }
}

void raster_contract() {
    constexpr ui::NoiseOptions base_options{
        .feature_size = 48.0f, .seed = 0x12345678u};
    constexpr std::array<std::array<int, 2>, 5> samples{{
        {0, 0}, {8, 8}, {13, 7}, {31, 23}, {52, 45}
    }};

    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const auto options = fractal_options(mode);
            const auto source = ui::NoiseSource::create_fractal(
                base, base_options, options);
            NUI_CHECK(source.ok());
            const auto brush = source.noise.as_brush();
            for (const auto& xy : samples) {
                const auto pixel = render_pixel(brush, xy[0], xy[1]);
                const double expected = fractal_reference(
                    base, base_options, options,
                    double(xy[0]) + 0.5, double(xy[1]) + 0.5);
                NUI_CHECK(std::abs(double(pixel.r) / 255.0 - expected) < 0.015);
                NUI_CHECK(pixel.r == pixel.g && pixel.r == pixel.b);
                NUI_CHECK(pixel.a == 255);
            }
        }
    }
}

void degeneracy_contract() {
    constexpr ui::NoiseOptions base_options{
        .feature_size = 48.0f, .seed = 0x12345678u};
    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const auto one = ui::NoiseSource::create_fractal(
                base, base_options, fractal_options(mode, 1, 4.0f, 1.0f));
            const auto zero = ui::NoiseSource::create_fractal(
                base, base_options, fractal_options(mode, 6, 4.0f, 0.0f));
            NUI_CHECK(one.ok() && zero.ok());
            for (const auto xy : {
                     std::array<int, 2>{3, 5},
                     std::array<int, 2>{17, 9},
                     std::array<int, 2>{41, 37}}) {
                const auto a = render_pixel(one.noise.as_brush(), xy[0], xy[1]);
                const auto b = render_pixel(zero.noise.as_brush(), xy[0], xy[1]);
                NUI_CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
            }
        }
    }
}

void six_octave_grid_contract() {
    constexpr ui::NoiseOptions base_options{
        .feature_size = 37.0f, .seed = 0x6a09e667u};

    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const auto options = fractal_options(mode, 6, 1.7f, 0.3f);
            for (int y = -96; y <= 96; y += 16) {
                for (int x = -96; x <= 96; x += 16) {
                    const double a = fractal_reference(
                        base, base_options, options, double(x), double(y));
                    const double b = fractal_reference(
                        base, base_options, options, double(x), double(y));
                    NUI_CHECK(std::isfinite(a));
                    NUI_CHECK(a >= 0.0 && a <= 1.0);
                    NUI_CHECK(a == b);
                }
            }

            const auto first = ui::NoiseSource::create_fractal(
                base, base_options, options);
            const auto second = ui::NoiseSource::create_fractal(
                base, base_options, options);
            NUI_CHECK(first.ok() && second.ok());

            const auto render = [](const ui::Brush& brush) {
                ui::UI tree{ui::Canvas{48.0f, 48.0f,
                    [brush](ui::CanvasContext2D& g) {
                        g.fill_rect({0, 0, 48, 48}, brush);
                    }}};
                ui::HeadlessRenderer renderer{{48, 48}, 1.0f};
                NUI_CHECK(renderer.render(tree));
                return renderer.rgba_pixels();
            };
            const auto pixels_a = render(first.noise.as_brush());
            const auto pixels_b = render(second.noise.as_brush());
            NUI_CHECK(pixels_a == pixels_b);
            NUI_CHECK(!pixels_a.empty());
        }
    }
}

void tiny_feature_size_contract() {
    constexpr ui::NoiseOptions tiny{
        .feature_size = 1.0e-30f, .seed = 0x12345678u};
    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const auto options = fractal_options(mode, 6, 4.0f, 1.0f);
            const double reference = fractal_reference(
                base, tiny, options, 8.5, 8.5);
            NUI_CHECK(std::isfinite(reference) &&
                      reference >= 0.0 && reference <= 1.0);
            const auto source = ui::NoiseSource::create_fractal(
                base, tiny, options);
            NUI_CHECK(source.ok());
            const auto pixel = render_pixel(source.noise.as_brush(), 8, 8);
            NUI_CHECK(pixel.a == 255);
            NUI_CHECK(std::abs(double(pixel.r) / 255.0 - reference) < 0.015);
        }
    }
}


void extreme_painter_coordinate_contract() {
    constexpr float kPainterScale = 1.0e-20f;
    constexpr float kLogicalWidth = 2.0e21f;

    for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                      ui::NoiseType::Simplex}) {
        for (auto mode : {ui::FractalNoiseMode::FBm,
                          ui::FractalNoiseMode::Turbulence,
                          ui::FractalNoiseMode::Ridged}) {
            const auto source = ui::NoiseSource::create_fractal(
                base, {.feature_size = 1.0f, .seed = 0x12345678u},
                fractal_options(mode, 6, 4.0f, 1.0f));
            NUI_CHECK(source.ok());
            const auto brush = source.noise.as_brush();

            ui::UI tree{ui::Canvas{16.0f, 16.0f,
                [brush](ui::CanvasContext2D& g) {
                    g.save();
                    g.scale(kPainterScale, 1.0f);
                    g.fill_rect({0.0f, 0.0f, kLogicalWidth, 16.0f}, brush);
                    g.restore();
                }}};
            ui::HeadlessRenderer renderer{{16, 16}, 1.0f};
            NUI_CHECK(renderer.render(tree));
            const auto pixel = renderer.pixel(8, 8);
            NUI_CHECK(pixel.a == 255);
            NUI_CHECK(pixel.r == pixel.g && pixel.r == pixel.b);

            const int expected =
                mode == ui::FractalNoiseMode::FBm ? 128 :
                mode == ui::FractalNoiseMode::Turbulence ? 0 : 255;
            NUI_CHECK(std::abs(int(pixel.r) - expected) <= 1);
        }
    }
}

void lifetime_isolation_contract() {
    const auto a = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin,
        {.feature_size = 24.0f, .seed = 0x11111111u},
        fractal_options(ui::FractalNoiseMode::FBm, 4, 2.0f, 0.5f));
    const auto b = ui::NoiseSource::create_fractal(
        ui::NoiseType::Simplex,
        {.feature_size = 24.0f, .seed = 0x22222222u},
        fractal_options(ui::FractalNoiseMode::Ridged, 5, 2.25f, 0.4f));
    NUI_CHECK(a.ok() && b.ok());
    const auto brush_a = a.noise.as_brush();
    const auto brush_b = b.noise.as_brush();

    const auto make_tree = [](const ui::Brush& brush) {
        return ui::UI{ui::Canvas{32.0f, 32.0f,
            [brush](ui::CanvasContext2D& g) {
                g.fill_rect({0, 0, 32, 32}, brush);
            }}};
    };

    auto tree_b = make_tree(brush_b);
    ui::HeadlessRenderer renderer_b{{32, 32}, 1.0f};
    std::uint8_t expected_b = 0;
    {
        auto tree_a = make_tree(brush_a);
        ui::HeadlessRenderer renderer_a{{32, 32}, 1.0f};
        NUI_CHECK(renderer_a.render(tree_a));
        NUI_CHECK(renderer_b.render(tree_b));
        expected_b = renderer_b.pixel(12, 9).r;
    }
    NUI_CHECK(renderer_b.render(tree_b));
    NUI_CHECK(renderer_b.pixel(12, 9).r == expected_b);

    auto invalid = fractal_options(ui::FractalNoiseMode::FBm);
    invalid.set_octaves(0);
    const auto failed_a = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin,
        {.feature_size = 24.0f, .seed = 0x11111111u},
        invalid);
    NUI_CHECK(!failed_a.ok());
    NUI_CHECK(failed_a.error == ui::NoiseCreateError::InvalidArgument);
    NUI_CHECK(renderer_b.render(tree_b));
    NUI_CHECK(renderer_b.pixel(12, 9).r == expected_b);

    ui::Brush retained{ui::Color{0, 0, 0, 0}};
    std::uint8_t retained_expected = 0;
    {
        const auto temporary = ui::NoiseSource::create_fractal(
            ui::NoiseType::Value,
            {.feature_size = 18.0f, .seed = 0xabcdef01u},
            fractal_options(ui::FractalNoiseMode::Turbulence, 3, 2.0f, 0.6f));
        NUI_CHECK(temporary.ok());
        retained = temporary.noise.as_brush();
        retained_expected = render_pixel(retained, 12, 9).r;
    }
    NUI_CHECK(render_pixel(retained, 12, 9).r == retained_expected);
}

double measure_raster(const ui::Brush& brush) {
    ui::UI tree{ui::Canvas{256.0f, 256.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 256, 256}, brush);
        }}};
    ui::HeadlessRenderer renderer{{256, 256}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    std::array<double, 5> samples{};
    for (double& sample : samples) {
        const auto begin = std::chrono::steady_clock::now();
        NUI_CHECK(renderer.render(tree));
        sample = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
    }
    std::sort(samples.begin(), samples.end());
    return samples[2];
}

void benchmark() {
    const auto base = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto fractal = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin,
        {.feature_size = 48.0f, .seed = 0x12345678u},
        fractal_options(ui::FractalNoiseMode::FBm, 6, 2.0f, 0.5f));
    NUI_CHECK(base.ok() && fractal.ok());
    const double base_ms = measure_raster(base.noise.as_brush());
    const double fractal_ms = measure_raster(fractal.noise.as_brush());
    std::cout << "Fractal noise warm 256x256 raster median (5 runs): base_perlin="
              << base_ms << " ms, fractal6_perlin=" << fractal_ms
              << " ms, ratio=" << (fractal_ms / base_ms) << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        option_contract();
        validation_contract();
        recurrence_contract();
        renderer_float_recurrence_contract();
        same_seed_octave_contract();
        reference_contract();
        raster_contract();
        degeneracy_contract();
        six_octave_grid_contract();
        tiny_feature_size_contract();
        extreme_painter_coordinate_contract();
        lifetime_isolation_contract();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL fractal noise: " << e.what() << '\n';
        return 1;
    }
}
