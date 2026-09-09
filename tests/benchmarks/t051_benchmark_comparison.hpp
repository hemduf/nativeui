#pragma once

#include "t051_benchmark_harness.hpp"

#include <stdexcept>

namespace nativeui::bench {

struct TwoRunComparison {
    bool timing_blocking{};
    bool allocation_metrics_comparable{};
    bool allocation_blocking{};

    [[nodiscard]] bool blocking() const noexcept {
        return timing_blocking || allocation_blocking;
    }
};

inline TwoRunComparison compare_two_complete_runs(const ResultRecord& baseline,
                                                   const ResultRecord& first_candidate,
                                                   const ResultRecord& second_candidate,
                                                   bool zero_recurring_allocation) {
    if (!metadata_compatible(baseline.metadata, first_candidate.metadata) ||
        !metadata_compatible(baseline.metadata, second_candidate.metadata)) {
        throw std::invalid_argument("T051 comparison metadata mismatch");
    }

    const RunMetrics baseline_timing{
        .median_ns_per_op = baseline.timing.median_ns_per_op,
        .p95_ns_per_op = baseline.timing.p95_ns_per_op,
    };
    const RunMetrics first_timing{
        .median_ns_per_op = first_candidate.timing.median_ns_per_op,
        .p95_ns_per_op = first_candidate.timing.p95_ns_per_op,
    };
    const RunMetrics second_timing{
        .median_ns_per_op = second_candidate.timing.median_ns_per_op,
        .p95_ns_per_op = second_candidate.timing.p95_ns_per_op,
    };

    const bool timing_blocking =
        blocking_after_two_runs(baseline_timing, first_timing, second_timing);

    const bool allocation_metrics_comparable = baseline.allocation_metrics_available &&
                                               first_candidate.allocation_metrics_available &&
                                               second_candidate.allocation_metrics_available;

    bool allocation_blocking = false;
    if (allocation_metrics_comparable) {
        const AllocationMetrics baseline_allocations{
            .allocations_per_op = baseline.allocations_per_op,
            .bytes_per_op = baseline.bytes_allocated_per_op,
        };
        const AllocationMetrics first_allocations{
            .allocations_per_op = first_candidate.allocations_per_op,
            .bytes_per_op = first_candidate.bytes_allocated_per_op,
        };
        const AllocationMetrics second_allocations{
            .allocations_per_op = second_candidate.allocations_per_op,
            .bytes_per_op = second_candidate.bytes_allocated_per_op,
        };
        allocation_blocking = allocation_blocking_after_two_runs(
            baseline_allocations,
            first_allocations,
            second_allocations,
            zero_recurring_allocation);
    }

    return TwoRunComparison{
        .timing_blocking = timing_blocking,
        .allocation_metrics_comparable = allocation_metrics_comparable,
        .allocation_blocking = allocation_blocking,
    };
}

} // namespace nativeui::bench
