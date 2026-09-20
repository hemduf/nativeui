#pragma once

#include <cstddef>

namespace ui::detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class ImageTextureMaterializationFailurePoint {
    None,
    BeforeShader,
    ForceNullShader,
};

[[nodiscard]] std::size_t image_decode_call_count_for_test() noexcept;
[[nodiscard]] std::size_t image_texture_materialization_call_count_for_test() noexcept;

void set_image_texture_materialization_failure_for_test(
    ImageTextureMaterializationFailurePoint point) noexcept;

#endif

} // namespace ui::detail
