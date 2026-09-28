#include <nativeui/noise.hpp>

#include "test_support.hpp"
#include "src/detail/noise_math.hpp"
#include "src/detail/noise_sksl.hpp"
#include "src/detail/shader_brush_access.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <array>
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

std::array<float, 4> bytes(std::uint32_t v) {
    return {float(v & 255u), float((v >> 8) & 255u),
            float((v >> 16) & 255u), float((v >> 24) & 255u)};
}

void salted_hash_contract() {
    constexpr struct {
        std::uint32_t seed;
        std::int32_t x;
        std::int32_t y;
        std::uint32_t hx;
        std::uint32_t hy;
    } vectors[]{
        {0x12345678u, 0, 0, 0x41698e9eu, 0x2f7a54afu},
        {0x12345678u, 17, -9, 0x5f689fd5u, 0xc5ed4d4du},
        {0xffffffffu, -1, -1, 0x0d59d12cu, 0x483aa1abu},
        {0x87654321u, -2147483647, 2147483646,
         0x79d2bf5eu, 0x1efc3d9eu},
        {0x27182818u, 2147483646, -2147483647,
         0xf5431ff0u, 0xca184d5eu},
    };
    for (const auto& v : vectors) {
        const auto hx = ui::detail::noise_hash2(
            v.seed ^ ui::detail::kWorleyXSeedSalt, v.x, v.y);
        const auto hy = ui::detail::noise_hash2(
            v.seed ^ ui::detail::kWorleyYSeedSalt, v.x, v.y);
        NUI_CHECK(hx == v.hx);
        NUI_CHECK(hy == v.hy);
        NUI_CHECK(ui::detail::worley_u24_reference(hx) ==
                  double(hx >> 8) / 16777216.0);
        NUI_CHECK(ui::detail::worley_u24_reference(hy) ==
                  double(hy >> 8) / 16777216.0);
    }
}

void row_major_candidate_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    constexpr struct {
        int dx, dy;
        std::uint32_t hx, hy;
        double ux, uy;
    } expected[]{
        {-1, -1, 0x55e3810au, 0x2ce5f5b6u,
         0.33550268411636353, 0.17538386583328247},
        { 0, -1, 0x1be8f3e2u, 0xe9b1abfdu,
         0.10902327299118042, 0.91286724805831909},
        { 1, -1, 0x4bfc06d2u, 0x3e4338e0u,
         0.29681432247161865, 0.24321317672729492},
        {-1,  0, 0xe8919253u, 0x82adb252u,
         0.90847122669219971, 0.51046288013458252},
        { 0,  0, 0xdb8af549u, 0x223087aau,
         0.85758906602859497, 0.13355296850204468},
        { 1,  0, 0x5c3b5636u, 0x50e21f36u,
         0.36028039455413818, 0.31595033407211304},
        {-1,  1, 0x6904f081u, 0xd6ded8b3u,
         0.41023159027099609, 0.83933782577514648},
        { 0,  1, 0xb01c69b2u, 0xda8ac4b4u,
         0.68793350458145142, 0.85367989540100098},
        { 1,  1, 0xaab3233cu, 0x6c3c6ac0u,
         0.66679590940475464, 0.42279684543609619},
    };
    std::size_t index = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            NUI_CHECK(expected[index].dx == dx && expected[index].dy == dy);
            const auto cx = std::int32_t(3 + dx);
            const auto cy = std::int32_t(-2 + dy);
            const auto hx = ui::detail::noise_hash2(
                seed ^ ui::detail::kWorleyXSeedSalt, cx, cy);
            const auto hy = ui::detail::noise_hash2(
                seed ^ ui::detail::kWorleyYSeedSalt, cx, cy);
            NUI_CHECK(hx == expected[index].hx);
            NUI_CHECK(hy == expected[index].hy);
            NUI_CHECK(ui::detail::worley_u24_reference(hx) ==
                      expected[index].ux);
            NUI_CHECK(ui::detail::worley_u24_reference(hy) ==
                      expected[index].uy);
            ++index;
        }
    }
    NUI_CHECK(index == 9);
}

