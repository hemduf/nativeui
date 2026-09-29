#pragma once

#include <cstddef>

namespace ui {

class Image;

namespace detail {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)

enum class ImageTextureMaterializationFailurePoint {
    None,
    BeforeShader,
    ForceNullShader,
};

[[nodiscard]] std::size_t image_decode_call_count_for_test() noexcept;
[[nodiscard]] std::size_t image_texture_materialization_call_count_for_test() noexcept;
[[nodiscard]] std::size_t image_texture_color_materialization_call_count_for_test() noexcept;
[[nodiscard]] std::size_t image_texture_data_materialization_call_count_for_test() noexcept;
[[nodiscard]] bool image_backing_is_lazy_for_test(const Image& image) noexcept;
[[nodiscard]] bool image_color_backing_is_premul_for_test(const Image& image) noexcept;
[[nodiscard]] bool image_data_backing_is_unpremul_for_test(const Image& image) noexcept;
[[nodiscard]] bool image_color_backing_is_srgb_for_test(const Image& image) noexcept;

void set_image_texture_materialization_failure_for_test(
    ImageTextureMaterializationFailurePoint point) noexcept;

#endif

} // namespace detail
} // namespace ui
