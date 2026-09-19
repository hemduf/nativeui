#pragma once

#include <cstddef>

namespace ui::detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class ShaderMaterializationFailurePoint {
    None,
    BeforeUniformData,
    BeforeShader,
    ForceNullShader,
};

[[nodiscard]] std::size_t shader_compile_call_count_for_test() noexcept;
[[nodiscard]] std::size_t shader_materialization_call_count_for_test() noexcept;

void set_shader_materialization_failure_for_test(
    ShaderMaterializationFailurePoint point) noexcept;

#endif

} // namespace ui::detail
