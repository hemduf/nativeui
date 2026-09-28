#include "benchmarks/t051_benchmark_harness.hpp"
#include "src/detail/render_resource_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

struct SyntheticResource final {
    std::uint64_t value{};
};

using Cache = ui::detail::RenderResourceCache<std::uint64_t, SyntheticResource>;

constexpr std::uint64_t kWarmOperationsPerSample = 20000;
constexpr std::uint64_t kColdOperationsPerSample = 1000;
constexpr std::size_t kAccountedBytesPerResource = 64;

struct BenchmarkPair final {
    nativeui::bench::ProtocolRun warm;
    nativeui::bench::ProtocolRun cold;
    std::uint64_t warm_factory_calls{};
    std::uint64_t cold_factory_calls{};
    std::uint64_t checksum{};
};

BenchmarkPair run_benchmarks() {
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

} // namespace

int main() {
    try {
        const auto result = run_benchmarks();
        const auto warm_median = result.warm.summary.median_ns_per_op;
        const auto cold_median = result.cold.summary.median_ns_per_op;
        const auto ratio = warm_median > 0.0 ? cold_median / warm_median : 0.0;

        std::cout
            << "render_resource_cache"
            << " warm_median_ns=" << warm_median
            << " warm_p95_ns=" << result.warm.summary.p95_ns_per_op
            << " cold_median_ns=" << cold_median
            << " cold_p95_ns=" << result.cold.summary.p95_ns_per_op
            << " cold_to_warm_median_ratio=" << ratio
            << " warm_factory_calls=" << result.warm_factory_calls
            << " cold_factory_calls=" << result.cold_factory_calls
            << " checksum=" << result.checksum
            << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "render resource cache benchmark failure: "
                  << error.what() << '\n';
        return 1;
    }
}
