#include <nativeui/shader.hpp>

#include "src/detail/shader_instance_access.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace allocation_probe {
bool fail_allocations = false;
std::size_t allocation_count = 0;
} // namespace allocation_probe

void* operator new(std::size_t size) {
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    ++allocation_probe::allocation_count;
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    ++allocation_probe::allocation_count;
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

namespace {

constexpr std::string_view kFailDiagnosticMarker =
    "/*__NATIVEUI_T079_FAIL_DIAGNOSTIC__*/";
constexpr std::string_view kFailAfterDiagnosticMarker =
    "/*__NATIVEUI_T079_FAIL_AFTER_DIAGNOSTIC__*/";
constexpr std::string_view kFailReflectionMarker =
    "/*__NATIVEUI_T080_FAIL_REFLECTION__*/";
constexpr std::string_view kFailAfterReflectionMarker =
    "/*__NATIVEUI_T080_FAIL_AFTER_REFLECTION__*/";
constexpr std::string_view kFailUnsupportedDiagnosticMarker =
    "/*__NATIVEUI_T080_FAIL_UNSUPPORTED_DIAGNOSTIC__*/";
constexpr std::string_view kFailProgramDataMarker =
    "/*__NATIVEUI_T079_FAIL_PROGRAM_DATA__*/";
constexpr std::string_view kFailWrapperMarker =
    "/*__NATIVEUI_T079_FAIL_WRAPPER__*/";
constexpr std::string_view kFailPublicationMarker =
    "/*__NATIVEUI_T079_FAIL_PUBLICATION__*/";
constexpr std::string_view kEmptyDiagnosticMarker =
    "/*__NATIVEUI_T079_EMPTY_DIAGNOSTIC__*/";
constexpr std::string_view kOversizedSourceMarker =
    "/*__NATIVEUI_T079_OVERSIZED_SOURCE__*/";

constexpr std::string_view kValidShader = R"(
    half4 main(float2 p) {
        return half4(0.25, 0.5, 0.75, 1.0);
    }
)";

constexpr std::string_view kFloatShader = R"(
    uniform float value;
    half4 main(float2 p) { return half4(value); }
)";

constexpr std::string_view kAllSetterShader = R"(
    uniform float scalar;
    uniform float2 pair;
    uniform float3 triple;
    uniform float4 vector;
    uniform int integer;
    uniform int2 ipair;
    uniform int3 itriple;
    uniform int4 ivector;
    layout(color) uniform float4 tint;
    half4 main(float2 p) { return half4(0.0); }
)";

class ScopedAllocationFailure {
public:
    ScopedAllocationFailure() noexcept {
        allocation_probe::fail_allocations = true;
    }

    ScopedAllocationFailure(const ScopedAllocationFailure&) = delete;
    ScopedAllocationFailure& operator=(const ScopedAllocationFailure&) = delete;

    ~ScopedAllocationFailure() noexcept {
        allocation_probe::fail_allocations = false;
    }
};

void check(bool condition, const char* message) {
    if (!condition) throw message;
}

void check_compile_failure(const ui::ShaderCompileResult& result) {
    check(!result.ok(), "fault result unexpectedly succeeded");
    check(!result.program, "fault result published a program");
    check(!result.diagnostics.empty(), "fault result has no diagnostic");
    for (const auto& diagnostic : result.diagnostics) {
        check(diagnostic.code == ui::ShaderCompileError::CompileError,
              "fault diagnostic classification mismatch");
        check(diagnostic.line == 0 && diagnostic.column == 0,
              "fault diagnostic source location mismatch");
        check(!diagnostic.message.empty(), "fault diagnostic message is empty");
    }
}

