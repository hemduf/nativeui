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
    // All eight gradient indices map to the exact frozen vectors, and the
    // index is the low three bits exactly like hash & 7.
    for (std::uint32_t i = 0; i < 8; ++i) {
        const auto g = ui::detail::noise_gradient(i);
        NUI_CHECK(g.x == kGradientTable[i][0]);
        NUI_CHECK(g.y == kGradientTable[i][1]);
        const auto masked = ui::detail::noise_gradient(i + 8u * 5u);
        NUI_CHECK(masked.x == g.x && masked.y == g.y);
    }
}

void sksl_gradient_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kSimplexNoiseKernelSkSL);
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

void skew_unskew_contract() {
    // The normative T090 skew constants are the exact decimal literals used by
    // the CPU oracle and the SkSL kernel.
    NUI_CHECK(ui::detail::kSimplexF2 ==
              0.366025403784438646763723170752936);
    NUI_CHECK(ui::detail::kSimplexG2 ==
              0.211324865405187117745425609749021);

    // Independent fixed skew/unskew fixtures (fs = 48). The (96,0) fixture has
    // |x0 - y0| at one double ulp and still takes the strict x0 > y0 branch.
    struct Fixture {
        double x, y;
        double s, u, v;
        std::int32_t i, j;
        double x0, y0;
        std::int32_t i1, j1;
        double x1, y1, x2, y2;
    };
    const Fixture fixtures[]{
        {12.0, 0.0, 0.09150635094610966, 0.34150635094610965,
         0.09150635094610966, 0, 0, 0.25, 0.0, 1, 0,
         -0.5386751345948129, 0.2113248654051871,
         -0.3273502691896258, -0.5773502691896257},
        {12.0, 24.0, 0.274519052838329, 0.524519052838329,
         0.774519052838329, 0, 0, 0.25, 0.5, 0, 1,
         0.46132486540518713, -0.28867513459481287,
         -0.3273502691896258, -0.07735026918962579},
        {-12.0, 60.0, 0.36602540378443865, 0.11602540378443865,
         1.6160254037844386, 0, 1, -0.03867513459481289, 0.46132486540518713,
         0, 1, 0.1726497308103742, -0.32735026918962573,
         -0.6160254037844386, -0.11602540378443865},
        {96.0, 0.0, 0.7320508075688773, 2.732050807568877,
         0.7320508075688773, 2, 0, 0.42264973081037427, 0.4226497308103742,
         1, 0, -0.3660254037844386, 0.6339745962155613,
         -0.15470053837925152, -0.15470053837925152},
        {36.0, -12.0, 0.18301270189221933, 0.9330127018922193,
         -0.06698729810778067, 0, -1, 0.5386751345948129, 0.5386751345948129,
         0, 1, 0.75, -0.25,
         -0.03867513459481292, -0.03867513459481292},
    };
    for (const auto& f : fixtures) {
        const double nx = f.x / 48.0;
        const double ny = f.y / 48.0;
        const double s = (nx + ny) * ui::detail::kSimplexF2;
        const double u = nx + s;
        const double v = ny + s;
        NUI_CHECK(std::abs(s - f.s) <= 1e-14);
        NUI_CHECK(std::abs(u - f.u) <= 1e-14);
        NUI_CHECK(std::abs(v - f.v) <= 1e-14);
        const auto li = static_cast<std::int32_t>(std::floor(u));
        const auto lj = static_cast<std::int32_t>(std::floor(v));
        NUI_CHECK(li == f.i && lj == f.j);
        const double t =
            (double(li) + double(lj)) * ui::detail::kSimplexG2;
        const double x0 = nx - (double(li) - t);
        const double y0 = ny - (double(lj) - t);
        NUI_CHECK(std::abs(x0 - f.x0) <= 1e-14);
        NUI_CHECK(std::abs(y0 - f.y0) <= 1e-14);
        std::int32_t i1 = 0;
        std::int32_t j1 = 1;
        if (x0 > y0) {
            i1 = 1;
            j1 = 0;
        }
        NUI_CHECK(i1 == f.i1 && j1 == f.j1);
        const double x1 = x0 - double(i1) + ui::detail::kSimplexG2;
        const double y1 = y0 - double(j1) + ui::detail::kSimplexG2;
        const double x2 = x0 - 1.0 + 2.0 * ui::detail::kSimplexG2;
        const double y2 = y0 - 1.0 + 2.0 * ui::detail::kSimplexG2;
        NUI_CHECK(std::abs(x1 - f.x1) <= 1e-14);
        NUI_CHECK(std::abs(y1 - f.y1) <= 1e-14);
        NUI_CHECK(std::abs(x2 - f.x2) <= 1e-14);
        NUI_CHECK(std::abs(y2 - f.y2) <= 1e-14);
    }
}

