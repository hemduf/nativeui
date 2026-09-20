#pragma once

#include <nativeui/paint_style.hpp>

#include <variant>

namespace ui::detail {

struct ImageTextureBrushAccess {
    [[nodiscard]] static bool is_image_texture(const Brush& brush) noexcept {
        return std::holds_alternative<ImageTexture>(brush.value_);
    }

    [[nodiscard]] static const ImageTexture* texture(const Brush& brush) noexcept {
        return std::get_if<ImageTexture>(&brush.value_);
    }

    [[nodiscard]] static bool is_transparent_solid(const Brush& brush) noexcept {
        const auto* color = std::get_if<Color>(&brush.value_);
        return color &&
               color->r == 0.0f && color->g == 0.0f &&
               color->b == 0.0f && color->a == 0.0f;
    }
};

} // namespace ui::detail
