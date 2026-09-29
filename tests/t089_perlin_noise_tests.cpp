#include <nativeui/noise.hpp>

#include "test_support.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/noise_math.hpp"
#include "src/detail/noise_sksl.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

static_assert(std::is_nothrow_default_constructible_v<ui::NoiseSource>);
static_assert(std::is_copy_constructible_v<ui::NoiseSource>);
static_assert(std::is_nothrow_destructible_v<ui::NoiseSource>);

// Frozen T089 gradient table. S is the mathematical 1/sqrt(2) written with the
// exact decimal literal shared by the C++ reference and the SkSL kernel.
constexpr double kS = 0.707106781186547524400844362104849;
constexpr double kGradientTable[8][2]{
    {1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0},
    {kS, kS}, {-kS, kS}, {kS, -kS}, {-kS, -kS},
};

struct HashVector {
    std::uint32_t seed;
    std::int32_t x;
    std::int32_t y;
    std::uint32_t hash;
    std::uint32_t gradient_index;
};

// The T088 frozen hash vectors, reused bit-exactly, plus their T089
// gradient indices (hash & 7).
constexpr HashVector kHashVectors[]{
    {0u, 0, 0, 0x00000000u, 0u},
    {0x12345678u, 17, -9, 0x9342507bu, 3u},
    {0xffffffffu, -1, -1, 0xcf6c0c3au, 2u},
    {0x87654321u, std::numeric_limits<std::int32_t>::min(), 0,
     0x630d83f1u, 1u},
    {0x87654321u, std::numeric_limits<std::int32_t>::max(),
     std::numeric_limits<std::int32_t>::min(), 0xa02e9e13u, 3u},
};

std::array<float, 4> bytes(std::uint32_t v) {
    return {float(v & 255u), float((v >> 8) & 255u),
            float((v >> 16) & 255u), float((v >> 24) & 255u)};
}

void hash_and_gradient_contract() {
    for (const auto& v : kHashVectors) {
        NUI_CHECK(ui::detail::noise_hash2(v.seed, v.x, v.y) == v.hash);
        NUI_CHECK((ui::detail::noise_hash2(v.seed, v.x, v.y) & 7u) ==
                  v.gradient_index);
        // hash & 7 selects exactly the frozen gradient vector.
        const auto g = ui::detail::noise_gradient(
            ui::detail::noise_hash2(v.seed, v.x, v.y) & 7u);
        NUI_CHECK(g.x == kGradientTable[v.gradient_index][0]);
        NUI_CHECK(g.y == kGradientTable[v.gradient_index][1]);
    }
    // All eight gradient indices map to the exact frozen vectors.
    for (std::uint32_t i = 0; i < 8; ++i) {
        const auto g = ui::detail::noise_gradient(i);
        NUI_CHECK(g.x == kGradientTable[i][0]);
        NUI_CHECK(g.y == kGradientTable[i][1]);
        // The index is the low three bits, exactly like hash & 7.
        const auto masked = ui::detail::noise_gradient(i + 8u * 5u);
        NUI_CHECK(masked.x == g.x && masked.y == g.y);
    }
}