void tie_insertion_contract() {
    double best1 = 8.0;
    double best2 = 8.0;
    ui::detail::worley_update_best(1.0, best1, best2);
    NUI_CHECK(best1 == 1.0 && best2 == 8.0);
    ui::detail::worley_update_best(1.0, best1, best2);
    NUI_CHECK(best1 == 1.0 && best2 == 1.0);
    ui::detail::worley_update_best(0.25, best1, best2);
    NUI_CHECK(best1 == 0.25 && best2 == 1.0);
    ui::detail::worley_update_best(0.25, best1, best2);
    NUI_CHECK(best1 == 0.25 && best2 == 0.25);
}

void reference_contract() {
    constexpr struct {
        std::uint32_t seed;
        double feature_size, x, y, f1, f2;
    } vectors[]{
        {0x12345678u, 48.0, 0.0, 0.0,
         0.10867726857473196, 0.11162684099715002},
        {0x12345678u, 48.0, 12.0, 0.0,
         0.0655991327043123, 0.15353653736572662},
        {0x12345678u, 48.0, 24.0, 24.0,
         0.14084885395394478, 0.21120836134567353},
        {0x12345678u, 48.0, -12.0, 60.0,
         0.2780352332642745, 0.3378491663173757},
        {0x31415926u, 72.0, 18.0, 54.0,
         0.07742700865993303, 0.14017922152807255},
        {0x31415926u, 72.0, -36.0, 18.0,
         0.13833782582626178, 0.30090421908910964},
        {0x27182818u, 52.0, 13.0, 7.0,
         0.20702102176765394, 0.22644232423775762},
        {0xffff00ffu, 33.0, -16.25, -8.75,
         0.06679267981473003, 0.300881477318098},
    };
    for (const auto& v : vectors) {
        const auto actual = ui::detail::worley_noise_reference(
            v.seed, v.feature_size, v.x, v.y);
        NUI_CHECK(std::abs(actual.f1 - v.f1) < 1e-12);
        NUI_CHECK(std::abs(actual.f2 - v.f2) < 1e-12);
        NUI_CHECK(actual.f1 <= actual.f2);
    }

    for (std::uint32_t seed : {0u, 0x12345678u, 0x31415926u, 0xffffffffu}) {
        for (int y = -20; y <= 20; ++y) {
            for (int x = -20; x <= 20; ++x) {
                const auto a = ui::detail::worley_noise_reference(
                    seed, 37.0, double(x) * 5.25 + 0.125,
                    double(y) * 4.75 - 0.375);
                const auto b = ui::detail::worley_noise_reference(
                    seed, 37.0, double(x) * 5.25 + 0.125,
                    double(y) * 4.75 - 0.375);
                NUI_CHECK(std::isfinite(a.f1) && std::isfinite(a.f2));
                NUI_CHECK(a.f1 >= 0.0 && a.f1 <= 1.0);
                NUI_CHECK(a.f2 >= 0.0 && a.f2 <= 1.0);
                NUI_CHECK(a.f1 <= a.f2);
                NUI_CHECK(a.f1 == b.f1 && a.f2 == b.f2);
            }
        }
    }
}

void local_distance_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const std::int32_t cx = 12;
    const std::int32_t cy = -8;
    const double ux = ui::detail::worley_u24_reference(
        ui::detail::noise_hash2(seed ^ ui::detail::kWorleyXSeedSalt, cx, cy));
    const double uy = ui::detail::worley_u24_reference(
        ui::detail::noise_hash2(seed ^ ui::detail::kWorleyYSeedSalt, cx, cy));
    const double fx = 0.625;
    const double fy = 0.375;
    const double local = (ux - fx) * (ux - fx) + (uy - fy) * (uy - fy);
    const double absolute =
        (double(cx) + ux - (double(cx) + fx)) *
            (double(cx) + ux - (double(cx) + fx)) +
        (double(cy) + uy - (double(cy) + fy)) *
            (double(cy) + uy - (double(cy) + fy));
    NUI_CHECK(std::abs(local - absolute) < 1e-15);

    // Float absolute positions lose the fractional feature offset near 2e9;
    // the normative local form does not.
    constexpr std::int32_t large = 2000000000;
    const double large_ux = ui::detail::worley_u24_reference(
        ui::detail::noise_hash2(seed ^ ui::detail::kWorleyXSeedSalt,
                                large, large));
    const double local_dx = large_ux - 0.8125;
    const float absolute_feature = float(large) + float(large_ux);
    const float absolute_point = float(large) + 0.8125f;
    NUI_CHECK(std::abs(double(absolute_feature - absolute_point) - local_dx) >
              1e-3);
}