void reference_contract() {
    // Independent fixed CPU double reference values for the frozen T090
    // kernel, produced by an external script that transcribes the issue
    // formulas directly (never from the C++ implementation). Tolerance 1e-12.
    const struct {
        std::uint32_t seed;
        double feature_size, x, y, value;
    } vectors[]{
        {0x12345678u, 48.0, 12.0, 0.0, 0.4856869072107538},
        {0x12345678u, 48.0, 24.0, 24.0, 0.2828075463285771},
        {0x12345678u, 48.0, 36.0, 36.0, 0.8339448681579731},
        {0x12345678u, 48.0, -12.0, 60.0, 0.30838400437756935},
        {0x12345678u, 48.0, 3.5, -7.25, 0.23805600217458583},
        {0x31415926u, 72.0, 18.0, 54.0, 0.20210344671331237},
        {0x31415926u, 72.0, -36.0, 18.0, 0.671424532104264},
        {0x31415926u, 72.0, 36.0, -72.0, 0.41680452212885993},
        {0x31415926u, 72.0, 100.0, -200.0, 0.8108975608181115},
        {0x00000000u, 1.0, 2.5, -3.25, 0.3551692155394751},
        {0x27182818u, 52.0, 13.0, 7.0, 0.6400109464729645},
        {0x27182818u, 52.0, -39.0, 26.0, 0.3221640748654946},
        {0xFFFF00FFu, 33.0, -16.25, -8.75, 0.22416271348273498},
    };
    for (const auto& v : vectors) {
        NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
            v.seed, v.feature_size, v.x, v.y) - v.value) < 1e-12);
    }

    // Deterministic grid: output range, no NaN, exact repeat.
    const std::uint32_t seeds[]{0x12345678u, 0x31415926u, 0u, 0xffffffffu};
    for (std::uint32_t seed : seeds) {
        std::vector<double> first;
        first.reserve(41 * 41);
        for (int j = -20; j <= 20; ++j) {
            for (int i = -20; i <= 20; ++i) {
                const double value = ui::detail::simplex_noise_reference(
                    seed, 48.0, double(i) * 6.5 - 7.25, double(j) * 6.5 + 3.75);
                NUI_CHECK(std::isfinite(value) && value >= 0.0 && value <= 1.0);
                first.push_back(value);
            }
        }
        std::size_t index = 0;
        for (int j = -20; j <= 20; ++j) {
            for (int i = -20; i <= 20; ++i) {
                const double value = ui::detail::simplex_noise_reference(
                    seed, 48.0, double(i) * 6.5 - 7.25, double(j) * 6.5 + 3.75);
                NUI_CHECK(value == first[index++]);
            }
        }
    }
}

