#include "benchmarks/t051_benchmark_comparison.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    throw std::runtime_error("line " + std::to_string(line) + ": CHECK failed: " + expression);
}

#define NUI_CHECK(expr) ::check(static_cast<bool>(expr), #expr, __LINE__)

nativeui::bench::ResultRecord make_record(double median,
                                          double p95,
                                          double allocations,
                                          double bytes) {
    return nativeui::bench::ResultRecord{
        .metadata = nativeui::bench::ComparisonMetadata{
            .schema_version = 1,
            .workload_version = 1,
            .benchmark_name = "layout_small",
            .os = "Linux",
            .architecture = "x86_64",
            .compiler_id = "Clang",
            .compiler_major = 18,
            .compiler_version = "18.1.2",
            .build_type = "Release",
            .operations_per_sample = 100,
            .commit_sha = "candidate",
        },
        .node_count = 116,
        .actions_per_operation = 1,
        .logical_width = 1024,
        .logical_height = 768,
        .warmup_samples = 5,
        .measured_samples = 30,
        .timing = {.median_ns_per_op = median,
                   .p95_ns_per_op = p95,
                   .min_ns_per_op = median,
                   .max_ns_per_op = p95},
        .allocation_metrics_available = true,
        .allocations_per_op = allocations,
        .bytes_allocated_per_op = bytes,
    };
}

void suite() {
    auto baseline = make_record(100.0, 100.0, 10.0, 100.0);
    baseline.metadata.commit_sha = "baseline";
    const auto first = make_record(116.0, 121.0, 11.1, 100.0);
    const auto second = make_record(117.0, 122.0, 10.0, 111.0);

    const auto decision = nativeui::bench::compare_two_complete_runs(
        baseline, first, second, false);
    NUI_CHECK(decision.timing_blocking);
    NUI_CHECK(decision.allocation_metrics_comparable);
    NUI_CHECK(decision.allocation_blocking);
    NUI_CHECK(decision.blocking());

    auto incompatible = second;
    incompatible.metadata.compiler_major = 19;
    bool rejected_incompatible_metadata = false;
    try {
        (void)nativeui::bench::compare_two_complete_runs(
            baseline, first, incompatible, false);
    } catch (const std::invalid_argument&) {
        rejected_incompatible_metadata = true;
    }
    NUI_CHECK(rejected_incompatible_metadata);

    auto unavailable = second;
    unavailable.allocation_metrics_available = false;
    const auto timing_only = nativeui::bench::compare_two_complete_runs(
        baseline, first, unavailable, false);
    NUI_CHECK(timing_only.timing_blocking);
    NUI_CHECK(!timing_only.allocation_metrics_comparable);
    NUI_CHECK(!timing_only.allocation_blocking);
    NUI_CHECK(timing_only.blocking());
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t051 comparison contract\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t051 comparison contract: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