ui::detail::WorleyNoiseReference expanded_reference(std::uint32_t seed,
                                                     double x,
                                                     double y) {
    const auto ix = std::int32_t(std::floor(x));
    const auto iy = std::int32_t(std::floor(y));
    const double fx = x - std::floor(x);
    const double fy = y - std::floor(y);
    double best1 = 32.0;
    double best2 = 32.0;
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            const auto cx = std::int32_t(ix + dx);
            const auto cy = std::int32_t(iy + dy);
            const double ux = ui::detail::worley_u24_reference(
                ui::detail::noise_hash2(
                    seed ^ ui::detail::kWorleyXSeedSalt, cx, cy));
            const double uy = ui::detail::worley_u24_reference(
                ui::detail::noise_hash2(
                    seed ^ ui::detail::kWorleyYSeedSalt, cx, cy));
            const double rx = double(dx) + ux - fx;
            const double ry = double(dy) + uy - fy;
            ui::detail::worley_update_best(rx * rx + ry * ry, best1, best2);
        }
    }
    return {std::sqrt(best1) / ui::detail::kWorleySqrt8,
            std::sqrt(best2) / ui::detail::kWorleySqrt8};
}

void bounded_neighborhood_contract() {
    constexpr std::uint32_t seed = 0x30ed1cc0u;
    constexpr double x = 0.9771740270133293;
    constexpr double y = 0.8961531118928558;
    const auto bounded = ui::detail::worley_noise_reference(seed, 1.0, x, y);
    const auto expanded = expanded_reference(seed, x, y);
    NUI_CHECK(std::abs(bounded.f1 - 0.14354811216823427) < 1e-12);
    NUI_CHECK(std::abs(bounded.f2 - 0.36721593702167177) < 1e-12);
    NUI_CHECK(std::abs(expanded.f2 - 0.36395375509439604) < 1e-12);
    NUI_CHECK(bounded.f2 > expanded.f2);
}

void guard_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double coordinate : {2147483647.0, 2147483648.0,
                              -2147483648.0, -2147483649.0,
                              std::numeric_limits<double>::max(),
                              -std::numeric_limits<double>::max(),
                              inf, -inf, nan}) {
        const auto x = ui::detail::worley_noise_reference(
            seed, 1.0, coordinate, 0.0);
        const auto y = ui::detail::worley_noise_reference(
            seed, 1.0, 0.0, coordinate);
        NUI_CHECK(x.f1 == 0.5 && x.f2 == 0.5);
        NUI_CHECK(y.f1 == 0.5 && y.f2 == 0.5);
    }
    const auto low = ui::detail::worley_noise_reference(
        seed, 1.0, -2147483647.0, 0.0);
    const auto high = ui::detail::worley_noise_reference(
        seed, 1.0, 2147483646.0, 0.0);
    NUI_CHECK(std::isfinite(low.f1) && std::isfinite(low.f2));
    NUI_CHECK(std::isfinite(high.f1) && std::isfinite(high.f2));
}

