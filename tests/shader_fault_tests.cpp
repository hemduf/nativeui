#include <nativeui/paint.hpp>
#include <nativeui/shader.hpp>

#include "src/detail/shader_brush_access.hpp"
#include "src/detail/shader_instance_access.hpp"
#include "src/detail/shader_test_seams.hpp"

#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

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

[[nodiscard]] void* allocate_unaligned(std::size_t size) noexcept {
    return std::malloc(size == 0 ? 1U : size);
}

[[nodiscard]] void* allocate_aligned(std::size_t size,
                                     std::size_t alignment) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1U)) != 0U) {
        return nullptr;
    }

    constexpr std::size_t kPointerBytes = sizeof(void*);
    const auto max_size = (std::numeric_limits<std::size_t>::max)();
    if (alignment - 1U > max_size - kPointerBytes) {
        return nullptr;
    }

    const std::size_t overhead = kPointerBytes + alignment - 1U;
    const std::size_t payload_size = size == 0 ? 1U : size;
    if (payload_size > max_size - overhead) {
        return nullptr;
    }

    void* raw = std::malloc(payload_size + overhead);
    if (!raw) return nullptr;

    const auto raw_address = reinterpret_cast<std::uintptr_t>(raw);
    const auto candidate = raw_address + kPointerBytes;
    const auto aligned_address =
        (candidate + alignment - 1U) & ~static_cast<std::uintptr_t>(alignment - 1U);
    auto* aligned = reinterpret_cast<std::byte*>(aligned_address);
    std::memcpy(aligned - kPointerBytes, &raw, kPointerBytes);
    return aligned;
}

void deallocate_aligned(void* pointer) noexcept {
    if (!pointer) return;
    void* raw = nullptr;
    auto* aligned = static_cast<std::byte*>(pointer);
    std::memcpy(&raw, aligned - sizeof(void*), sizeof(raw));
    std::free(raw);
}
} // namespace allocation_probe

void* operator new(std::size_t size) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_unaligned(size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_unaligned(size)) return pointer;
    throw std::bad_alloc{};
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_unaligned(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_unaligned(size);
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

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept {
    std::free(pointer);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_aligned(
            size,
            static_cast<std::size_t>(alignment))) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) throw std::bad_alloc{};
    if (void* pointer = allocation_probe::allocate_aligned(
            size,
            static_cast<std::size_t>(alignment))) {
        return pointer;
    }
    throw std::bad_alloc{};
}

