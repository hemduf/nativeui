#include "src/detail/noise_math.hpp"
#include "src/detail/noise_sksl.hpp"
#include "src/detail/platform_test_access.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

// THE single documented numeric tolerance pair for T090 raster/GPU parity:
//   1. raster pixels vs the CPU double oracle: < 0.015 (T088/T089 precedent);
//   2. GPU readback vs the raster reference: <= 5/255 per channel.
// Gradient-index/hash vectors are compared bit-exactly on CPU and within the
// 5/255 GPU readback tolerance on GPU.

[[nodiscard]] ui::UI make_ui(const ui::Brush& brush, float size = 64.0f) {
    return ui::UI{ui::Canvas{size, size,
        [brush, size](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, size, size}, brush);
        }}};
}

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

std::array<float, 4> bytes(std::uint32_t v) {
    return {float(v & 255u), float((v >> 8) & 255u),
            float((v >> 16) & 255u), float((v >> 24) & 255u)};
}

double benchmark_window(const ui::Brush& brush) {
    ui::Application app;
    check(app.valid(), "benchmark application is invalid");
    auto tree = make_ui(brush, 256.0f);
    ui::StandaloneWindow window{
        app, tree,
        ui::WindowDesc{.title = "NativeUI T090 benchmark",
                       .size = {256, 256}, .resizable = false}};
    if (!window.valid()) {
        throw std::runtime_error{std::string{"benchmark window: "} +
                                 std::string{window.last_error()}};
    }
    const auto frame = [&] {
        check(ui::detail::PlatformTestAccess::request_gpu_readback(
            window, {128, 128}), "benchmark readback rejected");
        std::optional<ui::detail::PlatformReadbackPixel> pixel;
        for (int i = 0; i < 64 && !pixel; ++i) {
            (void)app.poll(0.0);
            pixel = ui::detail::PlatformTestAccess::take_gpu_readback(window);
        }
        check(pixel.has_value(), "benchmark readback incomplete");
    };
    frame();
    std::array<double, 5> samples{};
    for (double& sample : samples) {
        const auto begin = std::chrono::steady_clock::now();
        frame();
        sample = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
    }
    std::sort(samples.begin(), samples.end());
    return samples[2];
}

void benchmark() {
    const auto simplex = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = 0x12345678u});
    check(simplex.ok() && perlin.ok(), "benchmark source failed to compile");
    const double solid_ms = benchmark_window(
        ui::Brush{ui::Color{0.5f, 0.5f, 0.5f, 1.0f}});
    const double perlin_ms = benchmark_window(perlin.noise.as_brush());
    const double simplex_ms = benchmark_window(simplex.noise.as_brush());
    std::cout << "T090 warm 256x256 logical GPU redraw+readback median (5 runs): "
              << "solid=" << solid_ms << " ms, perlin=" << perlin_ms
              << " ms, simplex=" << simplex_ms << " ms\n";
}