void sksl_salted_hash_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(R"(
        uniform float4 seed_bytes;
        uniform float4 x_bytes;
        uniform float4 y_bytes;
        uniform float use_y_salt;
        half4 main(float2 p) {
            float4 salt = use_y_salt > 0.5
                ? float4(149.0, 53.0, 216.0, 99.0)
                : float4(179.0, 233.0, 17.0, 165.0);
            float4 h = hash2(xor32(seed_bytes, salt), x_bytes, y_bytes);
            if (p.x < 1.0) {
                return half4(h.x / 255.0, h.y / 255.0,
                             h.z / 255.0, 1.0);
            }
            return half4(h.w / 255.0, 0.0, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());

    constexpr struct {
        std::uint32_t seed;
        std::int32_t x, y;
    } vectors[]{
        {0x12345678u, 3, -2},
        {0xffffffffu, -1, -1},
        {0x87654321u, -2147483520, 2147483520},
    };
    for (const auto& v : vectors) {
        for (bool y_salt : {false, true}) {
            const std::uint32_t salt = y_salt
                ? ui::detail::kWorleyYSeedSalt
                : ui::detail::kWorleyXSeedSalt;
            const std::uint32_t expected =
                ui::detail::noise_hash2(v.seed ^ salt, v.x, v.y);

            ui::ShaderInstance shader{compiled.program};
            NUI_CHECK(shader.set_float4("seed_bytes", bytes(v.seed)) ==
                      ui::ShaderSetResult::Ok);
            NUI_CHECK(shader.set_float4("x_bytes",
                                        bytes(std::uint32_t(v.x))) ==
                      ui::ShaderSetResult::Ok);
            NUI_CHECK(shader.set_float4("y_bytes",
                                        bytes(std::uint32_t(v.y))) ==
                      ui::ShaderSetResult::Ok);
            NUI_CHECK(shader.set_float("use_y_salt", y_salt ? 1.0f : 0.0f) ==
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
            NUI_CHECK(pixels[0] == std::uint8_t(expected));
            NUI_CHECK(pixels[1] == std::uint8_t(expected >> 8));
            NUI_CHECK(pixels[2] == std::uint8_t(expected >> 16));
            NUI_CHECK(pixels[4] == std::uint8_t(expected >> 24));
        }
    }
}

void sksl_byte_lane_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kWorleyNoiseKernelSkSL);
    source.append(R"(
        uniform float test_value;
        uniform float test_add;
        uniform float test_byte;
        half4 main(float2 p) {
            float4 v = bytes_of_int(test_value);
            if (test_add > 0.0) v = add_one(v);
            else v = subtract_one(v);
            float selected = v.x;
            if (test_byte > 0.5) selected = v.y;
            if (test_byte > 1.5) selected = v.z;
            if (test_byte > 2.5) selected = v.w;
            return half4(selected / 255.0, 0.0, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    const auto read_byte = [&compiled](std::int32_t value, bool add, int lane) {
        ui::ShaderInstance shader{compiled.program};
        NUI_CHECK(shader.set_float("test_value", float(value)) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float("test_add", add ? 1.0f : 0.0f) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float("test_byte", float(lane)) ==
                  ui::ShaderSetResult::Ok);
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
        NUI_CHECK(surface);
        ui::Painter painter{*surface->getCanvas()};
        painter.fill_rounded_rect({0, 0, 1, 1}, 0.0f, ui::Brush{shader});
        SkPixmap pixels;
        NUI_CHECK(surface->peekPixels(&pixels));
        return std::lround(pixels.getColor4f(0, 0).fR * 255.0f);
    };
    for (std::int32_t value : {std::int32_t{0}, std::int32_t{1},
                               std::int32_t{-1}, std::int32_t{2147483520},
                               std::int32_t{-2147483520}}) {
        for (bool add : {false, true}) {
            const std::uint32_t expected =
                std::uint32_t(value) + (add ? 1u : 0xffffffffu);
            for (int lane = 0; lane < 4; ++lane) {
                NUI_CHECK(read_byte(value, add, lane) ==
                          long((expected >> (lane * 8)) & 255u));
            }
        }
    }
}

void sksl_reference_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kWorleyNoiseKernelSkSL);
    source.append(R"(
        uniform float2 test_point;
        half4 main(float2 p) {
            float2 v = worley_noise(test_point);
            return half4(v.x, v.y, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    NUI_CHECK(compiled.ok());
    const auto read = [&compiled](std::uint32_t seed, float feature_size,
                                  float x, float y) {
        ui::ShaderInstance shader{compiled.program};
        NUI_CHECK(shader.set_float("feature_size", feature_size) ==
                  ui::ShaderSetResult::Ok);
        NUI_CHECK(shader.set_float4("seed_bytes", bytes(seed)) ==
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
        return pixels.getColor4f(0, 0);
    };
    constexpr struct {
        std::uint32_t seed;
        float feature_size, x, y;
    } samples[]{
        {0x12345678u, 48.0f, 12.0f, 0.0f},
        {0x12345678u, 48.0f, 24.0f, 24.0f},
        {0x31415926u, 72.0f, -36.0f, 18.0f},
        {0x27182818u, 52.0f, 13.0f, 7.0f},
    };
    for (const auto& s : samples) {
        const auto expected = ui::detail::worley_noise_reference(
            s.seed, s.feature_size, s.x, s.y);
        const auto actual = read(s.seed, s.feature_size, s.x, s.y);
        NUI_CHECK(std::abs(double(actual.fR) - expected.f1) <= 0.02);
        NUI_CHECK(std::abs(double(actual.fG) - expected.f2) <= 0.02);
    }
    const auto upper = read(0x12345678u, 1.0f, 2147483648.0f, 0.0f);
    const auto lower = read(0x12345678u, 1.0f, -2147483648.0f, 0.0f);
    NUI_CHECK(upper.fR == 0.5f && upper.fG == 0.5f);
    NUI_CHECK(lower.fR == 0.5f && lower.fG == 0.5f);
}

ui::Rgba8 render_pixel(const ui::Brush& brush, int x, int y) {
    ui::UI tree{ui::Canvas{64.0f, 64.0f,
        [brush](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, 64, 64}, brush);
        }}};
    ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    return renderer.pixel(x, y);
}

void raster_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    const auto f1 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 48.0f, .seed = seed});
    const auto f2 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF2, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(f1.ok() && f2.ok());
    const auto b1 = f1.noise.as_brush();
    const auto b2 = f2.noise.as_brush();
    for (const auto [x, y] : {std::array<int, 2>{0, 0}, {8, 8},
                              {24, 24}, {48, 16}, {63, 63}}) {
        const auto expected = ui::detail::worley_noise_reference(
            seed, 48.0, double(x) + 0.5, double(y) + 0.5);
        const auto p1 = render_pixel(b1, x, y);
        const auto p2 = render_pixel(b2, x, y);
        NUI_CHECK(std::abs(double(p1.r) / 255.0 - expected.f1) < 0.015);
        NUI_CHECK(std::abs(double(p2.r) / 255.0 - expected.f2) < 0.015);
        NUI_CHECK(p1.r == p1.g && p1.r == p1.b && p1.a == 255);
        NUI_CHECK(p2.r == p2.g && p2.r == p2.b && p2.a == 255);
        NUI_CHECK(int(p1.r) <= int(p2.r) + 1);
    }

    const auto same = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 48.0f, .seed = seed});
    const auto other = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 48.0f, .seed = 0x87654321u});
    NUI_CHECK(same.ok() && other.ok());
    NUI_CHECK(render_pixel(same.noise.as_brush(), 17, 11).r ==
              render_pixel(b1, 17, 11).r);
    NUI_CHECK(render_pixel(other.noise.as_brush(), 17, 11).r !=
              render_pixel(b1, 17, 11).r);
}