void tie_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    // x == y makes x0 == y0 bit-exactly; equality must take the second branch.
    NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
        seed, 48.0, 24.0, 24.0) - 0.2828075463285771) < 1e-12);
    NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
        0x00000000u, 1.0, 5.0, 5.0) - 0.5362613880464462) < 1e-12);
    NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
        0x31415926u, 72.0, -36.0, -36.0) - 0.5) < 1e-12);

    // No-epsilon probes at 1e-9 around the tie: the lower probe flips to the
    // strict first branch (x0 > y0), the upper probe stays on the second.
    const double tie = ui::detail::simplex_noise_reference(
        seed, 48.0, 24.0, 24.0);
    const double up = ui::detail::simplex_noise_reference(
        seed, 48.0, 24.0, 24.0 + 1e-9);
    const double down = ui::detail::simplex_noise_reference(
        seed, 48.0, 24.0, 24.0 - 1e-9);
    NUI_CHECK(std::abs(up - 0.2828075463520889) < 1e-12);
    NUI_CHECK(std::abs(down - 0.28280754630506527) < 1e-12);
    NUI_CHECK(up != tie && down != tie && up != down);
}

void corner_contract() {
    // q < 0 and q == 0 both contribute exactly zero.
    NUI_CHECK(ui::detail::simplex_corner_contribution(0, 1.0, 1.0) == 0.0);
    NUI_CHECK(ui::detail::simplex_corner_contribution(4, -0.5, -0.5) == 0.0);
    NUI_CHECK(ui::detail::simplex_corner_contribution(4, 0.5, 0.5) == 0.0);
    // Small positive q keeps the q^4 * dot contract (not clamped to zero).
    NUI_CHECK(std::abs(ui::detail::simplex_corner_contribution(0, 0.7, 0.0) -
                       7.00000000000018e-09) < 1e-15);
    // Representative interior value with the diagonal (S,S) gradient.
    NUI_CHECK(std::abs(ui::detail::simplex_corner_contribution(4, 0.1, 0.2) -
                       0.008698739233809255) < 1e-15);
    // Dot sign follows the selected gradient for every frozen index.
    const double expected[8]{
        0.001171875,          -0.001171875,          -0.0015625,
        0.0015625,            -0.00027621358640099534, -0.001933495104806966,
        0.001933495104806966, 0.00027621358640099534,
    };
    for (std::uint32_t g = 0; g < 8; ++g) {
        NUI_CHECK(std::abs(ui::detail::simplex_corner_contribution(
            g, 0.3, -0.4) - expected[g]) < 1e-15);
    }
}

