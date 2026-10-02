#pragma once

#include <nativeui/image.hpp>

#include <cmath>
#include <cstddef>

namespace ui::detail {

inline constexpr std::size_t kRenderResourceMaxRetainedEntries = 512;
inline constexpr std::size_t kRenderResourceMaxAccountedBytes =
    128U * 1024U * 1024U;
inline constexpr std::size_t kRenderResourceOverBudgetBytes =
    kRenderResourceMaxAccountedBytes + 1U;

[[nodiscard]] inline std::size_t saturated_render_resource_storage_add(
    std::size_t left,
    std::size_t right) noexcept {
    if (left > kRenderResourceMaxAccountedBytes ||
        right > kRenderResourceMaxAccountedBytes ||
        right > kRenderResourceMaxAccountedBytes - left) {
        return kRenderResourceOverBudgetBytes;
    }
    return left + right;
}

[[nodiscard]] inline std::size_t image_texture_retained_storage_bytes(
    const ImageTexture& texture) noexcept {
    if (!texture.valid()) return 0U;

    const auto rgba_storage = [](Rect source, bool mipmapped) noexcept {
        constexpr std::size_t kBytesPerPixel = 4U;
        constexpr std::size_t kMaxPixels =
            kRenderResourceMaxAccountedBytes / kBytesPerPixel;

        const double width = std::ceil(static_cast<double>(source.w));
        const double height = std::ceil(static_cast<double>(source.h));
        if (!(width >= 1.0) || !(height >= 1.0) ||
            width > static_cast<double>(kMaxPixels) ||
            height > static_cast<double>(kMaxPixels)) {
            return kRenderResourceOverBudgetBytes;
        }

        const auto w = static_cast<std::size_t>(width);
        const auto h = static_cast<std::size_t>(height);
        if (h != 0U && w > kMaxPixels / h) {
            return kRenderResourceOverBudgetBytes;
        }
        const std::size_t base = w * h * kBytesPerPixel;
        if (!mipmapped) return base;
        if (base > kRenderResourceMaxAccountedBytes / 2U) {
            return kRenderResourceOverBudgetBytes;
        }
        return base * 2U;
    };

    const auto source = texture.source();
    const auto image_size = texture.image().size();
    const bool full_source =
        source.x == 0.0f && source.y == 0.0f &&
        source.w == image_size.w && source.h == image_size.h;
    const bool mipmapped =
        texture.sampling().mipmap() != TextureMipmap::None;
    if (mipmapped) return rgba_storage(source, true);

    const bool clamp_x = texture.tile_mode_x() == TextureTileMode::Clamp;
    const bool clamp_y = texture.tile_mode_y() == TextureTileMode::Clamp;
    if (clamp_x && clamp_y) return 0U;

    if (texture.interpretation() == TextureInterpretation::Data &&
        full_source) {
        return 0U;
    }

    const bool uses_decal =
        texture.tile_mode_x() == TextureTileMode::Decal ||
        texture.tile_mode_y() == TextureTileMode::Decal;
    if (texture.interpretation() == TextureInterpretation::Color &&
        full_source && !uses_decal) {
        return 0U;
    }

    return rgba_storage(source, false);
}

} // namespace ui::detail
