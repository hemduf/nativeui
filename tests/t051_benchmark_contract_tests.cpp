#include "benchmarks/t051_benchmark_harness.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    throw std::runtime_error("line " + std::to_string(line) + ": CHECK failed: " + expression);
}

#define NUI_CHECK(expr) ::check(static_cast<bool>(expr), #expr, __LINE__)

using nativeui::bench::AllocationMetrics;
using nativeui::bench::ComparisonMetadata;
using nativeui::bench::ResultRecord;
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

    bool rejected_wrong_sample_count = false;
    try {
        (void)nativeui::bench::summarize_samples(std::vector<double>(29, 1.0));
    } catch (const std::invalid_argument&) {
        rejected_wrong_sample_count = true;
    }
    NUI_CHECK(rejected_wrong_sample_count);

    std::uint64_t protocol_operations = 0;
    int protocol_sample_setups = 0;
    const auto protocol = nativeui::bench::run_fixed_protocol(
        7,
        [&] { ++protocol_sample_setups; },
        [&] { ++protocol_operations; });
    NUI_CHECK(protocol_sample_setups == 35);
    NUI_CHECK(protocol_operations == 35u * 7u);
    NUI_CHECK(protocol.measured_ns_per_op.size() == 30);
    NUI_CHECK(protocol.summary.min_ns_per_op >= 0.0);
    NUI_CHECK(protocol.summary.max_ns_per_op >= protocol.summary.min_ns_per_op);

    struct ExpectedWorkload {
        std::string_view name;
        std::uint64_t operations;
        bool timed;
    };
    constexpr ExpectedWorkload expected[] = {
        {"layout_small", 100, true},
        {"layout_large", 20, true},
        {"hit_test_deep", 2000, true},
        {"dispatch_pointer", 2000, true},
        {"dispatch_keyboard", 1000, true},
        {"text_edit_short", 500, true},
        {"text_edit_multiline", 100, true},
        {"paint_controls", 20, true},
        {"paint_text", 50, true},
        {"multi_instance", 20, true},
        {"idle_invalidation", 1000, false},
    };
    NUI_CHECK(nativeui::bench::kFixedWorkloads.size() == std::size(expected));
    for (const auto& item : expected) {
        const auto* workload = nativeui::bench::find_workload(item.name);
        NUI_CHECK(workload != nullptr);
        NUI_CHECK(workload->operations_per_sample == item.operations);
        NUI_CHECK(workload->timed == item.timed);
    }
    NUI_CHECK(nativeui::bench::find_workload("not-a-workload") == nullptr);

    const ComparisonMetadata linux_clang{
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
        .commit_sha = "0123456789abcdef",
    };
    NUI_CHECK(nativeui::bench::metadata_compatible(linux_clang, linux_clang));

    auto incompatible = linux_clang;
    incompatible.compiler_major = 19;
    NUI_CHECK(!nativeui::bench::metadata_compatible(linux_clang, incompatible));
    incompatible = linux_clang;
    incompatible.operations_per_sample = 20;
    NUI_CHECK(!nativeui::bench::metadata_compatible(linux_clang, incompatible));
    incompatible = linux_clang;
    incompatible.workload_version = 2;
    NUI_CHECK(!nativeui::bench::metadata_compatible(linux_clang, incompatible));
    incompatible = linux_clang;
    incompatible.commit_sha = "different-commit";
    NUI_CHECK(nativeui::bench::metadata_compatible(linux_clang, incompatible));

    const ResultRecord record{
        .metadata = linux_clang,
        .node_count = 116,
        .actions_per_operation = 0,
        .logical_width = 1024,
        .logical_height = 768,
        .warmup_samples = 5,
        .measured_samples = 30,
        .timing = {.median_ns_per_op = 12.5,
                   .p95_ns_per_op = 18.75,
                   .min_ns_per_op = 10.25,
                   .max_ns_per_op = 21.5},
        .allocation_metrics_available = true,
        .allocations_per_op = 0.125,
        .bytes_allocated_per_op = 32.0,
    };
    const auto json = nativeui::bench::to_json(record);
    NUI_CHECK(json.find("\"schema_version\":1") != std::string::npos);
    NUI_CHECK(json.find("\"benchmark_name\":\"layout_small\"") != std::string::npos);
    const auto parsed = nativeui::bench::parse_result_json(json);
    NUI_CHECK(parsed == record);

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

    // Allocation policy is also a two-complete-run gate. For non-zero
    // scenarios either allocation count or allocated bytes above 10% is a
    // regression; the exact 10% boundary remains non-blocking.
    const AllocationMetrics alloc_baseline{.allocations_per_op = 10.0, .bytes_per_op = 100.0};
    const AllocationMetrics alloc_exact_boundary{.allocations_per_op = 11.0,
                                                  .bytes_per_op = 110.0};
    const AllocationMetrics alloc_count_above{.allocations_per_op = 11.01,
                                               .bytes_per_op = 100.0};
    const AllocationMetrics alloc_bytes_above{.allocations_per_op = 10.0,
                                               .bytes_per_op = 110.01};
    NUI_CHECK(!nativeui::bench::is_allocation_regression(
        alloc_baseline, alloc_exact_boundary, false));
    NUI_CHECK(nativeui::bench::is_allocation_regression(
        alloc_baseline, alloc_count_above, false));
    NUI_CHECK(nativeui::bench::is_allocation_regression(
        alloc_baseline, alloc_bytes_above, false));
    NUI_CHECK(!nativeui::bench::allocation_blocking_after_two_runs(
        alloc_baseline, alloc_count_above, alloc_exact_boundary, false));
    NUI_CHECK(nativeui::bench::allocation_blocking_after_two_runs(
        alloc_baseline, alloc_count_above, alloc_bytes_above, false));

    const AllocationMetrics zero_alloc_baseline{};
    const AllocationMetrics zero_alloc_candidate{.allocations_per_op = 0.001,
                                                  .bytes_per_op = 1.0};
    NUI_CHECK(nativeui::bench::is_allocation_regression(
        zero_alloc_baseline, zero_alloc_candidate, true));
    NUI_CHECK(nativeui::bench::allocation_blocking_after_two_runs(
        zero_alloc_baseline, zero_alloc_candidate, zero_alloc_candidate, true));

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
