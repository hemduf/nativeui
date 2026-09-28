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
        ui::WindowDesc{.title = "NativeUI T091 benchmark",
                       .size = {256, 256}, .resizable = false}};
    check(window.valid(), "benchmark window invalid");
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
    const auto perlin = ui::NoiseSource::create(
        ui::NoiseType::Perlin, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto simplex = ui::NoiseSource::create(
        ui::NoiseType::Simplex, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto f1 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF1, {.feature_size = 48.0f, .seed = 0x12345678u});
    const auto f2 = ui::NoiseSource::create(
        ui::NoiseType::WorleyF2, {.feature_size = 48.0f, .seed = 0x12345678u});
    check(perlin.ok() && simplex.ok() && f1.ok() && f2.ok(),
          "benchmark source failed to compile");
    std::cout << "T091 warm 256x256 GPU redraw+readback median (5 runs): perlin="
              << benchmark_window(perlin.noise.as_brush())
              << " ms, simplex=" << benchmark_window(simplex.noise.as_brush())
              << " ms, worley_f1=" << benchmark_window(f1.noise.as_brush())
              << " ms, worley_f2=" << benchmark_window(f2.noise.as_brush())
              << " ms\n";
}

void compare_type(ui::Application& app, ui::NoiseType type, bool second) {
    constexpr std::uint32_t seed = 0x12345678u;
    const auto source = ui::NoiseSource::create(
        type, {.feature_size = 48.0f, .seed = seed});
    check(source.ok(), "Worley source did not compile");
    const auto brush = source.noise.as_brush();

    auto reference_ui = make_ui(brush);
    ui::HeadlessRenderer reference{{64, 64}, 1.0f};
    check(reference.render(reference_ui), "Worley raster reference failed");

    auto gpu_ui = make_ui(brush);
    ui::StandaloneWindow window{
        app, gpu_ui,
        ui::WindowDesc{.title = second ? "NativeUI T091 Worley F2"
                                       : "NativeUI T091 Worley F1",
                       .size = {64, 64}, .resizable = false}};
    check(window.valid() && window.native_handle(), "Worley GPU window invalid");

    constexpr std::array<std::array<int, 2>, 4> samples{{
        {0, 0}, {8, 8}, {31, 23}, {52, 45}
    }};
    for (const auto& xy : samples) {
        const auto expected = reference.pixel(xy[0], xy[1]);
        const auto cpu = ui::detail::worley_noise_reference(
            seed, 48.0, double(xy[0]) + 0.5, double(xy[1]) + 0.5);
        const double scalar = second ? cpu.f2 : cpu.f1;
        check(std::abs(double(expected.r) / 255.0 - scalar) < 0.015,
              "Worley raster diverged from CPU oracle");

        check(ui::detail::PlatformTestAccess::request_gpu_readback(
            window, ui::Point{float(xy[0]), float(xy[1])}),
            "Worley GPU readback rejected");
        std::optional<ui::detail::PlatformReadbackPixel> actual;
        for (int iteration = 0; iteration < 32 && !actual; ++iteration) {
            (void)app.poll(0.0);
            actual = ui::detail::PlatformTestAccess::take_gpu_readback(window);
        }
        check(actual.has_value(), "Worley GPU readback incomplete");
        check(std::abs(int(actual->r) - int(expected.r)) <= 5 &&
                  std::abs(int(actual->g) - int(expected.g)) <= 5 &&
                  std::abs(int(actual->b) - int(expected.b)) <= 5 &&
                  actual->a == expected.a,
              "Worley GPU pixel diverged from raster");
    }
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
        compare_type(app, ui::NoiseType::WorleyF1, false);
        compare_type(app, ui::NoiseType::WorleyF2, true);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T091 Worley GPU reference: " << e.what() << '\n';
        return 1;
    }
}
