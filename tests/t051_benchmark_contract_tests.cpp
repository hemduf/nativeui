#include "benchmarks/t051_benchmark_harness.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    throw std::runtime_error("line " + std::to_string(line) + ": CHECK failed: " + expression);
}

#define NUI_CHECK(expr) ::check(static_cast<bool>(expr), #expr, __LINE__)

using nativeui::bench::ComparisonMetadata;
using nativeui::bench::RunMetrics;

void suite() {
    NUI_CHECK(nativeui::bench::kWarmupSamples == 5);
    NUI_CHECK(nativeui::bench::kMeasuredSamples == 30);

    std::vector<double> samples;
    samples.reserve(nativeui::bench::kMeasuredSamples);
    for (int i = 1; i <= nativeui::bench::kMeasuredSamples; ++i) {
        samples.push_back(static_cast<double>(i));
    }
    const auto summary = nativeui::bench::summarize_samples(samples);
    NUI_CHECK(summary.median_ns_per_op == 15.5);
    NUI_CHECK(summary.p95_ns_per_op == 29.0);
    NUI_CHECK(summary.min_ns_per_op == 1.0);
    NUI_CHECK(summary.max_ns_per_op == 30.0);

    const ComparisonMetadata linux_clang{
        .schema_version = 1,
        .workload_version = 1,
        .benchmark_name = "layout_small",
        .os = "Linux",
        .architecture = "x86_64",
        .compiler_id = "Clang",
        .compiler_major = 18,
        .build_type = "Release",
        .operations_per_sample = 100,
    };
    NUI_CHECK(nativeui::bench::metadata_compatible(linux_clang, linux_clang));

    auto incompatible = linux_clang;
    incompatible.compiler_major = 19;
    NUI_CHECK(!nativeui::bench::metadata_compatible(linux_clang, incompatible));
    incompatible = linux_clang;
    incompatible.operations_per_sample = 20;
    NUI_CHECK(!nativeui::bench::metadata_compatible(linux_clang, incompatible));

    const RunMetrics baseline{.median_ns_per_op = 100.0, .p95_ns_per_op = 100.0};

    // Exact 15% / 20% boundaries are not blocking because the policy is
    // strictly greater-than on both metrics and requires two reproductions.
    const RunMetrics exact_boundary{.median_ns_per_op = 115.0, .p95_ns_per_op = 120.0};
    NUI_CHECK(!nativeui::bench::is_timing_regression(baseline, exact_boundary));

    const RunMetrics median_only{.median_ns_per_op = 115.01, .p95_ns_per_op = 120.0};
    NUI_CHECK(!nativeui::bench::is_timing_regression(baseline, median_only));

    const RunMetrics both_above{.median_ns_per_op = 115.01, .p95_ns_per_op = 120.01};
    NUI_CHECK(nativeui::bench::is_timing_regression(baseline, both_above));

    NUI_CHECK(!nativeui::bench::blocking_after_two_runs(baseline, both_above, exact_boundary));
    NUI_CHECK(nativeui::bench::blocking_after_two_runs(baseline, both_above, both_above));

    NUI_CHECK(nativeui::bench::allocation_regression(0.0, 0.001, true));
    NUI_CHECK(!nativeui::bench::allocation_regression(10.0, 11.0, false));
    NUI_CHECK(nativeui::bench::allocation_regression(10.0, 11.01, false));

    NUI_CHECK(nativeui::bench::idle_invalidation_passes(0));
    NUI_CHECK(!nativeui::bench::idle_invalidation_passes(1));
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t051 benchmark contract\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t051 benchmark contract: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
