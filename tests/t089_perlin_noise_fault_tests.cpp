#include <nativeui/noise.hpp>
#include <nativeui/paint.hpp>

#include "src/detail/noise_test_seams.hpp"
#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error{message};
}

void paint(const ui::Brush& brush) {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    check(bool(surface), "raster surface unavailable");
    ui::Painter painter{*surface->getCanvas()};
    painter.fill_rounded_rect({0, 0, 8, 8}, 0.0f, brush);
    std::uint32_t pixels[64]{};
    check(surface->readPixels(SkImageInfo::MakeN32Premul(8, 8),
                              pixels, 32, 0, 0), "paint readback failed");
    check(pixels[4 * 8 + 4] != 0u, "paint was transparent after recovery");
}

void fault_and_recovery() {
    const auto before = ui::detail::shader_compile_call_count_for_test();
    ui::detail::set_noise_creation_failure_for_test(
        ui::detail::NoiseCreationFailurePoint::Compile);
    const auto failed = ui::NoiseSource::create(ui::NoiseType::Perlin);
    check(!failed.ok(), "injected compile failure succeeded");
    check(failed.error == ui::NoiseCreateError::BackendCompileFailed,
          "wrong compile failure result");
    check(!failed.diagnostic.empty(), "missing compiler diagnostic");
    check(ui::detail::ShaderBrushAccess::is_transparent_solid(
        failed.noise.as_brush()), "failed source was not inert");
    check(ui::detail::shader_compile_call_count_for_test() == before + 1,
          "compile failure did not exercise the compiler once");

    const auto recovered = ui::NoiseSource::create(ui::NoiseType::Perlin);
    check(recovered.ok(), "creation after compile failure did not recover");
    check(ui::detail::shader_compile_call_count_for_test() == before + 2,
          "successful creation did not compile exactly once");
    for (int i = 0; i < 3; ++i) paint(recovered.noise.as_brush());
    check(ui::detail::shader_compile_call_count_for_test() == before + 2,
          "as_brush or paint recompiled source");

    ui::detail::set_noise_creation_failure_for_test(
        ui::detail::NoiseCreationFailurePoint::AfterCompileAllocation);
    bool threw = false;
    try {
        static_cast<void>(ui::NoiseSource::create(ui::NoiseType::Perlin));
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "injected publication allocation did not throw");
    check(ui::detail::shader_compile_call_count_for_test() == before + 3,
          "allocation failure did not follow compilation");

    const auto after_allocation = ui::NoiseSource::create(ui::NoiseType::Perlin);
    check(after_allocation.ok(), "creation after allocation failure did not recover");
    paint(after_allocation.noise.as_brush());
    check(ui::detail::shader_compile_call_count_for_test() == before + 4,
          "recovery painted with an unexpected compilation count");
}

} // namespace

int main() {
    try {
        fault_and_recovery();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL T089 perlin noise fault: " << e.what() << '\n';
        return 1;
    }
}
