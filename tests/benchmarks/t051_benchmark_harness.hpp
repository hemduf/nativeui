#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nativeui::bench {

inline constexpr int kSchemaVersion = 1;
inline constexpr int kWorkloadVersion = 1;
inline constexpr int kWarmupSamples = 5;
inline constexpr int kMeasuredSamples = 30;

struct SampleSummary {
    double median_ns_per_op{};
    double p95_ns_per_op{};
    double min_ns_per_op{};
    double max_ns_per_op{};
};

inline SampleSummary summarize_samples(std::vector<double> samples) {
    if (samples.size() != static_cast<std::size_t>(kMeasuredSamples)) {
        throw std::invalid_argument("T051 requires exactly 30 measured samples");
    }

    std::sort(samples.begin(), samples.end());
    return SampleSummary{
        .median_ns_per_op = (samples[14] + samples[15]) * 0.5,
        .p95_ns_per_op = samples[28],
        .min_ns_per_op = samples.front(),
        .max_ns_per_op = samples.back(),
    };
}

struct ProtocolRun {
    std::vector<double> measured_ns_per_op;
    SampleSummary summary;
};

template <class BeforeSample, class Operation>
ProtocolRun run_fixed_protocol(std::uint64_t operations_per_sample,
                               BeforeSample&& before_sample,
                               Operation&& operation) {
    if (operations_per_sample == 0) {
        throw std::invalid_argument("T051 operations_per_sample must be non-zero");
    }

    auto run_batch = [&] {
        for (std::uint64_t operation_index = 0; operation_index < operations_per_sample;
             ++operation_index) {
            std::invoke(operation);
        }
    };

    for (int sample = 0; sample < kWarmupSamples; ++sample) {
        std::invoke(before_sample);
        run_batch();
    }

    std::vector<double> measured;
    measured.reserve(kMeasuredSamples);
    for (int sample = 0; sample < kMeasuredSamples; ++sample) {
        std::invoke(before_sample);
        const auto start = std::chrono::steady_clock::now();
        run_batch();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double, std::nano>(end - start).count();
        measured.push_back(elapsed / static_cast<double>(operations_per_sample));
    }

    return ProtocolRun{.measured_ns_per_op = measured,
                       .summary = summarize_samples(std::move(measured))};
}

struct ComparisonMetadata {
    int schema_version{};
    int workload_version{};
    std::string benchmark_name;
    std::string os;
    std::string architecture;
    std::string compiler_id;
    int compiler_major{};
    std::string build_type;
    std::uint64_t operations_per_sample{};
};

inline bool metadata_compatible(const ComparisonMetadata& baseline,
                                const ComparisonMetadata& candidate) noexcept {
    return baseline.schema_version == candidate.schema_version &&
           baseline.workload_version == candidate.workload_version &&
           baseline.benchmark_name == candidate.benchmark_name &&
           baseline.os == candidate.os &&
           baseline.architecture == candidate.architecture &&
           baseline.compiler_id == candidate.compiler_id &&
           baseline.compiler_major == candidate.compiler_major &&
           baseline.build_type == candidate.build_type &&
           baseline.operations_per_sample == candidate.operations_per_sample;
}

struct RunMetrics {
    double median_ns_per_op{};
    double p95_ns_per_op{};
};

inline bool is_timing_regression(const RunMetrics& baseline,
                                 const RunMetrics& candidate) noexcept {
    return candidate.median_ns_per_op > baseline.median_ns_per_op * 1.15 &&
           candidate.p95_ns_per_op > baseline.p95_ns_per_op * 1.20;
}

inline bool blocking_after_two_runs(const RunMetrics& baseline,
                                    const RunMetrics& first_candidate,
                                    const RunMetrics& second_candidate) noexcept {
    return is_timing_regression(baseline, first_candidate) &&
           is_timing_regression(baseline, second_candidate);
}

inline bool allocation_regression(double baseline_allocations_per_op,
                                  double candidate_allocations_per_op,
                                  bool zero_recurring_allocation) noexcept {
    if (zero_recurring_allocation) {
        return candidate_allocations_per_op > 0.0;
    }
    return candidate_allocations_per_op > baseline_allocations_per_op * 1.10;
}

inline bool idle_invalidation_passes(std::uint64_t invalidation_count) noexcept {
    return invalidation_count == 0;
}

struct WorkloadSpec {
    std::string_view name;
    std::uint64_t operations_per_sample;
    bool timed;
};

inline constexpr std::array<WorkloadSpec, 11> kFixedWorkloads{{
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
}};

inline const WorkloadSpec* find_workload(std::string_view name) noexcept {
    const auto it = std::find_if(kFixedWorkloads.begin(), kFixedWorkloads.end(),
                                 [name](const WorkloadSpec& workload) {
                                     return workload.name == name;
                                 });
    return it == kFixedWorkloads.end() ? nullptr : &*it;
}

} // namespace nativeui::bench
