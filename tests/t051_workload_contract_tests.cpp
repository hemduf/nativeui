#define main t051_benchmark_program_main
#include "t051_benchmarks.cpp"
#undef main

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    throw std::runtime_error("line " + std::to_string(line) + ": CHECK failed: " + expression);
}

#define NUI_CHECK(expr) ::check(static_cast<bool>(expr), #expr, __LINE__)

struct ExpectedShape {
    std::string_view name;
    std::uint64_t nodes;
    std::uint64_t actions;
};

void suite() {
    auto cases = make_timed_cases();
    check_contract_shape(cases);

    constexpr std::array<ExpectedShape, 10> expected{{
        {"layout_small", 116, 1},
        {"layout_large", 990, 1},
        {"hit_test_deep", 33, 1},
        {"dispatch_pointer", 101, 1},
        {"dispatch_keyboard", 1, 2},
        {"text_edit_short", 0, 8},
        {"text_edit_multiline", 0, 16},
        {"paint_controls", 116, 1},
        {"paint_text", 93, 2},
        {"multi_instance", 47, 5},
    }};

    NUI_CHECK(cases.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        NUI_CHECK(cases[i].contract.name == expected[i].name);
        NUI_CHECK(cases[i].node_count == expected[i].nodes);
        NUI_CHECK(cases[i].actions_per_operation == expected[i].actions);
        cases[i].before_sample();
        cases[i].operation();
    }

    {
        benchmark_allocation::Scope scope;
        std::uint64_t accumulator = 0;
        for (std::uint64_t i = 0; i < 64; ++i) accumulator += i;
        const auto snapshot = scope.finish();
        NUI_CHECK(accumulator == 2016);
        NUI_CHECK(snapshot.allocations == 0);
        NUI_CHECK(snapshot.bytes == 0);
    }

    {
        benchmark_allocation::Scope scope;
        void* memory = ::operator new(64);
        ::operator delete(memory);
        const auto snapshot = scope.finish();
        NUI_CHECK(snapshot.allocations == 1);
        NUI_CHECK(snapshot.bytes >= 64);
    }

    const auto idle = run_idle_invalidation();
    NUI_CHECK(idle.metadata.benchmark_name == "idle_invalidation");
    NUI_CHECK(idle.metadata.operations_per_sample == 1000);
    NUI_CHECK(idle.timing.median_ns_per_op == 0.0);
    NUI_CHECK(idle.timing.p95_ns_per_op == 0.0);
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS t051 workload contract\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t051 workload contract: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
