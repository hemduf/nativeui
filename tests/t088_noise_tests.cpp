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
#include <chrono>
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

struct HashVector {
    std::uint32_t seed;
    std::int32_t x;
    std::int32_t y;
    std::uint32_t hash;
};

constexpr HashVector kHashVectors[]{
    {0u, 0, 0, 0x00000000u},
    {0x12345678u, 17, -9, 0x9342507bu},
    {0xffffffffu, -1, -1, 0xcf6c0c3au},
    {0x87654321u, std::numeric_limits<std::int32_t>::min(), 0,
     0x630d83f1u},
    {0x87654321u, std::numeric_limits<std::int32_t>::max(),
     std::numeric_limits<std::int32_t>::min(), 0xa02e9e13u},
};

std::array<float, 4> bytes(std::uint32_t v) {
    return {float(v & 255u), float((v >> 8) & 255u),
            float((v >> 16) & 255u), float((v >> 24) & 255u)};
}

void hash_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(R"(
        uniform float4 seed_bytes;
        uniform float4 x_bytes;
        uniform float4 y_bytes;
        half4 main(float2 p) {
            float4 h = hash2(seed_bytes, x_bytes, y_bytes);
            if (p.x < 1.0) return half4(h.x / 255.0, h.y / 255.0,
                                        h.z / 255.0, 1.0);
            return half4(h.w / 255.0, 0.0, 0.0, 1.0);
        }
    )");
    const auto result = ui::ShaderProgram::compile(source);
    NUI_CHECK(result.ok());
    for (const auto& v : kHashVectors) {
        NUI_CHECK(ui::detail::noise_hash2(v.seed, v.x, v.y) == v.hash);
        ui::ShaderInstance shader{result.program};
        NUI_CHECK(shader.set_float4("seed_bytes", bytes(v.seed)) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float4("x_bytes", bytes(std::uint32_t(v.x))) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float4("y_bytes", bytes(std::uint32_t(v.y))) ==
                  ui::ShaderSetResult::Ok);
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
        NUI_CHECK(surface);
        ui::Painter painter{*surface->getCanvas()};
        painter.fill_rounded_rect({0, 0, 2, 1}, 0.0f, ui::Brush{shader});
        std::array<std::uint8_t, 8> pixels{};
        NUI_CHECK(surface->readPixels(SkImageInfo::Make(
            2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
            pixels.data(), 8, 0, 0));
        NUI_CHECK(pixels[0] == std::uint8_t(v.hash));
        NUI_CHECK(pixels[1] == std::uint8_t(v.hash >> 8));
        NUI_CHECK(pixels[2] == std::uint8_t(v.hash >> 16));
        NUI_CHECK(pixels[4] == std::uint8_t(v.hash >> 24));
    }

    NUI_CHECK(ui::detail::noise_u24(0u) == 0.0f);
    NUI_CHECK(ui::detail::noise_u24(0x000000ffu) == 0.0f);
    NUI_CHECK(ui::detail::noise_u24(0x010000ffu) == 0.00390625f);
    NUI_CHECK(ui::detail::noise_u24(0xffffffffu) ==
              16777215.0f / 16777216.0f);
}

