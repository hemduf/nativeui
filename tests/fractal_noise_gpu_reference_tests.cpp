#include "src/detail/noise_math.hpp"
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
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

ui::FractalNoiseOptions options(ui::FractalNoiseMode mode,
                                std::uint8_t octaves = 4) {
    ui::FractalNoiseOptions value;
    value.set_mode(mode)
         .set_octaves(octaves)
         .set_lacunarity(2.0f)
         .set_gain(0.5f);
    return value;
}

double base_reference(ui::NoiseType base,
                      std::uint32_t seed,
                      double feature_size,
                      double x,
                      double y) {
    if (base == ui::NoiseType::Value) {
        return ui::detail::value_noise_reference(seed, feature_size, x, y);
    }
    if (base == ui::NoiseType::Perlin) {
        return ui::detail::perlin_noise_reference(seed, feature_size, x, y);
    }
    return ui::detail::simplex_noise_reference(seed, feature_size, x, y);
}

double fractal_reference(ui::NoiseType base,
                         ui::FractalNoiseMode mode,
                         double x,
                         double y) {
    constexpr std::uint32_t seed = 0x12345678u;
    constexpr double feature_size = 48.0;
    double frequency = 1.0;
    double amplitude = 1.0;
    double weight_sum = 0.0;
    double total = 0.0;
    for (int octave = 0; octave < 4; ++octave) {
        const double n = base_reference(
            base, seed, feature_size, x * frequency, y * frequency);
        const double s = 2.0 * n - 1.0;
        if (mode == ui::FractalNoiseMode::FBm) {
            total += amplitude * s;
        } else if (mode == ui::FractalNoiseMode::Turbulence) {
            total += amplitude * std::abs(s);
        } else {
            const double ridge = 1.0 - std::abs(s);
            total += amplitude * ridge * ridge;
        }
        weight_sum = weight_sum + amplitude;
        frequency = frequency * 2.0;
        amplitude = amplitude * 0.5;
    }
    const double result = mode == ui::FractalNoiseMode::FBm
        ? 0.5 + 0.5 * total / weight_sum
        : total / weight_sum;
    return std::clamp(result, 0.0, 1.0);
}

ui::UI make_ui(const ui::Brush& brush, float size = 64.0f) {
    return ui::UI{ui::Canvas{size, size,
        [brush, size](ui::CanvasContext2D& g) {
            g.fill_rect({0, 0, size, size}, brush);
        }}};
}

void compare(ui::Application& app,
             ui::NoiseType base,
             ui::FractalNoiseMode mode) {
    constexpr std::uint32_t seed = 0x12345678u;
    const auto source = ui::NoiseSource::create_fractal(
        base, {.feature_size = 48.0f, .seed = seed}, options(mode));
    check(source.ok(), "fractal GPU source did not compile");

    auto reference_ui = make_ui(source.noise.as_brush());
    ui::HeadlessRenderer reference{{64, 64}, 1.0f};
    check(reference.render(reference_ui), "fractal raster reference failed");

    auto gpu_ui = make_ui(source.noise.as_brush());
    ui::StandaloneWindow window{
        app, gpu_ui,
        ui::WindowDesc{.title = "NativeUI fractal noise GPU reference",
                       .size = {64, 64}, .resizable = false}};
    check(window.valid() && window.native_handle(),
          "fractal GPU window invalid");

    constexpr std::array<std::array<int, 2>, 4> samples{{
        {0, 0}, {8, 8}, {31, 23}, {52, 45}
    }};
    for (const auto& xy : samples) {
        const auto expected = reference.pixel(xy[0], xy[1]);
        const double cpu = fractal_reference(
            base, mode, double(xy[0]) + 0.5, double(xy[1]) + 0.5);
        check(std::abs(double(expected.r) / 255.0 - cpu) < 0.015,
              "fractal raster diverged from CPU oracle");

        check(ui::detail::PlatformTestAccess::request_gpu_readback(
                  window, ui::Point{float(xy[0]), float(xy[1])}),
              "fractal GPU readback rejected");
        std::optional<ui::detail::PlatformReadbackPixel> actual;
        for (int iteration = 0; iteration < 32 && !actual; ++iteration) {
            (void)app.poll(0.0);
            actual = ui::detail::PlatformTestAccess::take_gpu_readback(window);
        }
        check(actual.has_value(), "fractal GPU readback incomplete");
        check(std::abs(int(actual->r) - int(expected.r)) <= 5 &&
                  std::abs(int(actual->g) - int(expected.g)) <= 5 &&
                  std::abs(int(actual->b) - int(expected.b)) <= 5 &&
                  actual->a == expected.a,
              "fractal GPU pixel diverged from raster");
    }
}

double benchmark_window(const ui::Brush& brush) {
    ui::Application app;
    check(app.valid(), "benchmark application invalid");
    auto tree = make_ui(brush, 256.0f);
    ui::StandaloneWindow window{
        app, tree,
        ui::WindowDesc{.title = "NativeUI fractal noise benchmark",
                       .size = {256, 256}, .resizable = false}};
    check(window.valid(), "benchmark window invalid");
    const auto frame = [&] {
        check(ui::detail::PlatformTestAccess::request_gpu_readback(
                  window, {128, 128}),
              "benchmark readback rejected");
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
    const auto base = ui::NoiseSource::create(
        ui::NoiseType::Perlin,
        {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto fractal = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin,
        {.feature_size = 48.0f, .seed = 0x12345678u},
        options(ui::FractalNoiseMode::FBm, 6));
    check(base.ok() && fractal.ok(), "fractal benchmark source failed");
    const double base_ms = benchmark_window(base.noise.as_brush());
    const double fractal_ms = benchmark_window(fractal.noise.as_brush());
    std::cout << "Fractal noise warm 256x256 GPU redraw+readback median (5 runs): "
              << "base_perlin=" << base_ms
              << " ms, fractal6_perlin=" << fractal_ms
              << " ms, ratio=" << (fractal_ms / base_ms) << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            benchmark();
            return 0;
        }
        ui::Application app;
        check(app.valid(), "platform application invalid");
        app.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
        for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                          ui::NoiseType::Simplex}) {
            for (auto mode : {ui::FractalNoiseMode::FBm,
                              ui::FractalNoiseMode::Turbulence,
                              ui::FractalNoiseMode::Ridged}) {
                compare(app, base, mode);
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL fractal noise GPU reference: " << e.what() << '\n';
        return 1;
    }
}
