#include "src/detail/noise_math.hpp"
#include "src/detail/noise_sksl.hpp"
#include "src/detail/platform_test_access.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/nativeui.hpp>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] ui::UI make_ui(const ui::Brush& brush, float size = 64.0f) {
    return ui::UI{ui::Canvas{size, size,
        [brush, size](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, size, size}, brush);
        }}};
}

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

double benchmark_window(const ui::Brush& brush) {
    ui::Application app;
    check(app.valid(), "benchmark application is invalid");
    auto tree = make_ui(brush, 256.0f);
    ui::StandaloneWindow window{
        app, tree,
        ui::WindowDesc{.title = "NativeUI T088 benchmark",
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
    const auto source = ui::NoiseSource::create(
        ui::NoiseType::Value, {.feature_size = 48.0f, .seed = 0x12345678u});
    check(source.ok(), "benchmark source failed to compile");
    const double solid_ms = benchmark_window(
        ui::Brush{ui::Color{0.5f, 0.5f, 0.5f, 1.0f}});
    const double noise_ms = benchmark_window(source.noise.as_brush());
    std::cout << "T088 warm 256x256 logical GPU redraw+readback median (5 runs): "
              << "solid=" << solid_ms << " ms, noise=" << noise_ms << " ms\n";
}

std::array<float, 4> bytes(std::uint32_t v) {
    return {float(v & 255u), float((v >> 8) & 255u),
            float((v >> 16) & 255u), float((v >> 24) & 255u)};
}

void gpu_hash_vectors(ui::Application& app) {
    constexpr struct {
        std::uint32_t seed;
        std::int32_t x;
        std::int32_t y;
        std::uint32_t expected;
    } vectors[]{
        {0u, 0, 0, 0x00000000u},
        {0x12345678u, 17, -9, 0x9342507bu},
        {0xffffffffu, -1, -1, 0xcf6c0c3au},
        {0x87654321u, std::numeric_limits<std::int32_t>::min(), 0,
         0x630d83f1u},
        {0x87654321u, std::numeric_limits<std::int32_t>::max(),
         std::numeric_limits<std::int32_t>::min(), 0xa02e9e13u},
    };
    std::string source{ui::detail::kNoiseHashSkSL};
    source.append(R"(
        uniform float4 seed_bytes;
        uniform float4 x_bytes;
        uniform float4 y_bytes;
        half4 main(float2 p) {
            float4 h = hash2(seed_bytes, x_bytes, y_bytes);
            if (p.x < 32.0) return half4(h.x / 255.0, h.y / 255.0,
                                         h.z / 255.0, 1.0);
            return half4(h.w / 255.0, 0.0, 0.0, 1.0);
        }
    )");
    const auto compiled = ui::ShaderProgram::compile(source);
    check(compiled.ok(), "GPU hash-only shader did not compile");
    std::vector<ui::Brush> brushes;
    brushes.reserve(std::size(vectors));
    for (const auto& v : vectors) {
        check(ui::detail::noise_hash2(v.seed, v.x, v.y) == v.expected,
              "CPU hash vector mismatch");
        ui::ShaderInstance shader{compiled.program};
        check(shader.set_float4("seed_bytes", bytes(v.seed)) == ui::ShaderSetResult::Ok &&
                  shader.set_float4("x_bytes", bytes(std::uint32_t(v.x))) ==
                      ui::ShaderSetResult::Ok &&
                  shader.set_float4("y_bytes", bytes(std::uint32_t(v.y))) ==
                      ui::ShaderSetResult::Ok,
              "GPU hash vector binding failed");
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
        ui::WindowDesc{.title = "NativeUI T088 GPU hash vectors",
                       .size = {64, 60}, .resizable = false}};
    check(window.valid(), "GPU hash vector window is invalid");
    for (std::size_t i = 0; i < std::size(vectors); ++i) {
        const auto read = [&](float x) {
            check(ui::detail::PlatformTestAccess::request_gpu_readback(
                window, {x, float(i * 12 + 6)}),
                "GPU hash vector readback rejected");
            std::optional<ui::detail::PlatformReadbackPixel> pixel;
            for (int pass = 0; pass < 32 && !pixel; ++pass) {
                (void)app.poll(0.0);
                pixel = ui::detail::PlatformTestAccess::take_gpu_readback(window);
            }
            check(pixel.has_value(), "GPU hash vector readback incomplete");
            return *pixel;
        };
        const auto lo = read(16);
        const auto hi = read(48);
        const auto expected = vectors[i].expected;
        if (lo.r != std::uint8_t(expected) ||
            lo.g != std::uint8_t(expected >> 8) ||
            lo.b != std::uint8_t(expected >> 16) ||
            hi.r != std::uint8_t(expected >> 24) ||
            lo.a != 255 || hi.a != 255) {
            throw std::runtime_error{"GPU hash vector " + std::to_string(i) +
                                     " diverged from exact uint32 result"};
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
            ui::NoiseType::Value, {.feature_size = 48.0f, .seed = seed});
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
            ui::WindowDesc{.title = "NativeUI T088 GPU reference",
                           .size = {64, 64}, .resizable = false}};
        check(window.valid() && window.native_handle(), "GPU window is invalid");

        constexpr std::array<std::array<int, 2>, 3> samples{{
            {8, 8}, {31, 23}, {52, 45}
        }};
        for (const auto& xy : samples) {
            const auto expected = reference.pixel(xy[0], xy[1]);
            const auto cpu = ui::detail::value_noise_reference(
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
        gpu_hash_vectors(application);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T088 noise GPU reference: " << e.what() << '\n';
        return 1;
    }
}