void family_non_alias_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    bool f1_value = false, f1_perlin = false, f1_simplex = false, f1_f2 = false;
    for (const auto [x, y] : {std::array<double, 2>{0.0, -37.5},
                              {24.0, 24.0}, {62.5, 50.0}}) {
        const auto w = ui::detail::worley_noise_reference(seed, 48.0, x, y);
        const double value = ui::detail::value_noise_reference(seed, 48.0, x, y);
        const double perlin = ui::detail::perlin_noise_reference(seed, 48.0, x, y);
        const double simplex = ui::detail::simplex_noise_reference(seed, 48.0, x, y);
        f1_value |= std::abs(w.f1 - value) > 0.01;
        f1_perlin |= std::abs(w.f1 - perlin) > 0.01;
        f1_simplex |= std::abs(w.f1 - simplex) > 0.01;
        f1_f2 |= std::abs(w.f1 - w.f2) > 0.01;
    }
    NUI_CHECK(f1_value && f1_perlin && f1_simplex && f1_f2);
}

void independent_views() {
    const auto a = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1,
        {.feature_size = 16.0f, .seed = 0x98765432u});
    const auto b = ui::NoiseSource::create(
        ui::NoiseType::WorleyF2,
        {.feature_size = 16.0f, .seed = 0x11111111u});
    NUI_CHECK(a.ok() && b.ok());
    const auto brush_a = a.noise.as_brush();
    const auto brush_b = b.noise.as_brush();
    const auto make_tree = [](const ui::Brush& brush) {
        return ui::UI{ui::Canvas{32, 32,
            [brush](ui::CanvasContext2D& g) {
                g.fill_rect({0, 0, 32, 32}, brush);
            }}};
    };

    auto tree_b = make_tree(brush_b);
    ui::HeadlessRenderer renderer_b{{32, 32}, 1.0f};
    std::uint8_t expected_a = 0;
    std::uint8_t expected_b = 0;
    {
        auto tree_a = make_tree(brush_a);
        ui::HeadlessRenderer renderer_a{{32, 32}, 1.0f};
        NUI_CHECK(renderer_a.render(tree_a));
        NUI_CHECK(renderer_b.render(tree_b));
        expected_a = renderer_a.pixel(12, 9).r;
        expected_b = renderer_b.pixel(12, 9).r;
    }
    NUI_CHECK(renderer_b.render(tree_b));
    NUI_CHECK(renderer_b.pixel(12, 9).r == expected_b);

    ui::Brush retained{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};
    {
        const auto temporary = ui::NoiseSource::create(
            ui::NoiseType::WorleyF1,
            {.feature_size = 16.0f, .seed = 0x98765432u});
        NUI_CHECK(temporary.ok());
        retained = temporary.noise.as_brush();
    }
    NUI_CHECK(render_pixel(retained, 12, 9).r == expected_a);

    const auto failed = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 0.0f});
    NUI_CHECK(!failed.ok());
    NUI_CHECK(renderer_b.render(tree_b));
    NUI_CHECK(renderer_b.pixel(12, 9).r == expected_b);
}