void sksl_gradient_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kPerlinNoiseKernelSkSL);
    source.append(R"(
        uniform float4 hash_bytes;
        half4 main(float2 p) {
            float2 g = gradient_of(hash_bytes);
            return half4((g.x + 1.0) * 0.5, (g.y + 1.0) * 0.5, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    // Output readback tolerance: the frozen gradient components are exact
    // small rationals; half/float shader output rounding needs < 1e-3 slack.
    constexpr float kReadbackTolerance = 1e-3f;
    const auto read = [&compiled](std::array<float, 4> hash_bytes) {
        ui::ShaderInstance shader{compiled.program};
        NUI_CHECK(shader.set_float4("hash_bytes", hash_bytes) ==
                  ui::ShaderSetResult::Ok);
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
        NUI_CHECK(surface);
        ui::Painter painter{*surface->getCanvas()};
        painter.fill_rounded_rect({0, 0, 1, 1}, 0.0f, ui::Brush{shader});
        SkPixmap pixels;
        NUI_CHECK(surface->peekPixels(&pixels));
        return pixels.getColor4f(0, 0);
    };
    for (int i = 0; i < 8; ++i) {
        const auto color = read({float(i), 0.0f, 0.0f, 0.0f});
        const float expected_x = float((kGradientTable[i][0] + 1.0) * 0.5);
        const float expected_y = float((kGradientTable[i][1] + 1.0) * 0.5);
        NUI_CHECK(std::abs(color.fR - expected_x) <= kReadbackTolerance);
        NUI_CHECK(std::abs(color.fG - expected_y) <= kReadbackTolerance);
    }
    // mod(h.x, 8.0): the low hash byte above 7 wraps to the same vector.
    for (int i = 0; i < 8; ++i) {
        const auto wrapped = read({float(i + 8), 0.0f, 0.0f, 0.0f});
        const auto direct = read({float(i), 0.0f, 0.0f, 0.0f});
        NUI_CHECK(wrapped.fR == direct.fR && wrapped.fG == direct.fG);
    }
}

void reference_contract() {
    // CPU double reference fixed values from the frozen T089 kernel
    // (independent reference, tolerance 1e-12).
    const struct { std::uint32_t seed; double feature_size, x, y, value; }
    vectors[]{
        {0x12345678u, 48.0,  12.0,   0.0, 0.47255127485140114},
        {0x12345678u, 48.0,  24.0,  24.0, 0.3933058261758408},
        {0x12345678u, 48.0,  36.0,  36.0, 0.3695710383326333},
        {0x12345678u, 48.0, -12.0,  60.0, 0.5608703578064187},
        {0x12345678u, 48.0,   3.5,  -7.25, 0.44642349535912795},
        {0x31415926u, 72.0,  18.0,  54.0, 0.3593137931834338},
        {0x31415926u, 72.0, -36.0, -36.0, 0.6066941738241592},
        {0x31415926u, 72.0, 100.0, -200.0, 0.39221672637310906},
        {0x00000000u,  1.0,   2.5,  -3.25, 0.5272349479686098},
    };
    for (const auto& v : vectors) {
        NUI_CHECK(std::abs(ui::detail::perlin_noise_reference(
            v.seed, v.feature_size, v.x, v.y) - v.value) < 1e-12);
    }

    // Integer lattice points are exactly 0.5 in double.
    NUI_CHECK(ui::detail::perlin_noise_reference(
        0x12345678u, 48.0, 0.0, 0.0) == 0.5);
    NUI_CHECK(ui::detail::perlin_noise_reference(
        0x12345678u, 48.0, 48.0, -96.0) == 0.5);
    NUI_CHECK(ui::detail::perlin_noise_reference(
        0x31415926u, 72.0, -72.0, 144.0) == 0.5);
    NUI_CHECK(ui::detail::perlin_noise_reference(
        0u, 1.0, 5.0, -3.0) == 0.5);

    // Deterministic grid: output range, no NaN, exact repeat.
    const std::uint32_t seeds[]{0x12345678u, 0x31415926u, 0u, 0xffffffffu};
    for (std::uint32_t seed : seeds) {
        std::vector<double> first;
        first.reserve(41 * 41);
        for (int j = -20; j <= 20; ++j) {
            for (int i = -20; i <= 20; ++i) {
                const double value = ui::detail::perlin_noise_reference(
                    seed, 48.0, double(i) * 6.5 - 7.25, double(j) * 6.5 + 3.75);
                NUI_CHECK(std::isfinite(value) && value >= 0.0 && value <= 1.0);
                first.push_back(value);
            }
        }
        std::size_t index = 0;
        for (int j = -20; j <= 20; ++j) {
            for (int i = -20; i <= 20; ++i) {
                const double value = ui::detail::perlin_noise_reference(
                    seed, 48.0, double(i) * 6.5 - 7.25, double(j) * 6.5 + 3.75);
                NUI_CHECK(value == first[index++]);
            }
        }
    }
}

void boundary_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    constexpr double feature_size = 48.0;
    constexpr double eps = 1e-4;
    // Documented tolerances: value continuity 1e-5 (a 1e-4 probe moves the
    // field by less than 6e-6 for feature_size 48) and one-sided first
    // derivative agreement 1e-3.
    constexpr double kValueTolerance = 1e-5;
    constexpr double kDerivativeTolerance = 1e-3;
    for (double boundary : {-48.0, 0.0, 48.0}) {
        // X boundary at (boundary, -24).
        const double at_x = ui::detail::perlin_noise_reference(
            seed, feature_size, boundary, -24.0);
        const double left_x = ui::detail::perlin_noise_reference(
            seed, feature_size, boundary - eps, -24.0);
        const double right_x = ui::detail::perlin_noise_reference(
            seed, feature_size, boundary + eps, -24.0);
        NUI_CHECK(std::abs(left_x - at_x) < kValueTolerance);
        NUI_CHECK(std::abs(right_x - at_x) < kValueTolerance);
        const double d_left_x = (at_x - left_x) / eps;
        const double d_right_x = (right_x - at_x) / eps;
        NUI_CHECK(std::abs(d_left_x - d_right_x) <= kDerivativeTolerance);

        // Y boundary at (24, boundary).
        const double at_y = ui::detail::perlin_noise_reference(
            seed, feature_size, 24.0, boundary);
        const double left_y = ui::detail::perlin_noise_reference(
            seed, feature_size, 24.0, boundary - eps);
        const double right_y = ui::detail::perlin_noise_reference(
            seed, feature_size, 24.0, boundary + eps);
        NUI_CHECK(std::abs(left_y - at_y) < kValueTolerance);
        NUI_CHECK(std::abs(right_y - at_y) < kValueTolerance);
        const double d_left_y = (at_y - left_y) / eps;
        const double d_right_y = (right_y - at_y) / eps;
        NUI_CHECK(std::abs(d_left_y - d_right_y) <= kDerivativeTolerance);
    }

    // Frozen reference sanity: at (0,-24) both one-sided X derivatives are
    // approximately -0.003682847818931023.
    const double at = ui::detail::perlin_noise_reference(
        seed, feature_size, 0.0, -24.0);
    const double left = ui::detail::perlin_noise_reference(
        seed, feature_size, -eps, -24.0);
    const double right = ui::detail::perlin_noise_reference(
        seed, feature_size, eps, -24.0);
    NUI_CHECK(std::abs((at - left) / eps - (-0.003682847818931023)) < 1e-9);
    NUI_CHECK(std::abs((right - at) / eps - (-0.003682847818931023)) < 1e-9);
}

void guard_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double out_of_range[]{
        2147483647.0,
        2147483648.0,
        -4294967296.0,
        std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        double(std::numeric_limits<float>::max()),
        -double(std::numeric_limits<float>::max()),
        inf,
        -inf,
        nan,
    };
    for (double coordinate : out_of_range) {
        NUI_CHECK(ui::detail::perlin_noise_reference(
            seed, 1.0, coordinate, 0.0) == 0.5);
        NUI_CHECK(ui::detail::perlin_noise_reference(
            seed, 1.0, 0.0, coordinate) == 0.5);
    }
    // The last representable lattice neighborhood stays valid and finite.
    const double near = ui::detail::perlin_noise_reference(
        seed, 1.0, 2147483646.5, -2147483648.0);
    NUI_CHECK(std::isfinite(near) && near >= 0.0 && near <= 1.0);
}