void expect_bad_alloc(std::string_view marker, std::string_view source) {
    std::string marked_source{marker};
    marked_source.append(source);

    bool threw = false;
    try {
        (void)ui::ShaderProgram::compile(marked_source);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    check(threw, "fault seam did not propagate bad_alloc");

    const auto later = ui::ShaderProgram::compile(kValidShader);
    check(later.ok() && later.program && later.diagnostics.empty(),
          "compile did not recover after injected allocation failure");
}

std::vector<std::byte> bytes_of(const ui::ShaderInstance& instance) {
    const auto bytes = ui::detail::ShaderInstanceAccess::binding_bytes(instance);
    return {bytes.begin(), bytes.end()};
}

float read_float(std::span<const std::byte> bytes, std::size_t offset = 0) {
    check(offset <= bytes.size() && sizeof(float) <= bytes.size() - offset,
          "float read outside binding block");
    float value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::int32_t read_int(std::span<const std::byte> bytes, std::size_t offset = 0) {
    check(offset <= bytes.size() && sizeof(std::int32_t) <= bytes.size() - offset,
          "int read outside binding block");
    std::int32_t value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void compile_publication_faults() {
    expect_bad_alloc(
        kFailDiagnosticMarker,
        "half4 main(float2 p) { return missing_symbol; }");
    expect_bad_alloc(
        kFailAfterDiagnosticMarker,
        "half4 main(float2 p) { return missing_symbol; }");
    expect_bad_alloc(kFailReflectionMarker, kFloatShader);
    expect_bad_alloc(kFailAfterReflectionMarker, kFloatShader);
    expect_bad_alloc(
        kFailUnsupportedDiagnosticMarker,
        "uniform float2x2 m; half4 main(float2 p) { return half4(0.0); }");
    expect_bad_alloc(kFailProgramDataMarker, kValidShader);
    expect_bad_alloc(kFailWrapperMarker, kValidShader);
    expect_bad_alloc(kFailPublicationMarker, kValidShader);

    std::string fallback_source{kEmptyDiagnosticMarker};
    fallback_source.append("half4 main(float2 p) { return missing_symbol; }");
    const auto fallback = ui::ShaderProgram::compile(fallback_source);
    check_compile_failure(fallback);
    check(fallback.diagnostics.size() == 1U,
          "fallback diagnostic count mismatch");
    check(fallback.diagnostics.front().message ==
              "SkSL runtime-shader compilation failed",
          "fallback diagnostic text mismatch");

    const auto oversized = ui::ShaderProgram::compile(kOversizedSourceMarker);
    check_compile_failure(oversized);
    check(oversized.diagnostics.size() == 1U,
          "oversized source diagnostic count mismatch");
    check(oversized.diagnostics.front().message ==
              "SkSL source exceeds the backend size limit",
          "oversized source diagnostic text mismatch");
}

void zero_initialization_and_exact_values() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "typed program did not compile");

    ui::ShaderInstance instance{compiled.program};
    const auto initial = ui::detail::ShaderInstanceAccess::binding_bytes(instance);
    check(!initial.empty(), "typed binding block is unexpectedly empty");
    check(std::all_of(initial.begin(), initial.end(),
                      [](std::byte value) { return value == std::byte{0}; }),
          "fresh ShaderInstance binding block is not exactly zero initialized");

    const auto scalar = ui::ShaderProgram::compile(kFloatShader);
    check(scalar.ok(), "single-float program did not compile");
    ui::ShaderInstance float_instance{scalar.program};
    check(float_instance.set_float("value", -1.25f) == ui::ShaderSetResult::Ok,
          "float setter failed");
    check(read_float(ui::detail::ShaderInstanceAccess::binding_bytes(float_instance)) == -1.25f,
          "float bytes do not preserve the exact logical value");

    const auto integer = ui::ShaderProgram::compile(R"(
        uniform int value;
        half4 main(float2 p) { return half4(0.0); }
    )");
    check(integer.ok(), "single-int program did not compile");
    ui::ShaderInstance int_instance{integer.program};
    check(int_instance.set_int("value", -1234567) == ui::ShaderSetResult::Ok,
          "int setter failed");
    check(read_int(ui::detail::ShaderInstanceAccess::binding_bytes(int_instance)) == -1234567,
          "int bytes do not preserve int32 value");

    const auto color = ui::ShaderProgram::compile(R"(
        layout(color) uniform float4 tint;
        half4 main(float2 p) { return tint; }
    )");
    check(color.ok(), "single-color program did not compile");
    ui::ShaderInstance color_instance{color.program};
    const ui::Color extended{-2.0f, 0.5f, 3.0f, 1.5f};
    check(color_instance.set_color("tint", extended) == ui::ShaderSetResult::Ok,
          "color setter failed");
    const auto color_bytes = ui::detail::ShaderInstanceAccess::binding_bytes(color_instance);
    check(color_bytes.size() == sizeof(float) * 4U, "color byte size mismatch");
    check(read_float(color_bytes, 0U) == extended.r, "color r was clamped or transformed");
    check(read_float(color_bytes, sizeof(float)) == extended.g, "color g changed");
    check(read_float(color_bytes, sizeof(float) * 2U) == extended.b, "color b changed");
    check(read_float(color_bytes, sizeof(float) * 3U) == extended.a, "color a changed");
}

void construction_and_copy_failure_are_atomic() {
    const auto compiled = ui::ShaderProgram::compile(kFloatShader);
    check(compiled.ok(), "float program did not compile");

    bool null_threw_invalid_argument = false;
    try {
        ui::ShaderInstance invalid{std::shared_ptr<const ui::ShaderProgram>{}};
        (void)invalid;
    } catch (const std::invalid_argument&) {
        null_threw_invalid_argument = true;
    }
    check(null_threw_invalid_argument,
          "null ShaderInstance did not reject with invalid_argument");

    bool constructor_bad_alloc = false;
    try {
        ScopedAllocationFailure fail;
        ui::ShaderInstance failed{compiled.program};
        (void)failed;
    } catch (const std::bad_alloc&) {
        constructor_bad_alloc = true;
    }
    check(constructor_bad_alloc, "instance construction allocation failure was not observable");

    ui::ShaderInstance later{compiled.program};
    check(later.valid(), "instance construction did not recover after allocation failure");

    ui::ShaderInstance source{compiled.program};
    check(source.set_float("value", 0.25f) == ui::ShaderSetResult::Ok,
          "source setup failed");

    bool copy_bad_alloc = false;
    try {
        ScopedAllocationFailure fail;
        ui::ShaderInstance copy{source};
        (void)copy;
    } catch (const std::bad_alloc&) {
        copy_bad_alloc = true;
    }
    check(copy_bad_alloc, "copy construction allocation failure was not observable");

    ui::ShaderInstance destination{compiled.program};
    check(destination.set_float("value", 0.75f) == ui::ShaderSetResult::Ok,
          "destination setup failed");
    const auto before = bytes_of(destination);

    bool assignment_bad_alloc = false;
    try {
        ScopedAllocationFailure fail;
        destination = source;
    } catch (const std::bad_alloc&) {
        assignment_bad_alloc = true;
    }
    check(assignment_bad_alloc, "copy assignment allocation failure was not observable");
    check(bytes_of(destination) == before,
          "failed copy assignment changed destination bindings");
    check(destination.program() == compiled.program,
          "failed copy assignment changed destination program");
}

void setters_and_moves_allocate_nothing() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "all-setter program did not compile");
    ui::ShaderInstance instance{compiled.program};

    ui::ShaderSetResult scalar{};
    ui::ShaderSetResult pair{};
    ui::ShaderSetResult triple{};
    ui::ShaderSetResult vector{};
    ui::ShaderSetResult integer{};
    ui::ShaderSetResult ipair{};
    ui::ShaderSetResult itriple{};
    ui::ShaderSetResult ivector{};
    ui::ShaderSetResult tint{};
    {
        ScopedAllocationFailure fail;
        scalar = instance.set_float("scalar", 1.0f);
        pair = instance.set_float2("pair", {1.0f, 2.0f});
        triple = instance.set_float3("triple", {1.0f, 2.0f, 3.0f});
        vector = instance.set_float4("vector", {1.0f, 2.0f, 3.0f, 4.0f});
        integer = instance.set_int("integer", 1);
        ipair = instance.set_int2("ipair", {1, 2});
        itriple = instance.set_int3("itriple", {1, 2, 3});
        ivector = instance.set_int4("ivector", {1, 2, 3, 4});
        tint = instance.set_color("tint", {-1.0f, 0.5f, 2.0f, 1.25f});
    }
    check(scalar == ui::ShaderSetResult::Ok &&
              pair == ui::ShaderSetResult::Ok &&
              triple == ui::ShaderSetResult::Ok &&
              vector == ui::ShaderSetResult::Ok &&
              integer == ui::ShaderSetResult::Ok &&
              ipair == ui::ShaderSetResult::Ok &&
              itriple == ui::ShaderSetResult::Ok &&
              ivector == ui::ShaderSetResult::Ok &&
              tint == ui::ShaderSetResult::Ok,
          "setter allocation guard changed successful results");

    ui::ShaderSetResult missing{};
    ui::ShaderSetResult mismatch{};
    ui::ShaderSetResult invalid{};
    {
        ScopedAllocationFailure fail;
        missing = instance.set_float("missing", 1.0f);
        mismatch = instance.set_float4("tint", {1.0f, 1.0f, 1.0f, 1.0f});
        invalid = instance.set_float(
            "scalar",
            std::numeric_limits<float>::infinity());
    }
    check(missing == ui::ShaderSetResult::NotFound,
          "missing setter result mismatch under allocation guard");
    check(mismatch == ui::ShaderSetResult::TypeMismatch,
          "type mismatch result mismatch under allocation guard");
    check(invalid == ui::ShaderSetResult::InvalidValue,
          "invalid setter result mismatch under allocation guard");

    ui::ShaderInstance move_source{compiled.program};
    check(move_source.set_float("scalar", 0.25f) == ui::ShaderSetResult::Ok,
          "move source setup failed");

    alignas(ui::ShaderInstance) std::byte move_storage[sizeof(ui::ShaderInstance)];
    ui::ShaderInstance* moved = nullptr;
    {
        ScopedAllocationFailure fail;
        moved = ::new (static_cast<void*>(move_storage))
            ui::ShaderInstance(std::move(move_source));
    }
    check(moved->valid(), "move construction did not transfer valid state");
    check(!move_source.valid(), "move construction did not leave source inert");

    ui::ShaderInstance move_destination{compiled.program};
    {
        ScopedAllocationFailure fail;
        move_destination = std::move(*moved);
    }
    check(move_destination.valid(), "move assignment did not transfer valid state");
    check(!moved->valid(), "move assignment did not leave source inert");
    moved->~ShaderInstance();

    auto* move_destination_alias = &move_destination;
    {
        ScopedAllocationFailure fail;
        move_destination = std::move(*move_destination_alias);
    }
    check(move_destination.valid(), "self-move changed valid state");
}

