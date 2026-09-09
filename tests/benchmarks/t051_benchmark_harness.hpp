#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
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

    bool operator==(const SampleSummary&) const = default;
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
    std::string compiler_version;
    std::string build_type;
    std::uint64_t operations_per_sample{};
    std::string commit_sha;

    bool operator==(const ComparisonMetadata&) const = default;
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

struct ResultRecord {
    ComparisonMetadata metadata;
    std::uint64_t node_count{};
    std::uint64_t actions_per_operation{};
    int logical_width{};
    int logical_height{};
    int warmup_samples{kWarmupSamples};
    int measured_samples{kMeasuredSamples};
    SampleSummary timing;
    bool allocation_metrics_available{};
    double allocations_per_op{};
    double bytes_allocated_per_op{};

    bool operator==(const ResultRecord&) const = default;
};

namespace detail {

inline std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

inline std::size_t value_start(std::string_view json, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\":";
    const auto position = json.find(marker);
    if (position == std::string_view::npos) {
        throw std::invalid_argument("missing JSON field: " + std::string(key));
    }
    return position + marker.size();
}

inline std::string json_string(std::string_view json, std::string_view key) {
    std::size_t position = value_start(json, key);
    if (position >= json.size() || json[position] != '"') {
        throw std::invalid_argument("JSON field is not a string: " + std::string(key));
    }
    ++position;
    std::string out;
    while (position < json.size()) {
        const char ch = json[position++];
        if (ch == '"') return out;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (position >= json.size()) break;
        switch (const char escaped = json[position++]) {
        case '\\': out.push_back('\\'); break;
        case '"': out.push_back('"'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        default: throw std::invalid_argument("unsupported JSON escape");
        }
    }
    throw std::invalid_argument("unterminated JSON string");
}

inline std::string json_number_token(std::string_view json, std::string_view key) {
    std::size_t begin = value_start(json, key);
    std::size_t end = begin;
    while (end < json.size()) {
        const char ch = json[end];
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' ||
            ch == 'e' || ch == 'E') {
            ++end;
            continue;
        }
        break;
    }
    if (end == begin) {
        throw std::invalid_argument("JSON field is not numeric: " + std::string(key));
    }
    return std::string(json.substr(begin, end - begin));
}

inline int json_int(std::string_view json, std::string_view key) {
    return std::stoi(json_number_token(json, key));
}

inline std::uint64_t json_u64(std::string_view json, std::string_view key) {
    return std::stoull(json_number_token(json, key));
}

inline double json_double(std::string_view json, std::string_view key) {
    return std::stod(json_number_token(json, key));
}

inline bool json_bool(std::string_view json, std::string_view key) {
    const auto start = value_start(json, key);
    if (json.substr(start, 4) == "true") return true;
    if (json.substr(start, 5) == "false") return false;
    throw std::invalid_argument("JSON field is not boolean: " + std::string(key));
}

} // namespace detail

inline std::string to_json(const ResultRecord& record) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << '{'
        << "\"schema_version\":" << record.metadata.schema_version << ','
        << "\"workload_version\":" << record.metadata.workload_version << ','
        << "\"benchmark_name\":\"" << detail::json_escape(record.metadata.benchmark_name) << "\","
        << "\"node_count\":" << record.node_count << ','
        << "\"actions_per_operation\":" << record.actions_per_operation << ','
        << "\"logical_width\":" << record.logical_width << ','
        << "\"logical_height\":" << record.logical_height << ','
        << "\"operations_per_sample\":" << record.metadata.operations_per_sample << ','
        << "\"warmup_samples\":" << record.warmup_samples << ','
        << "\"measured_samples\":" << record.measured_samples << ','
        << "\"median_ns_per_op\":" << record.timing.median_ns_per_op << ','
        << "\"p95_ns_per_op\":" << record.timing.p95_ns_per_op << ','
        << "\"min_ns_per_op\":" << record.timing.min_ns_per_op << ','
        << "\"max_ns_per_op\":" << record.timing.max_ns_per_op << ','
        << "\"allocation_metrics_available\":"
        << (record.allocation_metrics_available ? "true" : "false") << ','
        << "\"allocations_per_op\":" << record.allocations_per_op << ','
        << "\"bytes_allocated_per_op\":" << record.bytes_allocated_per_op << ','
        << "\"compiler_id\":\"" << detail::json_escape(record.metadata.compiler_id) << "\","
        << "\"compiler_version\":\"" << detail::json_escape(record.metadata.compiler_version) << "\","
        << "\"compiler_major\":" << record.metadata.compiler_major << ','
        << "\"build_type\":\"" << detail::json_escape(record.metadata.build_type) << "\","
        << "\"os\":\"" << detail::json_escape(record.metadata.os) << "\","
        << "\"architecture\":\"" << detail::json_escape(record.metadata.architecture) << "\","
        << "\"nativeui_commit_sha\":\"" << detail::json_escape(record.metadata.commit_sha) << "\""
        << '}';
    return out.str();
}

inline ResultRecord parse_result_json(std::string_view json) {
    ResultRecord record;
    record.metadata.schema_version = detail::json_int(json, "schema_version");
    record.metadata.workload_version = detail::json_int(json, "workload_version");
    record.metadata.benchmark_name = detail::json_string(json, "benchmark_name");
    record.node_count = detail::json_u64(json, "node_count");
    record.actions_per_operation = detail::json_u64(json, "actions_per_operation");
    record.logical_width = detail::json_int(json, "logical_width");
    record.logical_height = detail::json_int(json, "logical_height");
    record.metadata.operations_per_sample = detail::json_u64(json, "operations_per_sample");
    record.warmup_samples = detail::json_int(json, "warmup_samples");
    record.measured_samples = detail::json_int(json, "measured_samples");
    record.timing.median_ns_per_op = detail::json_double(json, "median_ns_per_op");
    record.timing.p95_ns_per_op = detail::json_double(json, "p95_ns_per_op");
    record.timing.min_ns_per_op = detail::json_double(json, "min_ns_per_op");
    record.timing.max_ns_per_op = detail::json_double(json, "max_ns_per_op");
    record.allocation_metrics_available = detail::json_bool(json, "allocation_metrics_available");
    record.allocations_per_op = detail::json_double(json, "allocations_per_op");
    record.bytes_allocated_per_op = detail::json_double(json, "bytes_allocated_per_op");
    record.metadata.compiler_id = detail::json_string(json, "compiler_id");
    record.metadata.compiler_version = detail::json_string(json, "compiler_version");
    record.metadata.compiler_major = detail::json_int(json, "compiler_major");
    record.metadata.build_type = detail::json_string(json, "build_type");
    record.metadata.os = detail::json_string(json, "os");
    record.metadata.architecture = detail::json_string(json, "architecture");
    record.metadata.commit_sha = detail::json_string(json, "nativeui_commit_sha");
    return record;
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
