#pragma once

#include <nativeui/image.hpp>

#include "image_access.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

namespace ui::detail {

struct ImageTextureCacheKey final {
    std::shared_ptr<const ImageData> backing;
    Rect source{};
    Rect destination{};
    TextureTileMode tile_x{TextureTileMode::Clamp};
    TextureTileMode tile_y{TextureTileMode::Clamp};
    TextureFilter filter{TextureFilter::Linear};
    TextureMipmap mipmap{TextureMipmap::None};
    TextureInterpretation interpretation{TextureInterpretation::Color};
    Transform2D transform{};

    [[nodiscard]] bool operator==(const ImageTextureCacheKey& other) const noexcept {
        return backing == other.backing &&
            same_rect(source, other.source) &&
            same_rect(destination, other.destination) &&
            tile_x == other.tile_x &&
            tile_y == other.tile_y &&
            filter == other.filter &&
            mipmap == other.mipmap &&
            interpretation == other.interpretation &&
            same_transform(transform, other.transform);
    }

private:
    [[nodiscard]] static bool same_rect(Rect a, Rect b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] static bool same_transform(
        Transform2D a, Transform2D b) noexcept {
        return a.m00 == b.m00 && a.m01 == b.m01 && a.m02 == b.m02 &&
            a.m10 == b.m10 && a.m11 == b.m11 && a.m12 == b.m12;
    }
};

struct ImageTextureCacheKeyHash final {
    [[nodiscard]] std::size_t operator()(
        const ImageTextureCacheKey& key) const noexcept {
        std::size_t seed = std::hash<const ImageData*>{}(key.backing.get());
        combine(seed, key.source.x);
        combine(seed, key.source.y);
        combine(seed, key.source.w);
        combine(seed, key.source.h);
        combine(seed, key.destination.x);
        combine(seed, key.destination.y);
        combine(seed, key.destination.w);
        combine(seed, key.destination.h);
        combine(seed, static_cast<unsigned>(key.tile_x));
        combine(seed, static_cast<unsigned>(key.tile_y));
        combine(seed, static_cast<unsigned>(key.filter));
        combine(seed, static_cast<unsigned>(key.mipmap));
        combine(seed, static_cast<unsigned>(key.interpretation));
        combine(seed, key.transform.m00);
        combine(seed, key.transform.m01);
        combine(seed, key.transform.m02);
        combine(seed, key.transform.m10);
        combine(seed, key.transform.m11);
        combine(seed, key.transform.m12);
        return seed;
    }

private:
    template <class T>
    static void combine(std::size_t& seed, const T& value) noexcept {
        const std::size_t hash = std::hash<T>{}(value);
        seed ^= hash + static_cast<std::size_t>(0x9e3779b9U) +
            (seed << 6U) + (seed >> 2U);
    }
};

[[nodiscard]] inline std::optional<ImageTextureCacheKey>
image_texture_cache_key(const ImageTexture& texture) noexcept {
    if (!texture.valid()) return std::nullopt;

    const auto transform = texture.transform();
    if (!std::isfinite(transform.m00) ||
        !std::isfinite(transform.m01) ||
        !std::isfinite(transform.m02) ||
        !std::isfinite(transform.m10) ||
        !std::isfinite(transform.m11) ||
        !std::isfinite(transform.m12)) {
        return std::nullopt;
    }

    const auto& backing = ImageAccess::data(texture.image());
    if (!backing) return std::nullopt;

    const auto sampling = texture.sampling();
    return ImageTextureCacheKey{
        backing,
        texture.source(),
        texture.destination(),
        texture.tile_mode_x(),
        texture.tile_mode_y(),
        sampling.filter(),
        sampling.mipmap(),
        texture.interpretation(),
        transform};
}

} // namespace ui::detail
