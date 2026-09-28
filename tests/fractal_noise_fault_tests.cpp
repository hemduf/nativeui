#include <nativeui/noise.hpp>
#include <nativeui/paint.hpp>

#include "src/detail/noise_test_seams.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
std::atomic_size_t allocation_count{0};
}

void* operator new(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

ui::FractalNoiseOptions options() {
    ui::FractalNoiseOptions value;
    value.set_octaves(4)
         .set_lacunarity(2.0f)
         .set_gain(0.5f)
         .set_mode(ui::FractalNoiseMode::FBm);
    return value;
}

void paint(const ui::Brush& brush) {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    check(bool(surface), "raster surface unavailable");
    ui::Painter painter{*surface->getCanvas()};
    painter.fill_rounded_rect({0, 0, 8, 8}, 0.0f, brush);
    std::uint32_t pixels[64]{};
    check(surface->readPixels(SkImageInfo::MakeN32Premul(8, 8),
                              pixels, 32, 0, 0),
          "paint readback failed");
    check(pixels[4 * 8 + 4] != 0u, "paint was transparent after recovery");
}

void value_only_options_contract() {
    ui::FractalNoiseOptions value;
    const auto allocations_before =
        allocation_count.load(std::memory_order_relaxed);
    const auto compiles_before =
        ui::detail::shader_compile_call_count_for_test();
    value.set_octaves(6)
         .set_lacunarity(4.0f)
         .set_gain(1.0f)
         .set_mode(ui::FractalNoiseMode::Ridged);
    check(allocation_count.load(std::memory_order_relaxed) ==
              allocations_before,
          "FractalNoiseOptions setter allocated");
    check(ui::detail::shader_compile_call_count_for_test() == compiles_before,
          "FractalNoiseOptions setter compiled source");

    const auto allocations_after_setters =
        allocation_count.load(std::memory_order_relaxed);
    const auto compiles_after_setters =
        ui::detail::shader_compile_call_count_for_test();
    check(value.octaves() == 6 && value.lacunarity() == 4.0f &&
              value.gain() == 1.0f &&
              value.mode() == ui::FractalNoiseMode::Ridged,
          "FractalNoiseOptions getter changed stored values");
    check(allocation_count.load(std::memory_order_relaxed) ==
              allocations_after_setters,
          "FractalNoiseOptions getter allocated");
    check(ui::detail::shader_compile_call_count_for_test() ==
              compiles_after_setters,
          "FractalNoiseOptions getter compiled source");
}

void compile_count_is_independent_of_octaves() {
    const auto before = ui::detail::shader_compile_call_count_for_test();

    auto one = options();
    one.set_octaves(1);
    const auto one_result = ui::NoiseSource::create_fractal(
        ui::NoiseType::Value, {}, one);
    check(one_result.ok(), "one-octave fractal creation failed");
    check(ui::detail::shader_compile_call_count_for_test() == before + 1,
          "one-octave fractal did not compile exactly once");

    auto six = options();
    six.set_octaves(6);
    const auto six_result = ui::NoiseSource::create_fractal(
        ui::NoiseType::Value, {}, six);
    check(six_result.ok(), "six-octave fractal creation failed");
    check(ui::detail::shader_compile_call_count_for_test() == before + 2,
          "six-octave fractal did not compile exactly once");
}

void invalid_does_not_compile() {
    const auto before = ui::detail::shader_compile_call_count_for_test();
    auto invalid = options();
    invalid.set_octaves(0);
    const auto result = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, {}, invalid);
    check(!result.ok(), "invalid fractal options succeeded");
    check(result.error == ui::NoiseCreateError::InvalidArgument,
          "invalid fractal options returned wrong error");
    check(ui::detail::shader_compile_call_count_for_test() == before,
          "invalid fractal options reached compiler");
}

void fault_and_recovery() {
    const auto before = ui::detail::shader_compile_call_count_for_test();

    ui::detail::set_noise_creation_failure_for_test(
        ui::detail::NoiseCreationFailurePoint::Compile);
    const auto failed = ui::NoiseSource::create_fractal(
        ui::NoiseType::Simplex, {}, options());
    check(!failed.ok(), "injected fractal compile failure succeeded");
    check(failed.error == ui::NoiseCreateError::BackendCompileFailed,
          "wrong fractal compile failure result");
    check(!failed.diagnostic.empty(), "missing fractal compiler diagnostic");
    check(ui::detail::ShaderBrushAccess::is_transparent_solid(
              failed.noise.as_brush()),
          "failed fractal source was not inert");
    check(ui::detail::shader_compile_call_count_for_test() == before + 1,
          "fractal compile failure did not compile exactly once");

    const auto recovered = ui::NoiseSource::create_fractal(
        ui::NoiseType::Simplex, {}, options());
    check(recovered.ok(), "fractal creation did not recover");
    check(ui::detail::shader_compile_call_count_for_test() == before + 2,
          "successful fractal creation did not compile exactly once");
    for (int i = 0; i < 3; ++i) paint(recovered.noise.as_brush());
    check(ui::detail::shader_compile_call_count_for_test() == before + 2,
          "fractal as_brush or paint recompiled source");

    ui::detail::set_noise_creation_failure_for_test(
        ui::detail::NoiseCreationFailurePoint::AfterCompileAllocation);
    bool threw = false;
    try {
        static_cast<void>(ui::NoiseSource::create_fractal(
            ui::NoiseType::Perlin, {}, options()));
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "injected fractal publication allocation did not throw");
    check(ui::detail::shader_compile_call_count_for_test() == before + 3,
          "fractal allocation failure did not follow compilation");

    const auto after_allocation = ui::NoiseSource::create_fractal(
        ui::NoiseType::Perlin, {}, options());
    check(after_allocation.ok(),
          "fractal creation after allocation failure did not recover");
    paint(after_allocation.noise.as_brush());
    check(ui::detail::shader_compile_call_count_for_test() == before + 4,
          "fractal recovery used unexpected compile count");
}

} // namespace

int main() {
    try {
        value_only_options_contract();
        compile_count_is_independent_of_octaves();
        invalid_does_not_compile();
        fault_and_recovery();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL fractal noise fault: " << e.what() << '\n';
        return 1;
    }
}