void value_vs_perlin_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    // The kernels are not aliased: at an integer lattice point Perlin is
    // exactly 0.5 while Value is its hash-derived scalar.
    NUI_CHECK(ui::detail::perlin_noise_reference(
        seed, 48.0, 48.0, 0.0) == 0.5);
    NUI_CHECK(std::abs(ui::detail::value_noise_reference(
        seed, 48.0, 48.0, 0.0) - 0.42995017766952515) < 1e-12);
    const double perlin = ui::detail::perlin_noise_reference(
        seed, 48.0, 12.0, 0.0);
    const double value = ui::detail::value_noise_reference(
        seed, 48.0, 12.0, 0.0);
    NUI_CHECK(std::abs(perlin - 0.47255127485140114) < 1e-12);
    NUI_CHECK(value > 0.66 && value < 0.67);
    NUI_CHECK(std::abs(perlin - value) > 0.1);
}

void sksl_lattice_guard_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kPerlinNoiseKernelSkSL);
    source.append(R"(
        uniform float2 test_point;
        half4 main(float2 p) {
            float v = perlin_noise(test_point);
            return half4(v, v, v, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    const auto read = [&compiled](float feature_size, float x, float y) {
        ui::ShaderInstance shader{compiled.program};
        NUI_CHECK(shader.set_float("feature_size", feature_size) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float4("seed_bytes", bytes(0x12345678u)) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float2("test_point", {x, y}) ==
                  ui::ShaderSetResult::Ok);
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
        NUI_CHECK(surface);
        ui::Painter painter{*surface->getCanvas()};
        painter.fill_rounded_rect({0, 0, 1, 1}, 0.0f, ui::Brush{shader});
        SkPixmap pixels;
        NUI_CHECK(surface->peekPixels(&pixels));
        return pixels.getColor4f(0, 0).fR;
    };
    // Integer lattice points are bit-exactly 0.5 in float.
    NUI_CHECK(read(48.0f, 0.0f, 0.0f) == 0.5f);
    NUI_CHECK(read(48.0f, 48.0f, -96.0f) == 0.5f);
    NUI_CHECK(read(72.0f, -72.0f, 144.0f) == 0.5f);
    NUI_CHECK(read(1.0f, 5.0f, -3.0f) == 0.5f);
    // Out-of-range lattice guard returns exactly 0.5 without undefined
    // float-to-int conversion.
    for (float x : {2147483648.0f, -4294967296.0f,
                    std::numeric_limits<float>::max()}) {
        NUI_CHECK(read(1.0f, x, 0.0f) == 0.5f);
    }
}

ui::Rgba8 render_pixel(const ui::Brush& brush, int x, int y,
                       float translate_x = 0.0f) {
    ui::UI tree{ui::Canvas{64.0f, 64.0f,
        [brush, translate_x](ui::CanvasContext2D& g) {
            if (translate_x == 0.0f) {
                g.fill_rect({0, 0, 64, 64}, brush);
            } else {
                g.save();
                g.translate(translate_x, 0.0f);
                g.fill_rect({0, 0, 32, 64}, brush);
                g.restore();
            }
        }}};
    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    return renderer.pixel(x, y);
}

void raster_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const auto source = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(source.ok());
    const auto brush = source.noise.as_brush();
    // Raster pixels match the CPU double oracle within 0.015 (T088
    // precedent); the GPU test records the same single tolerance pair.
    for (const auto [x, y] : {std::array<int, 2>{0, 0}, {8, 8},
                              {24, 24}, {48, 16}, {63, 63}}) {
        const auto actual = render_pixel(brush, x, y);
        const double expected = ui::detail::perlin_noise_reference(
            seed, 48.0, double(x) + 0.5, double(y) + 0.5);
        NUI_CHECK(std::abs(double(actual.r) / 255.0 - expected) < 0.015);
        NUI_CHECK(actual.r == actual.g && actual.r == actual.b);
        NUI_CHECK(actual.a == 255);
    }
    NUI_CHECK(render_pixel(brush, 8, 8).r == render_pixel(brush, 8, 8).r);
    const auto same_seed = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(same_seed.ok());
    NUI_CHECK(render_pixel(same_seed.noise.as_brush(), 8, 8).r ==
              render_pixel(brush, 8, 8).r);
    const auto different = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = 0x87654321u});
    NUI_CHECK(different.ok());
    NUI_CHECK(render_pixel(different.noise.as_brush(), 8, 8).r !=
              render_pixel(brush, 8, 8).r);

    const auto scaled = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 24.0f, .seed = seed});
    NUI_CHECK(scaled.ok());
    NUI_CHECK(std::abs(int(render_pixel(brush, 16, 16).r) -
                       int(render_pixel(scaled.noise.as_brush(), 8, 8).r)) < 3);
    NUI_CHECK(std::abs(int(render_pixel(brush, 8, 8).r) -
                       int(render_pixel(brush, 40, 8, 32.0f).r)) < 3);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto canonical = source.noise.as_brush(
        {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    const auto invalid_color = source.noise.as_brush(
        {nan, -1.0f, 2.0f, std::numeric_limits<float>::infinity()},
        {1.0f, 1.0f, 1.0f, 1.0f});
    const auto a = render_pixel(canonical, 8, 8);
    const auto b = render_pixel(invalid_color, 8, 8);
    NUI_CHECK(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);

    const auto translucent = source.noise.as_brush(
        {1, 0, 0, 0.5f}, {1, 0, 0, 0.5f});
    auto surface = SkSurfaces::Raster(SkImageInfo::Make(
        1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
    NUI_CHECK(surface);
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    ui::Painter painter{*surface->getCanvas()};
    painter.fill_rounded_rect({0, 0, 1, 1}, 0.0f, translucent);
    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    const auto* p = static_cast<const float*>(pixels.addr(0, 0));
    NUI_CHECK(std::abs(p[3] - 0.5f) < 0.001f);
    NUI_CHECK(std::abs(p[0] - 0.5f) < 0.001f);
    NUI_CHECK(p[1] == 0.0f && p[2] == 0.0f);
}

void independent_views() {
    const auto source = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 16.0f, .seed = 0x98765432u});
    NUI_CHECK(source.ok());
    const auto brush = source.noise.as_brush();
    const auto make_tree = [brush] {
        return ui::UI{ui::Canvas{32, 32,
            [brush](ui::CanvasContext2D& g) {
                g.fill_rect({0, 0, 32, 32}, brush);
            }}};
    };
    auto second_tree = make_tree();
    ui::HeadlessRenderer second{{32, 32}, 1.0f};
    ui::Rgba8 first_pixel;
    {
        auto first_tree = make_tree();
        ui::HeadlessRenderer first{{32, 32}, 1.0f};
        NUI_CHECK(first.render(first_tree));
        first_pixel = first.pixel(12, 9);
        NUI_CHECK(second.render(second_tree));
        NUI_CHECK(second.pixel(12, 9).r == first_pixel.r);
    }
    NUI_CHECK(second.render(second_tree));
    NUI_CHECK(second.pixel(12, 9).r == first_pixel.r);

    const auto retained_brush = [] {
        const auto temporary = ui::NoiseSource::create(
            ui::NoiseType::Perlin, {.feature_size = 16.0f, .seed = 0x98765432u});
        NUI_CHECK(temporary.ok());
        return temporary.noise.as_brush();
    }();
    NUI_CHECK(render_pixel(retained_brush, 12, 9).r == first_pixel.r);
}

} // namespace