void boundary_continuity_contract() {
    constexpr std::uint32_t seed = 0x12345678u;
    constexpr double feature_size = 48.0;
    constexpr double eps = 1e-4;
    // Value continuity (1e-5) and one-sided first derivative agreement (1e-3)
    // across simplex cell boundaries follow the T089 probe tolerances.
    constexpr double kValueTolerance = 1e-5;
    constexpr double kDerivativeTolerance = 1e-3;

    // Cell edges (u integer and v integer) and a skewed cell corner (u,v
    // integer). At an exact corner a==0.5; probes stay within 1e-5.
    const struct {
        double x, y;
    } boundaries[]{
        {6.430780618346946, -24.0},
        {18.0, -4.82308546376021},
        {85.85640646055101, -58.143593539448986},
    };
    for (const auto& b : boundaries) {
        const double at = ui::detail::simplex_noise_reference(
            seed, feature_size, b.x, b.y);
        const double left = ui::detail::simplex_noise_reference(
            seed, feature_size, b.x - eps, b.y);
        const double right = ui::detail::simplex_noise_reference(
            seed, feature_size, b.x + eps, b.y);
        const double down = ui::detail::simplex_noise_reference(
            seed, feature_size, b.x, b.y - eps);
        const double up = ui::detail::simplex_noise_reference(
            seed, feature_size, b.x, b.y + eps);
        NUI_CHECK(std::abs(left - at) < kValueTolerance);
        NUI_CHECK(std::abs(right - at) < kValueTolerance);
        NUI_CHECK(std::abs(down - at) < kValueTolerance);
        NUI_CHECK(std::abs(up - at) < kValueTolerance);
        NUI_CHECK(std::abs((at - left) / eps - (right - at) / eps) <=
                  kDerivativeTolerance);
        NUI_CHECK(std::abs((at - down) / eps - (up - at) / eps) <=
                  kDerivativeTolerance);
    }

    // Triangle branch diagonal (x == y, so x0 == y0 exactly).
    for (double diagonal : {24.0, 0.0, -24.0}) {
        const double at = ui::detail::simplex_noise_reference(
            seed, feature_size, diagonal, diagonal);
        const double left = ui::detail::simplex_noise_reference(
            seed, feature_size, diagonal, diagonal - eps);
        const double right = ui::detail::simplex_noise_reference(
            seed, feature_size, diagonal, diagonal + eps);
        NUI_CHECK(std::abs(left - at) < kValueTolerance);
        NUI_CHECK(std::abs(right - at) < kValueTolerance);
        NUI_CHECK(std::abs((at - left) / eps - (right - at) / eps) <=
                  kDerivativeTolerance);
    }

    // Frozen reference sanity: at (24,24) the one-sided Y derivative is
    // approximately 0.0235116 on both sides of the branch diagonal.
    const double at = ui::detail::simplex_noise_reference(
        seed, feature_size, 24.0, 24.0);
    const double left = ui::detail::simplex_noise_reference(
        seed, feature_size, 24.0, 24.0 - eps);
    const double right = ui::detail::simplex_noise_reference(
        seed, feature_size, 24.0, 24.0 + eps);
    NUI_CHECK(std::abs((at - left) / eps - 0.02351162395608508) < 1e-9);
    NUI_CHECK(std::abs((right - at) / eps - 0.023511921639629563) < 1e-9);
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
        NUI_CHECK(ui::detail::simplex_noise_reference(
            seed, 1.0, coordinate, 0.0) == 0.5);
        NUI_CHECK(ui::detail::simplex_noise_reference(
            seed, 1.0, 0.0, coordinate) == 0.5);
        NUI_CHECK(ui::detail::simplex_noise_reference(
            seed, 1.0, coordinate, coordinate) == 0.5);
    }
    // The last representable simplex lattice neighborhood still evaluates
    // without undefined conversion or overflow.
    NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
        seed, 1.0, 2147483000.25, -2147483000.75) -
        0.5825951104984161) < 1e-12);
    NUI_CHECK(std::abs(ui::detail::simplex_noise_reference(
        seed, 1.0, 1000000000.5, -1000000000.25) -
        0.7684670833533876) < 1e-12);
    // A guard-rejected near-extreme point stays exactly 0.5.
    NUI_CHECK(ui::detail::simplex_noise_reference(
        seed, 1.0, 2147483646.5, -2147483648.0) == 0.5);
}

void value_perlin_simplex_non_alias() {
    // Frozen representative points where the three built-in kernels disagree
    // by a documented margin (minimum pairwise gap > 0.1).
    const struct {
        std::uint32_t seed;
        double feature_size, x, y, value, perlin, simplex;
    } vectors[]{
        {0x12345678u, 48.0, 0.0, -37.5,
         0.1304237390875045, 0.4797441422876008, 0.8225332824863649},
        {0x27182818u, 52.0, 62.5, 50.0,
         0.07458760093591144, 0.43212382302252034, 0.8002158728369766},
        {0xFFFF00FFu, 72.0, -12.5, -87.5,
         0.9269595353198263, 0.5654908910823571, 0.19337267095349647},
    };
    for (const auto& v : vectors) {
        const double value = ui::detail::value_noise_reference(
            v.seed, v.feature_size, v.x, v.y);
        const double perlin = ui::detail::perlin_noise_reference(
            v.seed, v.feature_size, v.x, v.y);
        const double simplex = ui::detail::simplex_noise_reference(
            v.seed, v.feature_size, v.x, v.y);
        NUI_CHECK(std::abs(value - v.value) < 1e-12);
        NUI_CHECK(std::abs(perlin - v.perlin) < 1e-12);
        NUI_CHECK(std::abs(simplex - v.simplex) < 1e-12);
        NUI_CHECK(std::abs(value - perlin) > 0.1);
        NUI_CHECK(std::abs(value - simplex) > 0.1);
        NUI_CHECK(std::abs(perlin - simplex) > 0.1);
        for (double output : {value, perlin, simplex}) {
            NUI_CHECK(std::isfinite(output) && output >= 0.0 &&
                      output <= 1.0);
        }
    }
}

