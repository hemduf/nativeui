#include "benchmarks/t051_benchmark_harness.hpp"
#include "src/detail/painter_private_hooks.hpp"
#include "src/detail/render_resource_cache.hpp"
#include "src/detail/render_resource_materialization.hpp"
#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace ui {

struct TreeTestAccess {
    static void paint_with_resources(
        Tree& tree,
        SkCanvas& canvas,
        PlatformServices& platform,
        const detail::PainterPrivateHooks* hooks) {
        tree.paint_with_resources(canvas, platform, hooks);
    }
};

} // namespace ui

namespace {

struct SyntheticResource final {
    std::uint64_t value{};
};

using Cache = ui::detail::RenderResourceCache<std::uint64_t, SyntheticResource>;

constexpr std::uint64_t kWarmOperationsPerSample = 20000;
constexpr std::uint64_t kColdOperationsPerSample = 1000;
constexpr std::uint64_t kPaintOperationsPerSample = 100;
constexpr std::size_t kAccountedBytesPerResource = 64;

struct BenchmarkPair final {
    nativeui::bench::ProtocolRun warm;
    nativeui::bench::ProtocolRun cold;
    std::uint64_t warm_factory_calls{};
    std::uint64_t cold_factory_calls{};
    std::uint64_t checksum{};
};

BenchmarkPair run_cache_core_benchmarks() {
    Cache warm_cache{{
        .max_entries = 512,
        .max_accounted_bytes = 128U * 1024U * 1024U,
    }};

    std::uint64_t warm_factory_calls = 0;
    {
        auto seeded = warm_cache.acquire(
            7U,
            kAccountedBytesPerResource,
            [&] {
                ++warm_factory_calls;
                return std::make_shared<SyntheticResource>(SyntheticResource{7U});
            });
        if (!seeded || seeded.hit || !seeded.retained) {
            throw std::runtime_error("failed to seed retained warm resource");
        }
    }

    std::uint64_t checksum = 0;
    const auto warm = nativeui::bench::run_fixed_protocol(
        kWarmOperationsPerSample,
        [] {},
        [&] {
            const auto acquired = warm_cache.acquire(
                7U,
                kAccountedBytesPerResource,
                [&] {
                    ++warm_factory_calls;
                    return std::make_shared<SyntheticResource>(SyntheticResource{7U});
                });
            if (acquired && acquired.hit && acquired.retained) {
                checksum += acquired.resource->value;
            }
        });

    if (warm_factory_calls != 1U) {
        throw std::runtime_error("warm retained lookup recreated its resource");
    }

    Cache cold_cache{{
        .max_entries = 64,
        .max_accounted_bytes = 64U * kAccountedBytesPerResource,
    }};
    std::uint64_t cold_factory_calls = 0;
    std::uint64_t next_key = 0;

    const auto cold = nativeui::bench::run_fixed_protocol(
        kColdOperationsPerSample,
        [&] {
            cold_cache.clear();
            next_key = 0;
        },
        [&] {
            const auto key = next_key++;
            const auto acquired = cold_cache.acquire(
                key,
                kAccountedBytesPerResource,
                [&] {
                    ++cold_factory_calls;
                    return std::make_shared<SyntheticResource>(SyntheticResource{key});
                });
            if (acquired) checksum += acquired.resource->value;
        });

    const auto expected_cold_factory_calls =
        static_cast<std::uint64_t>(
            nativeui::bench::kWarmupSamples + nativeui::bench::kMeasuredSamples) *
        kColdOperationsPerSample;
    if (cold_factory_calls != expected_cold_factory_calls) {
        throw std::runtime_error("cold benchmark unexpectedly reused a retained resource");
    }

    return {
        .warm = warm,
        .cold = cold,
        .warm_factory_calls = warm_factory_calls,
        .cold_factory_calls = cold_factory_calls,
        .checksum = checksum,
    };
}

struct PaintHookState final {
    ui::detail::RenderResourceMaterializationContext resources;
    std::uint64_t shader_materializations{};
};

[[nodiscard]] sk_sp<SkShader> cached_shader_hook(
    void* opaque,
    const std::shared_ptr<const ui::detail::ShaderBrushSnapshot>& snapshot) {
    auto& state = *static_cast<PaintHookState*>(opaque);
    auto acquisition = state.resources.acquire_runtime_shader(
        snapshot,
        [&] {
            ++state.shader_materializations;
            return ui::detail::materialize_shader_brush(snapshot);
        });
    return acquisition ? std::move(acquisition.shader) : sk_sp<SkShader>{};
}

[[nodiscard]] sk_sp<SkShader> uncached_shader_hook(
    void* opaque,
    const std::shared_ptr<const ui::detail::ShaderBrushSnapshot>& snapshot) {
    auto& state = *static_cast<PaintHookState*>(opaque);
    ++state.shader_materializations;
    return ui::detail::materialize_shader_brush(snapshot);
}

[[nodiscard]] ui::Brush make_benchmark_shader_brush() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2 p) {
            return half4(gain, p.x * 0.001, p.y * 0.001, 1.0);
        }
    )");
    if (!compiled.ok()) {
        throw std::runtime_error("paint benchmark shader compilation failed");
    }

    ui::ShaderInstance instance{compiled.program};
    if (instance.set_float("gain", 0.75f) != ui::ShaderSetResult::Ok) {
        throw std::runtime_error("paint benchmark shader binding failed");
    }
    return ui::Brush{instance};
}