int main() {
    try {
        NUI_CHECK(!ui::NoiseCreateResult{}.ok());
        hash_and_gradient_contract();
        sksl_gradient_contract();
        reference_contract();
        boundary_contract();
        guard_contract();
        value_vs_perlin_contract();
        sksl_lattice_guard_contract();
        raster_contract();
        independent_views();
        ui::NoiseSource inert;
        NUI_CHECK(!ui::detail::ShaderBrushAccess::is_shader(inert.as_brush()));

        const auto valid = ui::NoiseSource::create(
            ui::NoiseType::Perlin, {.feature_size = 72.0f, .seed = 0x31415926u});
        NUI_CHECK(valid.ok());
        NUI_CHECK(valid.error == ui::NoiseCreateError::None);
        NUI_CHECK(valid.diagnostic.empty());
        NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(valid.noise.as_brush()));

        for (float size : {0.0f, -1.0f,
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
            const auto invalid = ui::NoiseSource::create(
                ui::NoiseType::Perlin, {.feature_size = size});
            NUI_CHECK(!invalid.ok());
            NUI_CHECK(invalid.error == ui::NoiseCreateError::InvalidArgument);
            NUI_CHECK(ui::detail::ShaderBrushAccess::is_transparent_solid(
                invalid.noise.as_brush()));
        }

        const auto invalid_type = ui::NoiseSource::create(
            static_cast<ui::NoiseType>(999));
        NUI_CHECK(!invalid_type.ok());
        NUI_CHECK(invalid_type.error == ui::NoiseCreateError::InvalidArgument);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T089 perlin noise: " << e.what() << '\n';
        return 1;
    }
}