void sksl_guard_parity_contract() {
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kSimplexNoiseKernelSkSL);
    source.append(R"(
        uniform float2 test_point;
        half4 main(float2 p) {
            float v = simplex_noise(test_point);
            return half4(v, v, v, 1.0);
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
        return pixels.getColor4f(0, 0).fR;
    };
    // Normalized out-of-range and skew out-of-range inputs return exactly 0.5
    // in the SkSL float path; no undefined conversion is reachable.
    NUI_CHECK(read(0x12345678u, 1.0f, 2147483648.0f, 0.0f) == 0.5f);
    NUI_CHECK(read(0x12345678u, 1.0f, -4294967296.0f, 0.0f) == 0.5f);
    NUI_CHECK(read(0x12345678u, 1.0f, std::numeric_limits<float>::max(),
                   0.0f) == 0.5f);
    // x/y are finite and in range but the skew pushes u/v out of range.
    NUI_CHECK(read(0x12345678u, 1.0f, 1500000000.0f, 1500000000.0f) == 0.5f);

    // Moderate samples match the CPU double oracle within 0.02.
    const struct {
        std::uint32_t seed;
        float feature_size, x, y;
        double expected;
    } samples[]{
        {0x12345678u, 48.0f, 12.5f, 0.5f, 0.4957943289292489},
        {0x12345678u, 48.0f, 23.75f, 47.25f, 0.7568390954304484},
        {0x12345678u, 48.0f, -12.25f, 60.5f, 0.2960971408149432},
        {0x31415926u, 72.0f, 100.5f, -200.25f, 0.8139006026469601},
        {0x27182818u, 52.0f, 13.75f, 7.5f, 0.6332794118255839},
    };
    for (const auto& s : samples) {
        const float actual =
            read(s.seed, s.feature_size, s.x, s.y);
        NUI_CHECK(std::abs(double(actual) - s.expected) <= 0.02);
    }
    // The frozen float path is bit-exact across repeated evaluations.
    NUI_CHECK(read(0x12345678u, 48.0f, 23.75f, 47.25f) ==
              read(0x12345678u, 48.0f, 23.75f, 47.25f));
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
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(source.ok());
    const auto brush = source.noise.as_brush();
    // Raster pixels match the CPU double oracle within 0.015 (the frozen
    // T089/T090 tolerance pair); the GPU test stores the same pair.
    for (const auto [x, y] : {std::array<int, 2>{0, 0}, {8, 8},
                              {24, 24}, {48, 16}, {63, 63}}) {
        const auto actual = render_pixel(brush, x, y);
        const double expected = ui::detail::simplex_noise_reference(
            seed, 48.0, double(x) + 0.5, double(y) + 0.5);
        NUI_CHECK(std::abs(double(actual.r) / 255.0 - expected) < 0.015);
        NUI_CHECK(actual.r == actual.g && actual.r == actual.b);
        NUI_CHECK(actual.a == 255);
    }
    NUI_CHECK(render_pixel(brush, 8, 8).r == render_pixel(brush, 8, 8).r);
    const auto same_seed = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = seed});
    NUI_CHECK(same_seed.ok());
    NUI_CHECK(render_pixel(same_seed.noise.as_brush(), 8, 8).r ==
              render_pixel(brush, 8, 8).r);
    const auto different = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = 0x87654321u});
    NUI_CHECK(different.ok());
    NUI_CHECK(render_pixel(different.noise.as_brush(), 8, 8).r !=
              render_pixel(brush, 8, 8).r);

    const auto scaled = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 24.0f, .seed = seed});
    NUI_CHECK(scaled.ok());
    // A half-feature brush painted through a 2x logical transform samples the
    // identical logical coordinates as the direct brush, so the raster output
    // must agree within 3/255. (The T089-style approximate probe at pixel
    // (16,16) vs (8,8) is not used: simplex pixel centers do not align under a
    // 2x feature-size change and it measured 6.2/255.)
    const auto transformed_pixel = [&scaled](int x, int y) {
        const auto scaled_brush = scaled.noise.as_brush();
        ui::UI tree{ui::Canvas{64.0f, 64.0f,
            [scaled_brush](ui::CanvasContext2D& g) {
                g.save();
                g.scale(2.0f);
                g.fill_rect({0, 0, 32, 32}, scaled_brush);
                g.restore();
            }}};
        ui::HeadlessRenderer renderer{{64.0f, 64.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        return renderer.pixel(x, y);
    };
    NUI_CHECK(std::abs(int(render_pixel(brush, 16, 16).r) -
                       int(transformed_pixel(16, 16).r)) < 3);
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
        ui::NoiseType::Simplex, {.feature_size = 16.0f, .seed = 0x98765432u});
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

    // A retained Brush stays valid after its originating NoiseSource dies, and
    // a failed/destroyed source A cannot disturb source B.
    ui::Brush retained{ui::Color{0.0f, 0.0f, 0.0f, 0.0f}};
    std::uint8_t expected_a = 0;
    std::uint8_t expected_b = 0;
    {
        const auto a = ui::NoiseSource::create(
            ui::NoiseType::Simplex, {.feature_size = 16.0f, .seed = 0x98765432u});
        const auto b = ui::NoiseSource::create(
            ui::NoiseType::Simplex, {.feature_size = 16.0f, .seed = 0x11111111u});
        NUI_CHECK(a.ok() && b.ok());
        retained = a.noise.as_brush();
        expected_a = render_pixel(retained, 12, 9).r;
        expected_b = render_pixel(b.noise.as_brush(), 12, 9).r;
        NUI_CHECK(expected_a != expected_b);
    }
    NUI_CHECK(render_pixel(retained, 12, 9).r == expected_a);

    const auto failed = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 0.0f});
    NUI_CHECK(!failed.ok());
    const auto still_valid = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 16.0f, .seed = 0x11111111u});
    NUI_CHECK(still_valid.ok());
    NUI_CHECK(render_pixel(still_valid.noise.as_brush(), 12, 9).r ==
              expected_b);
}