void* operator new(std::size_t size,
                   std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size,
                     std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
    ++allocation_probe::allocation_count;
    if (allocation_probe::fail_allocations) return nullptr;
    return allocation_probe::allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

void operator delete(void* pointer,
                     std::size_t,
                     std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

void operator delete[](void* pointer,
                       std::size_t,
                       std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

void operator delete(void* pointer,
                     std::align_val_t,
                     const std::nothrow_t&) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

void operator delete[](void* pointer,
                       std::align_val_t,
                       const std::nothrow_t&) noexcept {
    allocation_probe::deallocate_aligned(pointer);
}

namespace {

constexpr std::string_view kFailDiagnosticMarker =
    "/*__NATIVEUI_T079_FAIL_DIAGNOSTIC__*/";
constexpr std::string_view kFailAfterDiagnosticMarker =
    "/*__NATIVEUI_T079_FAIL_AFTER_DIAGNOSTIC__*/";
constexpr std::string_view kFailReflectionMarker =
    "/*__NATIVEUI_T080_FAIL_REFLECTION__*/";
constexpr std::string_view kFailDuringReflectionMarker =
    "/*__NATIVEUI_T080_FAIL_DURING_REFLECTION__*/";
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

struct alignas(64) OverAlignedAllocationProbe {
    std::byte payload[64]{};
};

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

void check_no_allocation_since(std::size_t before, const char* message) {
    check(allocation_probe::allocation_count == before, message);
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

int read_backend_int(std::span<const std::byte> bytes, std::size_t offset = 0) {
    check(offset <= bytes.size() && sizeof(int) <= bytes.size() - offset,
          "backend int read outside binding block");
    int value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void allocation_probe_covers_all_replaceable_new_forms() {
    bool ordinary_throwing = false;
    try {
        ScopedAllocationFailure fail;
        (void)::operator new(sizeof(std::byte));
    } catch (const std::bad_alloc&) {
        ordinary_throwing = true;
    }
    check(ordinary_throwing, "allocation guard misses ordinary throwing new");

    const auto ordinary_nothrow_before = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        void* probe = ::operator new(sizeof(std::byte), std::nothrow);
        check(probe == nullptr, "allocation guard misses ordinary nothrow new");
    }
    check(allocation_probe::allocation_count == ordinary_nothrow_before + 1U,
          "ordinary nothrow new was not counted");

    constexpr auto kProbeAlignment =
        std::align_val_t{alignof(OverAlignedAllocationProbe)};

    bool aligned_throwing = false;
    try {
        ScopedAllocationFailure fail;
        (void)::operator new(
            sizeof(OverAlignedAllocationProbe),
            kProbeAlignment);
    } catch (const std::bad_alloc&) {
        aligned_throwing = true;
    }
    check(aligned_throwing, "allocation guard misses aligned throwing new");

    const auto aligned_nothrow_before = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        void* probe = ::operator new(
            sizeof(OverAlignedAllocationProbe),
            kProbeAlignment,
            std::nothrow);
        check(probe == nullptr, "allocation guard misses aligned nothrow new");
    }
    check(allocation_probe::allocation_count == aligned_nothrow_before + 1U,
          "aligned nothrow new was not counted");
}

void compile_publication_faults() {
    expect_bad_alloc(
        kFailDiagnosticMarker,
        "half4 main(float2 p) { return missing_symbol; }");
    expect_bad_alloc(
        kFailAfterDiagnosticMarker,
        "half4 main(float2 p) { return missing_symbol; }");
    expect_bad_alloc(kFailReflectionMarker, kFloatShader);
    expect_bad_alloc(
        kFailDuringReflectionMarker,
        "uniform float first; uniform float second; "
        "half4 main(float2 p) { return half4(first + second); }");
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
    check(read_backend_int(ui::detail::ShaderInstanceAccess::binding_bytes(int_instance)) ==
              -1234567,
          "int bytes do not preserve the exact backend int value");

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


void complete_binding_block_matches_reflected_packing() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "packing program did not compile");
    ui::ShaderInstance instance{compiled.program};

    check(instance.set_float("scalar", 10.0f) == ui::ShaderSetResult::Ok,
          "packing scalar setup failed");
    check(instance.set_float2("pair", {20.0f, 21.0f}) == ui::ShaderSetResult::Ok,
          "packing float2 setup failed");
    check(instance.set_float3("triple", {30.0f, 31.0f, 32.0f}) == ui::ShaderSetResult::Ok,
          "packing float3 setup failed");
    check(instance.set_float4("vector", {40.0f, 41.0f, 42.0f, 43.0f}) ==
              ui::ShaderSetResult::Ok,
          "packing float4 setup failed");
    check(instance.set_int("integer", 50) == ui::ShaderSetResult::Ok,
          "packing int setup failed");
    check(instance.set_int2("ipair", {60, 61}) == ui::ShaderSetResult::Ok,
          "packing int2 setup failed");
    check(instance.set_int3("itriple", {70, 71, 72}) == ui::ShaderSetResult::Ok,
          "packing int3 setup failed");
    check(instance.set_int4("ivector", {80, 81, 82, 83}) == ui::ShaderSetResult::Ok,
          "packing int4 setup failed");
    check(instance.set_color("tint", {-1.0f, 0.5f, 2.0f, 1.25f}) ==
              ui::ShaderSetResult::Ok,
          "packing color setup failed");

    const auto bytes = ui::detail::ShaderInstanceAccess::binding_bytes(instance);
    constexpr std::size_t kExpectedBytes =
        (1U + 2U + 3U + 4U) * sizeof(float) +
        (1U + 2U + 3U + 4U) * sizeof(int) +
        4U * sizeof(float);
    check(bytes.size() == kExpectedBytes, "complete binding block byte size mismatch");

    std::size_t offset = 0;
    const auto expect_float = [&](float expected) {
        check(read_float(bytes, offset) == expected, "float packing mismatch");
        offset += sizeof(float);
    };
    const auto expect_int = [&](std::int32_t expected) {
        check(read_backend_int(bytes, offset) == static_cast<int>(expected),
              "int packing mismatch");
        offset += sizeof(int);
    };

    for (float value : {10.0f, 20.0f, 21.0f, 30.0f, 31.0f, 32.0f,
                        40.0f, 41.0f, 42.0f, 43.0f}) {
        expect_float(value);
    }
    for (std::int32_t value : {50, 60, 61, 70, 71, 72, 80, 81, 82, 83}) {
        expect_int(value);
    }
    for (float value : {-1.0f, 0.5f, 2.0f, 1.25f}) {
        expect_float(value);
    }
    check(offset == bytes.size(), "packing verification did not consume the full block");
}


void setters_do_not_recompile_source() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "compile-count program did not compile");
    ui::ShaderInstance instance{compiled.program};

    const std::size_t before = ui::detail::shader_compile_call_count_for_test();
    check(instance.set_float("scalar", 0.25f) == ui::ShaderSetResult::Ok,
          "compile-count float setter failed");
    check(instance.set_float2("pair", {1.0f, 2.0f}) == ui::ShaderSetResult::Ok,
          "compile-count float2 setter failed");
    check(instance.set_int("integer", std::numeric_limits<std::int32_t>::min()) ==
              ui::ShaderSetResult::Ok,
          "compile-count int setter failed");
    check(instance.set_int4(
              "ivector",
              {1, 2, 3, std::numeric_limits<std::int32_t>::max()}) ==
              ui::ShaderSetResult::Ok,
          "compile-count int4 setter failed");
    check(instance.set_color("tint", {-2.0f, 0.5f, 3.0f, 1.5f}) ==
              ui::ShaderSetResult::Ok,
          "compile-count color setter failed");
    const std::size_t after = ui::detail::shader_compile_call_count_for_test();

    check(after == before, "uniform mutation unexpectedly recompiled SkSL source");
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
    const auto successful_setter_allocations = allocation_probe::allocation_count;
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
    check_no_allocation_since(
        successful_setter_allocations,
        "successful setters attempted a heap allocation");
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
    const auto failed_setter_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        missing = instance.set_float("missing", 1.0f);
        mismatch = instance.set_float4("tint", {1.0f, 1.0f, 1.0f, 1.0f});
        invalid = instance.set_float(
            "scalar",
            std::numeric_limits<float>::infinity());
    }
    check_no_allocation_since(
        failed_setter_allocations,
        "failed setters attempted a heap allocation");
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
    const auto move_construct_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        moved = ::new (static_cast<void*>(move_storage))
            ui::ShaderInstance(std::move(move_source));
    }
    check_no_allocation_since(
        move_construct_allocations,
        "ShaderInstance move construction attempted a heap allocation");
    check(moved->valid(), "move construction did not transfer valid state");
    check(!move_source.valid(), "move construction did not leave source inert");

    ui::ShaderInstance move_destination{compiled.program};
    const auto move_assign_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        move_destination = std::move(*moved);
    }
    check_no_allocation_since(
        move_assign_allocations,
        "ShaderInstance move assignment attempted a heap allocation");
    check(move_destination.valid(), "move assignment did not transfer valid state");
    check(!moved->valid(), "move assignment did not leave source inert");
    moved->~ShaderInstance();

    auto* move_destination_alias = &move_destination;
    const auto self_move_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        move_destination = std::move(*move_destination_alias);
    }
    check_no_allocation_since(
        self_move_allocations,
        "ShaderInstance self-move attempted a heap allocation");
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