void reference_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const struct { double x, y, value; } vectors[]{
        {0.0, 0.0, 0.6905655264854431},
        {48.0, 0.0, 0.42995017766952515},
        {24.0, 24.0, 0.4033228009939194},
        {-12.0, 60.0, 0.302814389850937},
        {3.5, -7.25, 0.6733313807168521},
    };
    for (const auto& v : vectors) {
        NUI_CHECK(std::abs(ui::detail::value_noise_reference(
            seed, 48.0, v.x, v.y) - v.value) < 1e-12);
    }
    for (int y = -40; y <= 40; ++y) {
        for (int x = -40; x <= 40; ++x) {
            const double value = ui::detail::value_noise_reference(
                seed, 48.0, double(x) * 0.7, double(y) * 1.3);
            NUI_CHECK(std::isfinite(value) && value >= 0.0 && value <= 1.0);
        }
    }
    NUI_CHECK(ui::detail::value_noise_reference(
        seed, 1.0, 2147483647.0, 0.0) == 0.5);
    NUI_CHECK(ui::detail::value_noise_reference(
        seed, 1.0, std::numeric_limits<double>::infinity(), 0.0) == 0.5);
    NUI_CHECK(ui::detail::value_noise_reference(
        seed, 1.0, std::numeric_limits<double>::quiet_NaN(), 0.0) == 0.5);
    for (double boundary : {-48.0, 0.0, 48.0}) {
        const double at = ui::detail::value_noise_reference(
            seed, 48.0, boundary, -24.0);
        const double left = ui::detail::value_noise_reference(
            seed, 48.0, boundary - 0.0001, -24.0);
        const double right = ui::detail::value_noise_reference(
            seed, 48.0, boundary + 0.0001, -24.0);
        NUI_CHECK(std::abs(left - at) < 1e-7);
        NUI_CHECK(std::abs(right - at) < 1e-7);
    }
    NUI_CHECK(ui::detail::value_noise_reference(seed, 48.0, 0.0, 0.0) ==
              ui::detail::noise_u24(ui::detail::noise_hash2(seed, 0, 0)));
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
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(source.ok());
    const auto brush = source.noise.as_brush();
    for (const auto [x, y] : {std::array<int, 2>{0, 0}, {8, 8},
                              {24, 24}, {48, 16}, {63, 63}}) {
        const auto actual = render_pixel(brush, x, y);
        const double expected = ui::detail::value_noise_reference(
            seed, 48.0, double(x) + 0.5, double(y) + 0.5);
        NUI_CHECK(std::abs(double(actual.r) / 255.0 - expected) < 0.015);
        NUI_CHECK(actual.r == actual.g && actual.r == actual.b);
        NUI_CHECK(actual.a == 255);
    }
    NUI_CHECK(render_pixel(brush, 8, 8).r == render_pixel(brush, 8, 8).r);
    const auto same_seed = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(same_seed.ok());
    NUI_CHECK(render_pixel(same_seed.noise.as_brush(), 8, 8).r ==
              render_pixel(brush, 8, 8).r);
    const auto different = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x87654321u});
    NUI_CHECK(different.ok());
    NUI_CHECK(render_pixel(different.noise.as_brush(), 8, 8).r !=
              render_pixel(brush, 8, 8).r);

    const auto scaled = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 24.0f, .seed = seed});
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
        ui::NoiseType::Value, {.feature_size = 16.0f, .seed = 0x98765432u});
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
            ui::NoiseType::Value, {.feature_size = 16.0f, .seed = 0x98765432u});
        NUI_CHECK(temporary.ok());
        return temporary.noise.as_brush();
    }();
    NUI_CHECK(render_pixel(retained_brush, 12, 9).r == first_pixel.r);
}

void benchmark() {
    const auto source = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x12345678u});
    NUI_CHECK(source.ok());
    const auto measure = [](const ui::Brush& brush) {
        ui::UI tree{ui::Canvas{256, 256,
            [brush](ui::CanvasContext2D& g) {
                g.fill_rect({0, 0, 256, 256}, brush);
            }}};
        ui::HeadlessRenderer renderer{{256, 256}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        std::array<double, 5> elapsed{};
        for (double& sample : elapsed) {
            const auto begin = std::chrono::steady_clock::now();
            NUI_CHECK(renderer.render(tree));
            sample = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - begin).count();
        }
        std::sort(elapsed.begin(), elapsed.end());
        return elapsed[2];
    };
    const auto solid_ms = measure(ui::Brush{ui::Color{0.5f, 0.5f, 0.5f, 1.0f}});
    const auto noise_ms = measure(source.noise.as_brush());
    std::cout << "T088 warm 256x256 raster median (5 runs): solid="
              << solid_ms << " ms, noise=" << noise_ms << " ms\n";
}

void shader_guard_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kValueNoiseKernelSkSL);
    source.append(R"(
        uniform float2 test_point;
        half4 main(float2 p) {
            float v = value_noise(test_point);
            return half4(v, v, v, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    for (float x : {2147483648.0f, -4294967296.0f,
                    std::numeric_limits<float>::max()}) {
        ui::ShaderInstance shader{compiled.program};
        NUI_CHECK(shader.set_float("feature_size", 1.0f) == ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float4("seed_bytes", bytes(0x12345678u)) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float2("test_point", {x, 0.0f}) ==
                  ui::ShaderSetResult::Ok);
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
        NUI_CHECK(surface);
        ui::Painter painter{*surface->getCanvas()};
        painter.fill_rounded_rect({0, 0, 1, 1}, 0.0f, ui::Brush{shader});
        SkPixmap pixels;
        NUI_CHECK(surface->peekPixels(&pixels));
        NUI_CHECK(pixels.getColor4f(0, 0).fR == 0.5f);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        NUI_CHECK(!ui::NoiseCreateResult{}.ok());
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        hash_contract();
        reference_contract();
        raster_contract();
        independent_views();
        shader_guard_contract();
        ui::NoiseSource inert;
        NUI_CHECK(!ui::detail::ShaderBrushAccess::is_shader(inert.as_brush()));

        const auto valid = ui::NoiseSource::create(
            ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x12345678u});
        NUI_CHECK(valid.ok());
        NUI_CHECK(valid.error == ui::NoiseCreateError::None);
        NUI_CHECK(valid.diagnostic.empty());
        NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(valid.noise.as_brush()));

        for (float size : {0.0f, -1.0f,
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
            const auto invalid = ui::NoiseSource::create(
                ui::NoiseType::Value, {.feature_size = size});
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
        std::cerr << "FAIL T088 noise: " << e.what() << '\n';
        return 1;
    }
}
