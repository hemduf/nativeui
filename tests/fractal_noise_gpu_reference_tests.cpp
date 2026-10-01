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
#include <string>
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
                         std::uint32_t seed,
                         double x,
                         double y) {
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

struct GpuComparisonSummary {
    int max_red_delta = 0;
    int max_green_delta = 0;
    int max_blue_delta = 0;
    int max_alpha_delta = 0;
    std::size_t sample_count = 0;
    std::size_t mismatch_count = 0;
};

void compare(ui::Application& app,
             std::uint32_t seed,
             ui::NoiseType base,
             ui::FractalNoiseMode mode,
             GpuComparisonSummary& summary) {
    constexpr float logical_size = 64.0f;
    const auto source = ui::NoiseSource::create_fractal(
        base, {.feature_size = 48.0f, .seed = seed}, options(mode));
    check(source.ok(), "fractal GPU source did not compile");

    auto gpu_ui = make_ui(source.noise.as_brush());
    ui::StandaloneWindow window{
        app, gpu_ui,
        ui::WindowDesc{.title = "NativeUI fractal noise GPU reference",
                       .size = {logical_size, logical_size}, .resizable = false}};
    check(window.valid() && window.native_handle(),
          "fractal GPU window invalid");

    // Wait for the initial native expose/configure before reading the scale.
    // The renderer and request_gpu_readback() both use geometry_.last_valid_scale(),
    // so a completed initial scene is the stable synchronization point. A readback
    // is deliberately not used as the probe: on macOS the first readback can be
    // requested before the initial drawable is ready and then never complete.
    check(ui::detail::PlatformTestAccess::request_expose(window),
          "fractal GPU initial expose request rejected");
    ui::detail::SceneDiagnostics diagnostics;
    for (int iteration = 0; iteration < 100 && !diagnostics.scene_valid;
         ++iteration) {
        (void)app.poll(0.01);
        diagnostics =
            ui::detail::PlatformTestAccess::scene_diagnostics(window);
    }
    check(diagnostics.scene_valid,
          "fractal GPU initial scene did not become valid");

    const float scale = window.scale_factor();
    check(std::isfinite(scale) && scale > 0.0f,
          "fractal GPU window scale is invalid");

    // Native window extents use outward rounding, so the scale oracle must
    // mirror production rather than nearest-integer sample-point rounding.
    const int expected_scene_extent =
        static_cast<int>(std::ceil(logical_size * scale));
    check(diagnostics.scene_width == expected_scene_extent &&
              diagnostics.scene_height == expected_scene_extent,
          "fractal GPU scene extent does not match window scale");

    auto reference_ui = make_ui(source.noise.as_brush());
    ui::HeadlessRenderer reference{{logical_size, logical_size}, scale};
    check(reference.render(reference_ui), "fractal raster reference failed");

    constexpr std::array<std::array<int, 2>, 4> samples{{
        {0, 0}, {8, 8}, {31, 23}, {52, 45}
    }};
    for (const auto& xy : samples) {
        const int physical_x =
            static_cast<int>(std::lround(float(xy[0]) * scale));
        const int physical_y =
            static_cast<int>(std::lround(float(xy[1]) * scale));
        const auto expected = reference.pixel(physical_x, physical_y);
        const double sample_x = (double(physical_x) + 0.5) / double(scale);
        const double sample_y = (double(physical_y) + 0.5) / double(scale);
        const double cpu = fractal_reference(base, mode, seed, sample_x, sample_y);
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

        const int dr = int(actual->r) - int(expected.r);
        const int dg = int(actual->g) - int(expected.g);
        const int db = int(actual->b) - int(expected.b);
        const int da = int(actual->a) - int(expected.a);
        ++summary.sample_count;
        summary.max_red_delta = std::max(summary.max_red_delta, std::abs(dr));
        summary.max_green_delta =
            std::max(summary.max_green_delta, std::abs(dg));
        summary.max_blue_delta =
            std::max(summary.max_blue_delta, std::abs(db));
        summary.max_alpha_delta =
            std::max(summary.max_alpha_delta, std::abs(da));

        if (std::abs(dr) > 5 || std::abs(dg) > 5 || std::abs(db) > 5 ||
            da != 0) {
            ++summary.mismatch_count;
            std::cerr
                << "fractal GPU mismatch: seed=" << seed
                << " base=" << static_cast<int>(base)
                << " mode=" << static_cast<int>(mode)
                << " logical=(" << xy[0] << "," << xy[1] << ")"
                << " physical=(" << physical_x << "," << physical_y << ")"
                << " scale=" << scale
                << " expected=(" << int(expected.r) << "," << int(expected.g)
                << "," << int(expected.b) << "," << int(expected.a) << ")"
                << " actual=(" << int(actual->r) << "," << int(actual->g)
                << "," << int(actual->b) << "," << int(actual->a) << ")"
                << " cpu=" << cpu
                << " raster_error="
                << std::abs(double(expected.r) / 255.0 - cpu)
                << " delta=(" << dr << "," << dg << "," << db << "," << da
                << ")\n";
        }
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
        GpuComparisonSummary summary;
        constexpr std::array<std::uint32_t, 2> seeds{
            0x12345678u, 0xA5A5A5A5u
        };
        for (const auto seed : seeds) {
            for (auto base : {ui::NoiseType::Value, ui::NoiseType::Perlin,
                              ui::NoiseType::Simplex}) {
                for (auto mode : {ui::FractalNoiseMode::FBm,
                                  ui::FractalNoiseMode::Turbulence,
                                  ui::FractalNoiseMode::Ridged}) {
                    compare(app, seed, base, mode, summary);
                }
            }
        }
        std::cout << "Fractal GPU delta maxima: red=" << summary.max_red_delta
                  << " green=" << summary.max_green_delta
                  << " blue=" << summary.max_blue_delta
                  << " alpha=" << summary.max_alpha_delta
                  << " samples=" << summary.sample_count
                  << " mismatches=" << summary.mismatch_count << '\n';
        if (summary.mismatch_count != 0) {
            throw std::runtime_error{
                "fractal GPU pixels diverged from scale-matched raster"};
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL fractal noise GPU reference: " << e.what() << '\n';
        return 1;
    }
}