void setter_failure_is_non_destructive_and_instances_are_isolated() {
    const auto compiled = ui::ShaderProgram::compile(kFloatShader);
    check(compiled.ok(), "float program did not compile");

    ui::ShaderInstance first{compiled.program};
    ui::ShaderInstance second{compiled.program};
    check(first.set_float("value", 0.25f) == ui::ShaderSetResult::Ok,
          "first setup failed");
    check(second.set_float("value", 0.75f) == ui::ShaderSetResult::Ok,
          "second setup failed");

    const auto first_before = bytes_of(first);
    const auto second_before = bytes_of(second);

    check(first.set_float("value", std::numeric_limits<float>::quiet_NaN()) ==
              ui::ShaderSetResult::InvalidValue,
          "non-finite setter result mismatch");
    check(first.set_int("value", 17) == ui::ShaderSetResult::TypeMismatch,
          "type mismatch setter result mismatch");
    check(first.set_float("missing", 1.0f) == ui::ShaderSetResult::NotFound,
          "missing setter result mismatch");

    check(bytes_of(first) == first_before,
          "failed setters changed existing binding bytes");
    check(bytes_of(second) == second_before,
          "first instance failure changed second instance");

    ui::ShaderInstance copy{first};
    const auto copy_before = bytes_of(copy);
    check(first.set_float("value", 0.5f) == ui::ShaderSetResult::Ok,
          "post-copy first mutation failed");
    check(bytes_of(copy) == copy_before,
          "copy shares mutable binding storage with source");
    check(bytes_of(first) != bytes_of(copy),
          "copy isolation mutation was not observable");
}

