#include "test_support.hpp"

#include <nativeui/shader.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

constexpr std::string_view kValidShader = R"(
    half4 main(float2 p) {
        return half4(0.25, 0.5, 0.75, 1.0);
    }
)";

constexpr std::string_view kTypedUniformShader = R"(
    uniform float scalar;
    uniform half2 pair;
    uniform float3 triple;
    uniform half4 vector;
    uniform int integer;
    uniform int2 ipair;
    uniform int3 itriple;
    uniform int4 ivector;
    layout(color) uniform half4 tint;

    half4 main(float2 p) {
        return half4(p.x * 0.0, p.y * 0.0, 0.0, 1.0);
    }
)";

void api_traits() {
    static_assert(!std::is_default_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_copy_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_copy_assignable_v<ui::ShaderProgram>);
    static_assert(!std::is_move_constructible_v<ui::ShaderProgram>);
    static_assert(!std::is_move_assignable_v<ui::ShaderProgram>);
    static_assert(std::is_nothrow_destructible_v<ui::ShaderProgram>);

    static_assert(!std::is_default_constructible_v<ui::ShaderInstance>);
    static_assert(std::is_copy_constructible_v<ui::ShaderInstance>);
    static_assert(std::is_copy_assignable_v<ui::ShaderInstance>);
    static_assert(std::is_nothrow_move_constructible_v<ui::ShaderInstance>);
    static_assert(std::is_nothrow_move_assignable_v<ui::ShaderInstance>);
    static_assert(std::is_nothrow_destructible_v<ui::ShaderInstance>);

    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float(
        std::declval<std::string_view>(), std::declval<float>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float2(
        std::declval<std::string_view>(), std::declval<std::array<float, 2>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float3(
        std::declval<std::string_view>(), std::declval<std::array<float, 3>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_float4(
        std::declval<std::string_view>(), std::declval<std::array<float, 4>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int(
        std::declval<std::string_view>(), std::declval<std::int32_t>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int2(
        std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 2>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int3(
        std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 3>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_int4(
        std::declval<std::string_view>(), std::declval<std::array<std::int32_t, 4>>())));
    static_assert(noexcept(std::declval<ui::ShaderInstance&>().set_color(
        std::declval<std::string_view>(), std::declval<ui::Color>())));
    static_assert(!noexcept(std::declval<ui::ShaderInstance&>().set_child(
        std::declval<std::string_view>(), std::declval<const ui::Brush&>())));
    static_assert(ui::ShaderInstance::kMaxChildDepth == 16U);
}

void check_compile_failure(const ui::ShaderCompileResult& result) {
    NUI_CHECK(!result.ok());
    NUI_CHECK(!result.program);
    NUI_CHECK(!result.diagnostics.empty());
    for (const auto& diagnostic : result.diagnostics) {
        NUI_CHECK(diagnostic.code == ui::ShaderCompileError::CompileError);
        NUI_CHECK(diagnostic.line == 0);
        NUI_CHECK(diagnostic.column == 0);
        NUI_CHECK(!diagnostic.message.empty());
    }
}

void check_unsupported(const ui::ShaderCompileResult& result) {
    NUI_CHECK(!result.ok());
    NUI_CHECK(!result.program);
    NUI_CHECK(!result.diagnostics.empty());
    for (const auto& diagnostic : result.diagnostics) {
        NUI_CHECK(diagnostic.code == ui::ShaderCompileError::UnsupportedInterface);
        NUI_CHECK(diagnostic.line == 0);
        NUI_CHECK(diagnostic.column == 0);
        NUI_CHECK(!diagnostic.message.empty());
    }
}

void exact_result_contract() {
    const auto valid = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(valid.ok());
    NUI_CHECK(valid.program);
    NUI_CHECK(valid.diagnostics.empty());
    NUI_CHECK(valid.program->uniforms().empty());
    NUI_CHECK(valid.program->children().empty());

    check_compile_failure(ui::ShaderProgram::compile(""));
    check_compile_failure(ui::ShaderProgram::compile(std::string_view{}));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float2 p) {
            return half4(1.0, 0.0, 0.0, 1.0)
        }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float2 p) {
            return missing_symbol;
        }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        half4 main(float4 p) {
            return half4(1.0);
        }
    )"));
}

void typed_reflection_preserves_backend_order() {
    const auto first = ui::ShaderProgram::compile(kTypedUniformShader);
    const auto second = ui::ShaderProgram::compile(kTypedUniformShader);
    NUI_CHECK(first.ok());
    NUI_CHECK(second.ok());

    constexpr std::array<std::string_view, 9> expected_names{
        "scalar", "pair", "triple", "vector",
        "integer", "ipair", "itriple", "ivector", "tint",
    };
    constexpr std::array<ui::ShaderUniformType, 9> expected_types{
        ui::ShaderUniformType::Float,
        ui::ShaderUniformType::Float2,
        ui::ShaderUniformType::Float3,
        ui::ShaderUniformType::Float4,
        ui::ShaderUniformType::Int,
        ui::ShaderUniformType::Int2,
        ui::ShaderUniformType::Int3,
        ui::ShaderUniformType::Int4,
        ui::ShaderUniformType::Color,
    };

    const auto a = first.program->uniforms();
    const auto b = second.program->uniforms();
    NUI_CHECK(a.size() == expected_names.size());
    NUI_CHECK(b.size() == expected_names.size());

    for (std::size_t index = 0; index < expected_names.size(); ++index) {
        NUI_CHECK(a[index].name == expected_names[index]);
        NUI_CHECK(a[index].type == expected_types[index]);
        NUI_CHECK(b[index].name == a[index].name);
        NUI_CHECK(b[index].type == a[index].type);
    }
}

void child_reflection_preserves_source_order() {
    const auto result = ui::ShaderProgram::compile(R"(
        uniform float before;
        uniform shader albedo;
        uniform half2 middle;
        uniform shader detail;
        uniform float after;

        half4 main(float2 p) {
            return albedo.eval(p) * before +
                   detail.eval(p) * after +
                   half4(middle.x * 0.0);
        }
    )");
    NUI_CHECK(result.ok());
    NUI_CHECK(result.program);
    NUI_CHECK(result.diagnostics.empty());

    const auto children = result.program->children();
    NUI_CHECK(children.size() == 2U);
    NUI_CHECK(children[0].name == "albedo");
    NUI_CHECK(children[1].name == "detail");

    const auto uniforms = result.program->uniforms();
    NUI_CHECK(uniforms.size() == 3U);
    NUI_CHECK(uniforms[0].name == "before");
    NUI_CHECK(uniforms[1].name == "middle");
    NUI_CHECK(uniforms[2].name == "after");

    ui::ShaderInstance first{result.program};
    ui::ShaderInstance second{result.program};
    NUI_CHECK(first.valid());
    NUI_CHECK(second.valid());
}

void half_and_color_reflection_contract() {
    const auto result = ui::ShaderProgram::compile(R"(
        uniform half h;
        uniform half2 h2;
        uniform half3 h3;
        uniform half4 h4;
        layout(color) uniform float4 color4;
        half4 main(float2 p) { return half4(0.0); }
    )");
    NUI_CHECK(result.ok());
    const auto uniforms = result.program->uniforms();
    NUI_CHECK(uniforms.size() == 5U);
    NUI_CHECK(uniforms[0].type == ui::ShaderUniformType::Float);
    NUI_CHECK(uniforms[1].type == ui::ShaderUniformType::Float2);
    NUI_CHECK(uniforms[2].type == ui::ShaderUniformType::Float3);
    NUI_CHECK(uniforms[3].type == ui::ShaderUniformType::Float4);
    NUI_CHECK(uniforms[4].type == ui::ShaderUniformType::Color);
}

void unsupported_profile_is_atomic() {
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform float2x2 transform;
        half4 main(float2 p) { return half4(0.0); }
    )"));
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform float accepted_first;
        uniform float2x2 rejected_second;
        half4 main(float2 p) { return half4(accepted_first); }
    )"));
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform float values[2];
        half4 main(float2 p) { return half4(0.0); }
    )"));
    check_unsupported(ui::ShaderProgram::compile(R"(
        layout(color) uniform half3 tint;
        half4 main(float2 p) { return half4(0.0); }
    )"));
    {
        const auto supported_child = ui::ShaderProgram::compile(R"(
            uniform shader child;
            half4 main(float2 p) { return child.eval(p); }
        )");
        NUI_CHECK(supported_child.ok());
        NUI_CHECK(supported_child.program->children().size() == 1U);
        NUI_CHECK(supported_child.program->children().front().name == "child");
    }
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform colorFilter child;
        half4 main(float2 p) { return child.eval(half4(0.25, 0.5, 0.75, 1.0)); }
    )"));
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform shader accepted;
        uniform colorFilter rejected;
        half4 main(float2 p) {
            return rejected.eval(accepted.eval(p));
        }
    )"));
    check_unsupported(ui::ShaderProgram::compile(R"(
        uniform blender child;
        half4 main(float2 p) {
            return child.eval(half4(1.0, 0.0, 0.0, 1.0),
                              half4(0.0, 0.0, 1.0, 1.0));
        }
    )"));

    // Compiler failures remain compiler failures rather than being reparsed into
    // NativeUI profile diagnostics.
    check_compile_failure(ui::ShaderProgram::compile(R"(
        uniform unsupported_type value;
        half4 main(float2 p) { return half4(0.0); }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        uniform shader children[2];
        half4 main(float2 p) { return half4(0.0); }
    )"));
    check_compile_failure(ui::ShaderProgram::compile(R"(
        uniform shader duplicateChild;
        uniform shader duplicateChild;
        half4 main(float2 p) { return duplicateChild.eval(p); }
    )"));
}

void instance_setter_matrix() {
    const auto compiled = ui::ShaderProgram::compile(kTypedUniformShader);
    NUI_CHECK(compiled.ok());

    ui::ShaderInstance instance{compiled.program};
    NUI_CHECK(instance.valid());
    NUI_CHECK(instance.program() == compiled.program);

    NUI_CHECK(instance.set_float("scalar", 0.25f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_float2("pair", {-1.0f, 2.0f}) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_float3("triple", {-1.0f, 2.0f, 3.0f}) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_float4("vector", {-1.0f, 2.0f, 3.0f, 4.0f}) ==
              ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_int("integer", -17) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_int2("ipair", {-1, 2}) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_int3("itriple", {-1, 2, 3}) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_int4("ivector", {-1, 2, 3, 4}) == ui::ShaderSetResult::Ok);
    NUI_CHECK(instance.set_color("tint", {-2.0f, 0.5f, 3.0f, 1.5f}) ==
              ui::ShaderSetResult::Ok);

    NUI_CHECK(instance.set_float("missing", 1.0f) == ui::ShaderSetResult::NotFound);
    NUI_CHECK(instance.set_float4("tint", {1.0f, 1.0f, 1.0f, 1.0f}) ==
              ui::ShaderSetResult::TypeMismatch);
    NUI_CHECK(instance.set_color("vector", {1.0f, 1.0f, 1.0f, 1.0f}) ==
              ui::ShaderSetResult::TypeMismatch);

    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    NUI_CHECK(instance.set_float("scalar", inf) == ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(instance.set_float2("pair", {0.0f, nan}) == ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(instance.set_float3("triple", {0.0f, inf, 1.0f}) ==
              ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(instance.set_float4("vector", {0.0f, 1.0f, 2.0f, nan}) ==
              ui::ShaderSetResult::InvalidValue);
    NUI_CHECK(instance.set_color("tint", {0.0f, 1.0f, inf, 1.0f}) ==
              ui::ShaderSetResult::InvalidValue);

    // Name/type precedence stays deterministic and never mutates storage.
    NUI_CHECK(instance.set_float("missing", inf) == ui::ShaderSetResult::NotFound);
    NUI_CHECK(instance.set_float4("tint", {0.0f, 1.0f, 2.0f, inf}) ==
              ui::ShaderSetResult::TypeMismatch);
}

void null_and_zero_uniform_construction() {
    bool invalid_argument = false;
    try {
        ui::ShaderInstance invalid{std::shared_ptr<const ui::ShaderProgram>{}};
        (void)invalid;
    } catch (const std::invalid_argument&) {
        invalid_argument = true;
    }
    NUI_CHECK(invalid_argument);

    const auto no_uniforms = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(no_uniforms.ok());
    ui::ShaderInstance instance{no_uniforms.program};
    NUI_CHECK(instance.valid());
    NUI_CHECK(instance.set_float("missing", 1.0f) == ui::ShaderSetResult::NotFound);
}

void copy_move_and_inert_contract() {
    const auto compiled = ui::ShaderProgram::compile(R"(
        uniform float gain;
        half4 main(float2 p) { return half4(gain); }
    )");
    NUI_CHECK(compiled.ok());

    ui::ShaderInstance source{compiled.program};
    NUI_CHECK(source.set_float("gain", 0.25f) == ui::ShaderSetResult::Ok);

    ui::ShaderInstance copy{source};
    NUI_CHECK(copy.valid());
    NUI_CHECK(copy.program() == source.program());
    NUI_CHECK(source.set_float("gain", 0.75f) == ui::ShaderSetResult::Ok);
    NUI_CHECK(copy.set_float("gain", 0.5f) == ui::ShaderSetResult::Ok);

    auto* copy_alias = &copy;
    copy = *copy_alias;
    NUI_CHECK(copy.valid());

    ui::ShaderInstance moved{std::move(source)};
    NUI_CHECK(moved.valid());
    NUI_CHECK(!source.valid());
    NUI_CHECK(!source.program());
    NUI_CHECK(source.set_float("gain", 1.0f) == ui::ShaderSetResult::NotFound);

    ui::ShaderInstance inert_copy{source};
    NUI_CHECK(!inert_copy.valid());
    NUI_CHECK(!inert_copy.program());
    NUI_CHECK(inert_copy.set_float("gain", 1.0f) == ui::ShaderSetResult::NotFound);

    copy = source;
    NUI_CHECK(!copy.valid());
    NUI_CHECK(!copy.program());

    source = moved;
    NUI_CHECK(source.valid());
    NUI_CHECK(source.set_float("gain", 0.9f) == ui::ShaderSetResult::Ok);

    auto* moved_alias = &moved;
    moved = std::move(*moved_alias);
    NUI_CHECK(moved.valid());

    copy = std::move(source);
    NUI_CHECK(copy.valid());
    NUI_CHECK(!source.valid());
}

void caller_source_and_reflection_lifetime_are_independent() {
    const auto success = [] {
        std::string source{R"(
            uniform float lifetimeValue;
            half4 main(float2 p) { return half4(lifetimeValue); }
        )"};
        return ui::ShaderProgram::compile(source);
    }();
    NUI_CHECK(success.ok());
    NUI_CHECK(success.program);
    NUI_CHECK(success.diagnostics.empty());
    NUI_CHECK(success.program->uniforms().size() == 1U);
    NUI_CHECK(success.program->uniforms().front().name == "lifetimeValue");

    const auto failure = [] {
        std::string source{"half4 main(float2 p) { return nope; }"};
        return ui::ShaderProgram::compile(source);
    }();
    check_compile_failure(failure);
    NUI_CHECK(!failure.diagnostics.front().message.empty());
}

void child_reflection_lifetime_is_source_independent() {
    const auto compiled = [] {
        std::string source{R"(
            uniform shader lifetimeChild;
            half4 main(float2 p) { return lifetimeChild.eval(p); }
        )"};
        return ui::ShaderProgram::compile(source);
    }();

    NUI_CHECK(compiled.ok());
    NUI_CHECK(compiled.program);
    NUI_CHECK(compiled.program->children().size() == 1U);
    NUI_CHECK(compiled.program->children().front().name == "lifetimeChild");
}

void immutable_program_can_be_retained_by_two_uis() {
    auto result = ui::ShaderProgram::compile(kValidShader);
    NUI_CHECK(result.ok());
    auto shared = std::move(result.program);
    std::weak_ptr<const ui::ShaderProgram> lifetime = shared;

    const auto make_ui = [](std::shared_ptr<const ui::ShaderProgram> program) {
        return std::make_unique<ui::UI>(
            ui::Canvas{2.0f, 2.0f, [program = std::move(program)](ui::CanvasContext2D& canvas) {
                NUI_CHECK(program);
                canvas.fill_rect(
                    {0.0f, 0.0f, 2.0f, 2.0f},
                    ui::Color{0.0f, 0.0f, 0.0f, 1.0f});
            }});
    };

    auto first = make_ui(shared);
    auto second = make_ui(shared);
    shared.reset();

    ui::HeadlessRenderer renderer{{2.0f, 2.0f}, 1.0f};
    NUI_CHECK(renderer.render(*first));
    first.reset();
    NUI_CHECK(!lifetime.expired());

    NUI_CHECK(renderer.render(*second));
    second.reset();
    NUI_CHECK(lifetime.expired());
}

void repeated_failed_compile_remains_usable() {
    for (int i = 0; i < 128; ++i) {
        check_compile_failure(
            ui::ShaderProgram::compile("half4 main(float2 p) { return nope; }"));
    }

    const auto valid = ui::ShaderProgram::compile(kTypedUniformShader);
    NUI_CHECK(valid.ok());
    ui::ShaderInstance instance{valid.program};
    NUI_CHECK(instance.set_float("scalar", 0.5f) == ui::ShaderSetResult::Ok);
}

void suite() {
    api_traits();
    exact_result_contract();
    typed_reflection_preserves_backend_order();
    child_reflection_preserves_source_order();
    half_and_color_reflection_contract();
    unsupported_profile_is_atomic();
    instance_setter_matrix();
    null_and_zero_uniform_construction();
    copy_move_and_inert_contract();
    caller_source_and_reflection_lifetime_are_independent();
    child_reflection_lifetime_is_source_independent();
    immutable_program_can_be_retained_by_two_uis();
    repeated_failed_compile_remains_usable();
}

} // namespace

int main() { return test::run("shader", &suite); }
