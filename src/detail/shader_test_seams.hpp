#pragma once

#include <cstddef>

namespace ui::detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class ShaderMaterializationFailurePoint {
    None,
    BeforeUniformData,
    BeforeChildren,
    BeforeShader,
    ForceNullShader,
};

[[nodiscard]] std::size_t shader_compile_call_count_for_test() noexcept;
[[nodiscard]] std::size_t shader_materialization_call_count_for_test() noexcept;
[[nodiscard]] std::size_t shader_depth_read_call_count_for_test() noexcept;

void set_shader_materialization_failure_for_test(
    ShaderMaterializationFailurePoint point) noexcept;

void set_shader_materialization_failure_after_calls_for_test(
    ShaderMaterializationFailurePoint point,
    std::size_t call_offset) noexcept;

#endif

} // namespace ui::detail