void inert_setters_allocate_nothing() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "all-setter program did not compile");
    ui::ShaderInstance source{compiled.program};
    ui::ShaderInstance live{std::move(source)};
    check(live.valid() && !source.valid(), "failed to produce inert source");

    ui::ShaderSetResult results[9]{};
    {
        ScopedAllocationFailure fail;
        results[0] = source.set_float("scalar", 1.0f);
        results[1] = source.set_float2("pair", {1.0f, 2.0f});
        results[2] = source.set_float3("triple", {1.0f, 2.0f, 3.0f});
        results[3] = source.set_float4("vector", {1.0f, 2.0f, 3.0f, 4.0f});
        results[4] = source.set_int("integer", 1);
        results[5] = source.set_int2("ipair", {1, 2});
        results[6] = source.set_int3("itriple", {1, 2, 3});
        results[7] = source.set_int4("ivector", {1, 2, 3, 4});
        results[8] = source.set_color("tint", {1.0f, 1.0f, 1.0f, 1.0f});
    }

    for (auto result : results) {
        check(result == ui::ShaderSetResult::NotFound,
              "inert setter did not return NotFound");
    }

    ui::ShaderInstance inert_copy{source};
    check(!inert_copy.valid(), "copying inert instance produced valid state");
    inert_copy = live;
    check(inert_copy.valid(), "valid reassignment did not recover inert instance");
}

void suite() {
    compile_publication_faults();
    zero_initialization_and_exact_values();
    construction_and_copy_failure_are_atomic();
    setters_and_moves_allocate_nothing();
    setter_failure_is_non_destructive_and_instances_are_isolated();
    inert_setters_allocate_nothing();
}

} // namespace

int main() {
    try {
        suite();
        std::cout << "PASS shader_faults\n";
        return EXIT_SUCCESS;
    } catch (const char* error) {
        allocation_probe::fail_allocations = false;
        std::cerr << "FAIL shader_faults: " << error << '\n';
        return EXIT_FAILURE;
    } catch (const std::exception& error) {
        allocation_probe::fail_allocations = false;
        std::cerr << "FAIL shader_faults: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
