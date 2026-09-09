#include "benchmarks/t051_benchmark_comparison.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Options {
    std::string baseline_path;
    std::string first_candidate_path;
    std::string second_candidate_path;
    std::string baseline_sha;
    std::string candidate_sha;
};

std::string require_value(int& index, int argc, char** argv, std::string_view option) {
    if (++index >= argc) {
        throw std::invalid_argument(std::string(option) + " requires a value");
    }
    return argv[index];
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--baseline") {
            options.baseline_path = require_value(index, argc, argv, argument);
        } else if (argument == "--candidate-a") {
            options.first_candidate_path = require_value(index, argc, argv, argument);
        } else if (argument == "--candidate-b") {
            options.second_candidate_path = require_value(index, argc, argv, argument);
        } else if (argument == "--baseline-sha") {
            options.baseline_sha = require_value(index, argc, argv, argument);
        } else if (argument == "--candidate-sha") {
            options.candidate_sha = require_value(index, argc, argv, argument);
        } else {
            throw std::invalid_argument("unknown T052 benchmark-gate argument: " +
                                        std::string(argument));
        }
    }

    if (options.baseline_path.empty() || options.first_candidate_path.empty() ||
        options.second_candidate_path.empty() || options.baseline_sha.empty() ||
        options.candidate_sha.empty()) {
        throw std::invalid_argument(
            "T052 benchmark gate requires --baseline, --candidate-a, --candidate-b, "
            "--baseline-sha and --candidate-sha");
    }
    return options;
}

std::string read_text(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("unable to open T052 benchmark JSON: " + path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void skip_space_and_commas(std::string_view json, std::size_t& position) {
    while (position < json.size()) {
        const unsigned char ch = static_cast<unsigned char>(json[position]);
        if (std::isspace(ch) != 0 || json[position] == ',') {
            ++position;
            continue;
        }
        break;
    }
}

std::vector<nativeui::bench::ResultRecord> parse_results_document(std::string_view json) {
    const auto results_key = json.find("\"results\"");
    if (results_key == std::string_view::npos) {
        throw std::invalid_argument("T052 benchmark JSON is missing results");
    }
    const auto array_begin = json.find('[', results_key);
    if (array_begin == std::string_view::npos) {
        throw std::invalid_argument("T052 benchmark JSON results is not an array");
    }

    std::vector<nativeui::bench::ResultRecord> results;
    std::size_t position = array_begin + 1;
    while (position < json.size()) {
        skip_space_and_commas(json, position);
        if (position >= json.size()) break;
        if (json[position] == ']') return results;
        if (json[position] != '{') {
            throw std::invalid_argument("T052 benchmark JSON contains a non-object result");
        }

        const std::size_t object_begin = position;
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        for (; position < json.size(); ++position) {
            const char ch = json[position];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (ch == '\\') {
                    escaped = true;
                } else if (ch == '"') {
                    in_string = false;
                }
                continue;
            }
            if (ch == '"') {
                in_string = true;
            } else if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                --depth;
                if (depth == 0) {
                    ++position;
                    results.push_back(nativeui::bench::parse_result_json(
                        json.substr(object_begin, position - object_begin)));
                    break;
                }
                if (depth < 0) {
                    throw std::invalid_argument("T052 benchmark JSON object depth underflow");
                }
            }
        }
        if (depth != 0 || in_string) {
            throw std::invalid_argument("T052 benchmark JSON contains an unterminated result object");
        }
    }
    throw std::invalid_argument("T052 benchmark JSON results array is unterminated");
}

void require_idle_hard_gate(const nativeui::bench::ResultRecord& record) {
    if (record.timing.median_ns_per_op != 0.0 || record.timing.p95_ns_per_op != 0.0 ||
        record.allocations_per_op != 0.0 || record.bytes_allocated_per_op != 0.0) {
        throw std::runtime_error("T052 idle_invalidation hard gate is not zero");
    }
}

void validate(const Options& options) {
    const auto baseline = parse_results_document(read_text(options.baseline_path));
    const auto first = parse_results_document(read_text(options.first_candidate_path));
    const auto second = parse_results_document(read_text(options.second_candidate_path));

    if (baseline.size() != nativeui::bench::kFixedWorkloads.size() ||
        first.size() != baseline.size() || second.size() != baseline.size()) {
        throw std::runtime_error("T052 benchmark result-count mismatch");
    }

    for (std::size_t index = 0; index < baseline.size(); ++index) {
        const auto& base = baseline[index];
        const auto& candidate_a = first[index];
        const auto& candidate_b = second[index];
        const std::string& name = base.metadata.benchmark_name;

        if (nativeui::bench::find_workload(name) == nullptr) {
            throw std::runtime_error("T052 benchmark contains unknown workload: " + name);
        }
        if (base.metadata.commit_sha != options.baseline_sha) {
            throw std::runtime_error("T052 baseline SHA mismatch for " + name);
        }
        if (candidate_a.metadata.commit_sha != options.candidate_sha ||
            candidate_b.metadata.commit_sha != options.candidate_sha) {
            throw std::runtime_error("T052 candidate SHA mismatch for " + name);
        }
        if (candidate_a.metadata.benchmark_name != name ||
            candidate_b.metadata.benchmark_name != name) {
            throw std::runtime_error("T052 benchmark result ordering/name mismatch for " + name);
        }

        if (name == "idle_invalidation") {
            require_idle_hard_gate(base);
            require_idle_hard_gate(candidate_a);
            require_idle_hard_gate(candidate_b);
            continue;
        }

        const auto comparison = nativeui::bench::compare_two_complete_runs(
            base, candidate_a, candidate_b, false);
        if (!comparison.allocation_metrics_comparable) {
            throw std::runtime_error("T052 allocation evidence is unavailable for " + name);
        }
        if (comparison.timing_blocking) {
            throw std::runtime_error("T052 timing regression exceeds T051 budget twice: " + name);
        }
        if (comparison.allocation_blocking) {
            throw std::runtime_error("T052 allocation regression exceeds T051 budget twice: " + name);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        validate(parse_options(argc, argv));
        std::cout << "PASS T052 exact-head T051 benchmark policy gate\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL T052 benchmark gate: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