void benchmark() {
    const auto simplex = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = 0x12345678u});
    NUI_CHECK(simplex.ok() && perlin.ok());
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
    const auto solid_ms =
        measure(ui::Brush{ui::Color{0.5f, 0.5f, 0.5f, 1.0f}});
    const auto perlin_ms = measure(perlin.noise.as_brush());
    const auto simplex_ms = measure(simplex.noise.as_brush());
    std::cout << "T090 warm 256x256 raster median (5 runs): solid=" << solid_ms
              << " ms, perlin=" << perlin_ms
              << " ms, simplex=" << simplex_ms << " ms\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        NUI_CHECK(!ui::NoiseCreateResult{}.ok());
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        hash_and_gradient_contract();
        sksl_gradient_contract();
        skew_unskew_contract();
        reference_contract();
        tie_contract();
        corner_contract();
        boundary_continuity_contract();
        guard_contract();
        value_perlin_simplex_non_alias();
        sksl_guard_parity_contract();
        raster_contract();
        independent_views();

        ui::NoiseSource inert;
        NUI_CHECK(!ui::detail::ShaderBrushAccess::is_shader(inert.as_brush()));

        const auto valid = ui::NoiseSource::create(
            ui::NoiseType::Simplex,
            {.feature_size = 52.0f, .seed = 0x27182818u});
        NUI_CHECK(valid.ok());
        NUI_CHECK(valid.error == ui::NoiseCreateError::None);
        NUI_CHECK(valid.diagnostic.empty());
        NUI_CHECK(
            ui::detail::ShaderBrushAccess::is_shader(valid.noise.as_brush()));

        for (float size : {0.0f, -1.0f,
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN()}) {
            const auto invalid = ui::NoiseSource::create(
                ui::NoiseType::Simplex, {.feature_size = size});
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
        std::cerr << "FAIL T090 simplex noise: " << e.what() << '\n';
        return 1;
    }
}