void shader_brush_snapshot_failure_and_noallocation_edges() {
    const auto compiled = ui::ShaderProgram::compile(kFloatShader);
    check(compiled.ok(), "T081 float program did not compile");

    ui::ShaderInstance instance{compiled.program};
    check(instance.set_float("value", 0.5f) == ui::ShaderSetResult::Ok,
          "T081 source setup failed");

    const std::size_t compile_before =
        ui::detail::shader_compile_call_count_for_test();
    const std::size_t materialize_before =
        ui::detail::shader_materialization_call_count_for_test();

    bool snapshot_bad_alloc = false;
    try {
        ScopedAllocationFailure fail;
        ui::Brush failed{instance};
        (void)failed;
    } catch (const std::bad_alloc&) {
        snapshot_bad_alloc = true;
    }
    check(snapshot_bad_alloc, "T081 snapshot allocation failure was not observable");
    check(ui::detail::shader_compile_call_count_for_test() == compile_before,
          "T081 Brush construction recompiled source");
    check(ui::detail::shader_materialization_call_count_for_test() ==
              materialize_before,
          "T081 Brush construction materialized a backend shader");

    ui::Brush live{instance};
    check(ui::detail::ShaderBrushAccess::is_shader(live),
          "T081 normal Brush construction did not recover");

    ui::Brush repeated{instance};
    check(ui::detail::ShaderBrushAccess::is_shader(repeated),
          "T081 repeated Brush construction lost snapshot");
    check(ui::detail::shader_compile_call_count_for_test() == compile_before,
          "T081 repeated Brush construction recompiled source");
    check(ui::detail::shader_materialization_call_count_for_test() ==
              materialize_before,
          "T081 repeated Brush construction materialized a backend shader");

    ui::Brush assigned{ui::Color{0.0f, 0.0f, 1.0f, 1.0f}};
    const auto assign_before = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        assigned = live;
    }
    check_no_allocation_since(
        assign_before,
        "T081 shader Brush copy assignment attempted allocation");
    check(ui::detail::ShaderBrushAccess::is_shader(assigned),
          "T081 shader Brush copy assignment lost snapshot");

    ui::ShaderInstance move_source{compiled.program};
    ui::ShaderInstance moved{std::move(move_source)};
    check(moved.valid() && !move_source.valid(),
          "T081 failed to create inert ShaderInstance");

    const auto inert_before = allocation_probe::allocation_count;
    alignas(ui::Brush) std::byte inert_storage[sizeof(ui::Brush)];
    ui::Brush* inert = nullptr;
    {
        ScopedAllocationFailure fail;
        inert = ::new (static_cast<void*>(inert_storage)) ui::Brush(move_source);
    }
    check_no_allocation_since(
        inert_before,
        "T081 inert ShaderInstance -> Brush attempted allocation");
    check(ui::detail::ShaderBrushAccess::is_transparent_solid(*inert),
          "T081 inert ShaderInstance did not become transparent solid Brush");
    inert->~Brush();

    const auto copy_before = allocation_probe::allocation_count;
    alignas(ui::Brush) std::byte copy_storage[sizeof(ui::Brush)];
    ui::Brush* copy = nullptr;
    {
        ScopedAllocationFailure fail;
        copy = ::new (static_cast<void*>(copy_storage)) ui::Brush(live);
    }
    check_no_allocation_since(
        copy_before,
        "T081 shader Brush copy attempted allocation");
    check(ui::detail::ShaderBrushAccess::is_shader(*copy),
          "T081 shader Brush copy lost snapshot");
    copy->~Brush();
}