void gpu_gradient_vectors(ui::Application& app) {
    constexpr double kS = 0.707106781186547524400844362104849;
    constexpr double kGradientTable[8][2]{
        {1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0},
        {kS, kS}, {-kS, kS}, {kS, -kS}, {-kS, -kS},
    };
    constexpr struct {
        std::uint32_t seed;
        std::int32_t x;
        std::int32_t y;
        std::uint32_t hash;
        std::uint32_t gradient_index;
    } vectors[]{
        {0u, 0, 0, 0x00000000u, 0u},
        {0x12345678u, 17, -9, 0x9342507bu, 3u},
        {0xffffffffu, -1, -1, 0xcf6c0c3au, 2u},
        {0x87654321u, std::numeric_limits<std::int32_t>::min(), 0,
         0x630d83f1u, 1u},
        {0x87654321u, std::numeric_limits<std::int32_t>::max(),
         std::numeric_limits<std::int32_t>::min(), 0xa02e9e13u, 3u},
    };
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(ui::detail::kSimplexNoiseKernelSkSL);
    source.append(R"(
        uniform float4 x_bytes;
        uniform float4 y_bytes;
        half4 main(float2 p) {
            float2 g = gradient_of(hash2(seed_bytes, x_bytes, y_bytes));
            return half4((g.x + 1.0) * 0.5, (g.y + 1.0) * 0.5, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    check(compiled.ok(), "GPU gradient shader did not compile");
    std::vector<ui::Brush> brushes;
    brushes.reserve(std::size(vectors));
    for (const auto& v : vectors) {
        check(ui::detail::noise_hash2(v.seed, v.x, v.y) == v.hash &&
                  (v.hash & 7u) == v.gradient_index,
              "CPU hash/gradient vector mismatch");
        ui::ShaderInstance shader{compiled.program};
        check(shader.set_float4("seed_bytes", bytes(v.seed)) ==
                  ui::ShaderSetResult::Ok &&
                  shader.set_float4("x_bytes", bytes(std::uint32_t(v.x))) ==
                      ui::ShaderSetResult::Ok &&
                  shader.set_float4("y_bytes", bytes(std::uint32_t(v.y))) ==
                      ui::ShaderSetResult::Ok,
              "GPU gradient vector binding failed");
        brushes.emplace_back(shader);
    }
    ui::UI tree{ui::Canvas{64, 60,
        [brushes](ui::CanvasContext2D& g) {
            for (std::size_t i = 0; i < brushes.size(); ++i) {
                g.fill_rect({0, float(i * 12), 64, 12}, brushes[i]);
            }
        }}};
    ui::StandaloneWindow window{
        app, tree,
        ui::WindowDesc{.title = "NativeUI T090 GPU gradient vectors",
                       .size = {64, 60}, .resizable = false}};
    check(window.valid(), "GPU gradient vector window is invalid");
    for (std::size_t i = 0; i < std::size(vectors); ++i) {
        const auto read = [&](float x) {
            check(ui::detail::PlatformTestAccess::request_gpu_readback(
                window, {x, float(i * 12 + 6)}),
                "GPU gradient vector readback rejected");
            std::optional<ui::detail::PlatformReadbackPixel> pixel;
            for (int pass = 0; pass < 32 && !pixel; ++pass) {
                (void)app.poll(0.0);
                pixel = ui::detail::PlatformTestAccess::take_gpu_readback(window);
            }
            check(pixel.has_value(), "GPU gradient vector readback incomplete");
            return *pixel;
        };
        // hash & 7 selects the frozen gradient on GPU within the documented
        // 5/255 GPU readback tolerance.
        const auto* gradient = kGradientTable[vectors[i].gradient_index];
        const double expected_r = (gradient[0] + 1.0) * 0.5 * 255.0;
        const double expected_g = (gradient[1] + 1.0) * 0.5 * 255.0;
        for (const auto pixel : {read(16), read(48)}) {
            if (std::abs(double(pixel.r) - expected_r) > 5.0 ||
                std::abs(double(pixel.g) - expected_g) > 5.0 ||
                pixel.b != 0 || pixel.a != 255) {
                throw std::runtime_error{"GPU gradient vector " +
                                         std::to_string(i) +
                                         " diverged from the frozen mapping"};
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        constexpr std::uint32_t seed = 0x12345678u;
        const auto source = ui::NoiseSource::create(
            ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = seed});
        check(source.ok(), "noise source did not compile");
        const auto brush = source.noise.as_brush();

        auto reference_ui = make_ui(brush);
        ui::HeadlessRenderer reference{{64, 64}, 1.0f};
        check(reference.render(reference_ui), "raster reference failed");

        ui::Application application;
        check(application.valid(), "platform application is invalid");
        auto gpu_ui = make_ui(brush);
        ui::StandaloneWindow window{
            application, gpu_ui,
            ui::WindowDesc{.title = "NativeUI T090 GPU reference",
                           .size = {64, 64}, .resizable = false}};
        check(window.valid() && window.native_handle(), "GPU window is invalid");

        constexpr std::array<std::array<int, 2>, 3> samples{{
            {8, 8}, {31, 23}, {52, 45}
        }};
        for (const auto& xy : samples) {
            const auto expected = reference.pixel(xy[0], xy[1]);
            const auto cpu = ui::detail::simplex_noise_reference(
                seed, 48.0, double(xy[0]) + 0.5, double(xy[1]) + 0.5);
            check(std::abs(double(expected.r) / 255.0 - cpu) < 0.015,
                  "raster diverged from CPU reference");

            check(ui::detail::PlatformTestAccess::request_gpu_readback(
                window, ui::Point{float(xy[0]), float(xy[1])}),
                "GPU readback request rejected");
            std::optional<ui::detail::PlatformReadbackPixel> actual;
            for (int iteration = 0; iteration < 32 && !actual; ++iteration) {
                (void)application.poll(0.0);
                actual = ui::detail::PlatformTestAccess::take_gpu_readback(window);
            }
            check(actual.has_value(), "GPU readback incomplete");
            check(std::abs(int(actual->r) - int(expected.r)) <= 5 &&
                      std::abs(int(actual->g) - int(expected.g)) <= 5 &&
                      std::abs(int(actual->b) - int(expected.b)) <= 5 &&
                      actual->a == expected.a,
                  "GPU pixel diverged from raster reference");
        }
        gpu_gradient_vectors(application);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T090 simplex noise GPU reference: " << e.what() << '\n';
        return 1;
    }
}
