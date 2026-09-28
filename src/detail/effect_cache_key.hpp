#pragma once

#include <nativeui/paint_style.hpp>

#include "include/core/SkColor.h"
#include "include/core/SkImageFilter.h"
#include "include/effects/SkImageFilters.h"

#include <cstddef>
#include <functional>
#include <type_traits>

namespace ui::detail {

enum class EffectCacheKind : unsigned char {
    GaussianBlur,
    DropShadow,
    DropShadowOnly,
};

struct EffectCacheKey final {
    EffectCacheKind kind{EffectCacheKind::GaussianBlur};
    float sigma_x{};
    float sigma_y{};
    Point offset{};
    Color color{};

    [[nodiscard]] bool operator==(const EffectCacheKey& other) const noexcept {
        return kind == other.kind &&
            sigma_x == other.sigma_x &&
            sigma_y == other.sigma_y &&
            offset.x == other.offset.x &&
            offset.y == other.offset.y &&
            color.r == other.color.r &&
            color.g == other.color.g &&
            color.b == other.color.b &&
            color.a == other.color.a;
    }
};

struct EffectCacheKeyHash final {
    [[nodiscard]] std::size_t operator()(const EffectCacheKey& key) const noexcept {
        std::size_t seed = std::hash<unsigned>{}(
            static_cast<unsigned>(key.kind));
        combine(seed, key.sigma_x);
        combine(seed, key.sigma_y);
        combine(seed, key.offset.x);
        combine(seed, key.offset.y);
        combine(seed, key.color.r);
        combine(seed, key.color.g);
        combine(seed, key.color.b);
        combine(seed, key.color.a);
        return seed;
    }

private:
    template <class T>
    static void combine(std::size_t& seed, T value) noexcept {
        if constexpr (std::is_same_v<T, float>) {
            if (value == 0.0f) value = 0.0f;
        }
        const std::size_t hash = std::hash<T>{}(value);
        seed ^= hash + static_cast<std::size_t>(0x9e3779b9U) +
            (seed << 6U) + (seed >> 2U);
    }
};

struct EffectCacheAccess final {
    [[nodiscard]] static EffectCacheKey key(const Effect& effect) noexcept {
        EffectCacheKind kind{};
        switch (effect.kind_) {
            case Effect::Kind::GaussianBlur:
                kind = EffectCacheKind::GaussianBlur;
                break;
            case Effect::Kind::DropShadow:
                kind = EffectCacheKind::DropShadow;
                break;
            case Effect::Kind::DropShadowOnly:
                kind = EffectCacheKind::DropShadowOnly;
                break;
        }
        return {
            kind,
            effect.sigma_x_,
            effect.sigma_y_,
            effect.offset_,
            effect.color_};
    }

    [[nodiscard]] static sk_sp<SkImageFilter> materialize(
        const Effect& effect) {
        switch (effect.kind_) {
            case Effect::Kind::GaussianBlur:
                return SkImageFilters::Blur(
                    effect.sigma_x_, effect.sigma_y_, SkTileMode::kDecal, nullptr);
            case Effect::Kind::DropShadow:
                return SkImageFilters::DropShadow(
                    effect.offset_.x,
                    effect.offset_.y,
                    effect.sigma_x_,
                    effect.sigma_y_,
                    SkColor4f{
                        effect.color_.r,
                        effect.color_.g,
                        effect.color_.b,
                        effect.color_.a},
                    nullptr,
                    nullptr);
            case Effect::Kind::DropShadowOnly:
                return SkImageFilters::DropShadowOnly(
                    effect.offset_.x,
                    effect.offset_.y,
                    effect.sigma_x_,
                    effect.sigma_y_,
                    SkColor4f{
                        effect.color_.r,
                        effect.color_.g,
                        effect.color_.b,
                        effect.color_.a},
                    nullptr,
                    nullptr);
        }
        return {};
    }
};

} // namespace ui::detail