void shader_brush_materialization_failure_recovers() {
    const auto compiled = ui::ShaderProgram::compile(kFloatShader);
    check(compiled.ok(), "T081 materialization program did not compile");
    ui::ShaderInstance instance{compiled.program};
    check(instance.set_float("value", 1.0f) == ui::ShaderSetResult::Ok,
          "T081 materialization setup failed");
    const ui::Brush brush{instance};

    const auto info = SkImageInfo::Make(
        8, 8, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    auto surface = SkSurfaces::Raster(info);
    check(static_cast<bool>(surface), "T081 raster surface creation failed");
    auto* canvas = surface->getCanvas();
    check(canvas != nullptr, "T081 raster canvas missing");
    canvas->clear(SK_ColorBLACK);

    const std::size_t compile_before =
        ui::detail::shader_compile_call_count_for_test();
    const std::size_t materialize_before =
        ui::detail::shader_materialization_call_count_for_test();

    ui::detail::set_shader_materialization_failure_for_test(
        ui::detail::ShaderMaterializationFailurePoint::BeforeUniformData);
    bool before_data_failed = false;
    try {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    } catch (const std::bad_alloc&) {
        before_data_failed = true;
    }
    check(before_data_failed, "T081 pre-uniform failure seam did not fire");

    ui::detail::set_shader_materialization_failure_for_test(
        ui::detail::ShaderMaterializationFailurePoint::BeforeShader);
    bool before_shader_failed = false;
    try {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    } catch (const std::bad_alloc&) {
        before_shader_failed = true;
    }
    check(before_shader_failed, "T081 pre-shader failure seam did not fire");

    ui::detail::set_shader_materialization_failure_for_test(
        ui::detail::ShaderMaterializationFailurePoint::ForceNullShader);
    bool null_shader_failed = false;
    try {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    } catch (const std::runtime_error&) {
        null_shader_failed = true;
    }
    check(null_shader_failed, "T081 null-shader failure did not propagate");

    {
        ui::Painter painter{*canvas};
        painter.fill_rounded_rect({0.0f, 0.0f, 8.0f, 8.0f}, 0.0f, brush);
    }

    check(ui::detail::shader_compile_call_count_for_test() == compile_before,
          "T081 repeated paint recompiled SkSL source");
    check(ui::detail::shader_materialization_call_count_for_test() ==
              materialize_before + 4U,
          "T081 transient materialization count mismatch");

    SkPixmap pixels;
    check(surface->peekPixels(&pixels), "T081 recovery pixels unavailable");
    const auto recovered = pixels.getColor4f(4, 4);
    check(recovered.fR > 0.9f,
          "T081 later normal render did not recover after materialization failures");
}

void inert_setters_allocate_nothing() {
    const auto compiled = ui::ShaderProgram::compile(kAllSetterShader);
    check(compiled.ok(), "all-setter program did not compile");
    ui::ShaderInstance source{compiled.program};
    ui::ShaderInstance live{std::move(source)};
    check(live.valid() && !source.valid(), "failed to produce inert source");

    ui::ShaderSetResult results[9]{};
    const auto inert_setter_allocations = allocation_probe::allocation_count;
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
    check_no_allocation_since(
        inert_setter_allocations,
        "inert setters attempted a heap allocation");

    for (auto result : results) {
        check(result == ui::ShaderSetResult::NotFound,
              "inert setter did not return NotFound");
    }

    alignas(ui::ShaderInstance) std::byte inert_copy_storage[sizeof(ui::ShaderInstance)];
    ui::ShaderInstance* guarded_inert_copy = nullptr;
    const auto inert_copy_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        guarded_inert_copy = ::new (static_cast<void*>(inert_copy_storage))
            ui::ShaderInstance(source);
    }
    check_no_allocation_since(
        inert_copy_allocations,
        "copying an inert ShaderInstance attempted a heap allocation");
    check(!guarded_inert_copy->valid(),
          "copying inert instance produced valid state");
    guarded_inert_copy->~ShaderInstance();

    ui::ShaderInstance inert_copy{source};
    ui::ShaderInstance live_target{compiled.program};
    const auto inert_assign_allocations = allocation_probe::allocation_count;
    {
        ScopedAllocationFailure fail;
        live_target = source;
    }
    check_no_allocation_since(
        inert_assign_allocations,
        "copy assignment from inert ShaderInstance attempted a heap allocation");
    check(!live_target.valid(),
          "copy assignment from inert source allocated or failed to become inert");

    inert_copy = live;
    check(inert_copy.valid(), "valid reassignment did not recover inert instance");
}

void suite() {
    allocation_probe_covers_all_replaceable_new_forms();
    compile_publication_faults();
    zero_initialization_and_exact_values();
    complete_binding_block_matches_reflected_packing();
    setters_do_not_recompile_source();
    construction_and_copy_failure_are_atomic();
    setters_and_moves_allocate_nothing();
    setter_failure_is_non_destructive_and_instances_are_isolated();
    shader_brush_snapshot_failure_and_noallocation_edges();
    shader_brush_materialization_failure_recovers();
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