void benchmark() {
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto simplex = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto f1 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto f2 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF2, {.feature_size = 48.0f, .seed = 0x12345678u});
    NUI_CHECK(perlin.ok() && simplex.ok() && f1.ok() && f2.ok());
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
    std::cout << "T091 warm 256x256 raster median (5 runs): perlin="
              << measure(perlin.noise.as_brush())
              << " ms, simplex=" << measure(simplex.noise.as_brush())
              << " ms, worley_f1=" << measure(f1.noise.as_brush())
              << " ms, worley_f2=" << measure(f2.noise.as_brush()) << " ms\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        salted_hash_contract();
        row_major_candidate_contract();
        tie_insertion_contract();
        reference_contract();
        local_distance_contract();
        bounded_neighborhood_contract();
        guard_contract();
        sksl_salted_hash_contract();
        sksl_byte_lane_contract();
        sksl_reference_contract();
        raster_contract();
        family_non_alias_contract();
        independent_views();

        const auto invalid_type = ui::NoiseSource::create(
            static_cast<ui::NoiseType>(999));
        NUI_CHECK(!invalid_type.ok());
        NUI_CHECK(invalid_type.error == ui::NoiseCreateError::InvalidArgument);

        for (auto type : {ui::NoiseType::WorleyF1, ui::NoiseType::WorleyF2}) {
            const auto valid = ui::NoiseSource::create(
                type, {.feature_size = 28.0f, .seed = 0xc311c0deu});
            NUI_CHECK(valid.ok());
            NUI_CHECK(valid.error == ui::NoiseCreateError::None);
            NUI_CHECK(valid.diagnostic.empty());
            NUI_CHECK(ui::detail::ShaderBrushAccess::is_shader(
                valid.noise.as_brush()));
            for (float size : {0.0f, -1.0f,
                               std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()}) {
                const auto invalid =
                    ui::NoiseSource::create(type, {.feature_size = size});
                NUI_CHECK(!invalid.ok());
                NUI_CHECK(invalid.error ==
                          ui::NoiseCreateError::InvalidArgument);
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T091 Worley noise: " << e.what() << '\n';
        return 1;
    }
}