[[nodiscard]] std::unique_ptr<ui::Tree> make_shader_tree(const ui::Brush& brush) {
    auto tree = std::make_unique<ui::Tree>(
        ui::compile(ui::make_spec(ui::Canvas{
            32.0f, 32.0f, [brush](ui::CanvasContext2D& canvas) {
                canvas.fill_rect({0.0f, 0.0f, 32.0f, 32.0f}, brush);
            }})));
    tree->mount();
    tree->layout({32.0f, 32.0f});
    return tree;
}

struct PaintBenchmarkPair final {
    nativeui::bench::ProtocolRun warm;
    nativeui::bench::ProtocolRun uncached;
    std::uint64_t warm_materializations{};
    std::uint64_t uncached_materializations{};
};

PaintBenchmarkPair run_relative_paint_benchmarks() {
    const auto brush = make_benchmark_shader_brush();
    auto warm_tree = make_shader_tree(brush);
    auto uncached_tree = make_shader_tree(brush);

    const auto info = SkImageInfo::MakeN32Premul(32, 32);
    auto warm_surface = SkSurfaces::Raster(info);
    auto uncached_surface = SkSurfaces::Raster(info);
    if (!warm_surface || !uncached_surface) {
        throw std::runtime_error("paint benchmark raster surface creation failed");
    }

    test::MockPlatform warm_platform;
    test::MockPlatform uncached_platform;
    PaintHookState warm_state;
    PaintHookState uncached_state;

    const ui::detail::PainterPrivateHooks warm_hooks{
        &warm_state,
        nullptr,
        &cached_shader_hook};
    const ui::detail::PainterPrivateHooks uncached_hooks{
        &uncached_state,
        nullptr,
        &uncached_shader_hook};

    // Seed the retained materialization outside the timed region. Every timed
    // warm operation must then exercise the normal Tree/Painter path while
    // reusing that exact backend shader.
    warm_state.resources.begin_frame();
    ui::TreeTestAccess::paint_with_resources(
        *warm_tree, *warm_surface->getCanvas(), warm_platform, &warm_hooks);
    warm_state.resources.end_frame();
    if (warm_state.shader_materializations != 1U ||
        warm_state.resources.retained_entries() != 1U) {
        throw std::runtime_error("paint benchmark failed to seed one retained shader");
    }

    const auto warm = nativeui::bench::run_fixed_protocol(
        kPaintOperationsPerSample,
        [] {},
        [&] {
            warm_tree->invalidate();
            warm_state.resources.begin_frame();
            ui::TreeTestAccess::paint_with_resources(
                *warm_tree, *warm_surface->getCanvas(), warm_platform, &warm_hooks);
            warm_state.resources.end_frame();
        });

    if (warm_state.shader_materializations != 1U) {
        throw std::runtime_error("warm paint path recreated its retained shader");
    }

    const auto uncached = nativeui::bench::run_fixed_protocol(
        kPaintOperationsPerSample,
        [] {},
        [&] {
            uncached_tree->invalidate();
            ui::TreeTestAccess::paint_with_resources(
                *uncached_tree,
                *uncached_surface->getCanvas(),
                uncached_platform,
                &uncached_hooks);
        });

    const auto expected_uncached_materializations =
        static_cast<std::uint64_t>(
            nativeui::bench::kWarmupSamples + nativeui::bench::kMeasuredSamples) *
        kPaintOperationsPerSample;
    if (uncached_state.shader_materializations !=
        expected_uncached_materializations) {
        throw std::runtime_error(
            "uncached paint benchmark did not materialize exactly once per paint");
    }

    return {
        .warm = warm,
        .uncached = uncached,
        .warm_materializations = warm_state.shader_materializations,
        .uncached_materializations = uncached_state.shader_materializations,
    };
}

void print_pair(const char* name,
                const BenchmarkPair& result) {
    const auto warm_median = result.warm.summary.median_ns_per_op;
    const auto cold_median = result.cold.summary.median_ns_per_op;
    const auto ratio = warm_median > 0.0 ? cold_median / warm_median : 0.0;

    std::cout
        << name
        << " warm_median_ns=" << warm_median
        << " warm_p95_ns=" << result.warm.summary.p95_ns_per_op
        << " cold_median_ns=" << cold_median
        << " cold_p95_ns=" << result.cold.summary.p95_ns_per_op
        << " cold_to_warm_median_ratio=" << ratio
        << " warm_factory_calls=" << result.warm_factory_calls
        << " cold_factory_calls=" << result.cold_factory_calls
        << " checksum=" << result.checksum
        << '\n';
}

void print_paint_pair(const PaintBenchmarkPair& result) {
    const auto warm_median = result.warm.summary.median_ns_per_op;
    const auto uncached_median = result.uncached.summary.median_ns_per_op;
    const auto ratio =
        warm_median > 0.0 ? uncached_median / warm_median : 0.0;

    std::cout
        << "render_resource_paint"
        << " warm_median_ns=" << warm_median
        << " warm_p95_ns=" << result.warm.summary.p95_ns_per_op
        << " uncached_median_ns=" << uncached_median
        << " uncached_p95_ns=" << result.uncached.summary.p95_ns_per_op
        << " uncached_to_warm_median_ratio=" << ratio
        << " warm_shader_materializations=" << result.warm_materializations
        << " uncached_shader_materializations=" << result.uncached_materializations
        << '\n';
}

} // namespace

int main() {
    try {
        print_pair("render_resource_cache", run_cache_core_benchmarks());
        print_paint_pair(run_relative_paint_benchmarks());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "render resource cache benchmark failure: "
                  << error.what() << '\n';
        return 1;
    }
}
