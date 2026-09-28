#pragma once

#include "effect_cache_key.hpp"
#include "image_texture_cache_key.hpp"
#include "render_resource_cache.hpp"
#include "shader_brush_access.hpp"

#include "include/core/SkImageFilter.h"
#include "include/core/SkShader.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ui::detail {

class RenderResourceMaterializationContext final {
public:
    using CachedResource =
        std::variant<sk_sp<SkShader>, sk_sp<SkImageFilter>>;

    struct ImageTextureAcquisition final {
        sk_sp<SkShader> shader;
        std::shared_ptr<const sk_sp<SkShader>> frame_lease;
        bool hit{false};
        bool retained{false};

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(shader);
        }
    };

    struct EffectAcquisition final {
        sk_sp<SkImageFilter> filter;
        std::shared_ptr<const sk_sp<SkImageFilter>> frame_lease;
        bool hit{false};
        bool retained{false};

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(filter);
        }
    };

    static constexpr std::size_t kMaxRetainedEntries = 512;
    static constexpr std::size_t kMaxAccountedBytes =
        128U * 1024U * 1024U;

    RenderResourceMaterializationContext()
        : resources_({
              .max_entries = kMaxRetainedEntries,
              .max_accounted_bytes = kMaxAccountedBytes}) {}

    RenderResourceMaterializationContext(
        const RenderResourceMaterializationContext&) = delete;
    RenderResourceMaterializationContext& operator=(
        const RenderResourceMaterializationContext&) = delete;
    RenderResourceMaterializationContext(
        RenderResourceMaterializationContext&&) = delete;
    RenderResourceMaterializationContext& operator=(
        RenderResourceMaterializationContext&&) = delete;

    ~RenderResourceMaterializationContext() noexcept = default;

    void begin_frame() noexcept {
        release_frame_resources();
        frame_active_ = true;
    }

    void end_frame() noexcept {
        release_frame_resources();
        frame_active_ = false;
    }

    template <class Factory>
    [[nodiscard]] ImageTextureAcquisition acquire_image_texture(
        const ImageTexture& texture,
        Factory&& factory) {
        const auto key = image_texture_cache_key(texture);
        if (!key) {
            auto shader = std::forward<Factory>(factory)();
            return {std::move(shader), {}, false, false};
        }

        auto acquisition = resources_.acquire(
            RenderResourceKey{*key},
            retained_storage_bytes(texture),
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        if (!acquisition) return {};

        retain_frame_resource(acquisition.resource, acquisition.retained);
        const auto* shader =
            std::get_if<sk_sp<SkShader>>(acquisition.resource.get());
        if (!shader || !*shader) return {};
        std::shared_ptr<const sk_sp<SkShader>> typed_lease{
            acquisition.resource, shader};
        return {
            *shader,
            std::move(typed_lease),
            acquisition.hit,
            acquisition.retained};
    }

    template <class Factory>
    [[nodiscard]] ImageTextureAcquisition acquire_runtime_shader(
        const std::shared_ptr<const ShaderBrushSnapshot>& snapshot,
        Factory&& factory) {
        if (!snapshot) {
            auto shader = std::forward<Factory>(factory)();
            return {std::move(shader), {}, false, false};
        }

        auto acquisition = resources_.acquire(
            RenderResourceKey{RuntimeShaderKey{snapshot}},
            0U,
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        if (!acquisition) return {};

        retain_frame_resource(acquisition.resource, acquisition.retained);
        const auto* shader =
            std::get_if<sk_sp<SkShader>>(acquisition.resource.get());
        if (!shader || !*shader) return {};
        std::shared_ptr<const sk_sp<SkShader>> typed_lease{
            acquisition.resource, shader};
        return {
            *shader,
            std::move(typed_lease),
            acquisition.hit,
            acquisition.retained};
    }

    template <class Factory>
    [[nodiscard]] EffectAcquisition acquire_effect(
        const Effect& effect,
        Factory&& factory) {
        auto acquisition = resources_.acquire(
            RenderResourceKey{EffectCacheAccess::key(effect)},
            0U,
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto filter = std::forward<Factory>(factory)();
                if (!filter) return {};
                return std::make_shared<CachedResource>(std::move(filter));
            });
        if (!acquisition) return {};

        retain_frame_resource(acquisition.resource, acquisition.retained);
        const auto* filter =
            std::get_if<sk_sp<SkImageFilter>>(acquisition.resource.get());
        if (!filter || !*filter) return {};
        std::shared_ptr<const sk_sp<SkImageFilter>> typed_lease{
            acquisition.resource, filter};
        return {
            *filter,
            std::move(typed_lease),
            acquisition.hit,
            acquisition.retained};
    }

    [[nodiscard]] std::size_t retained_entries() const noexcept {
        return resources_.retained_entries();
    }

    [[nodiscard]] std::size_t retained_accounted_bytes() const noexcept {
        return resources_.retained_accounted_bytes();
    }

    void clear() noexcept {
        release_frame_resources();
        frame_active_ = false;
        resources_.clear();
    }

private:
    struct RuntimeShaderKey final {
        std::shared_ptr<const ShaderBrushSnapshot> snapshot;
    };

    using RenderResourceKey =
        std::variant<ImageTextureCacheKey, RuntimeShaderKey, EffectCacheKey>;

    struct RenderResourceKeyHash final {
        [[nodiscard]] std::size_t operator()(
            const RenderResourceKey& key) const noexcept {
            return std::visit(
                [](const auto& value) noexcept -> std::size_t {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, ImageTextureCacheKey>) {
                        return ImageTextureCacheKeyHash{}(value);
                    } else if constexpr (std::is_same_v<Value, RuntimeShaderKey>) {
                        return ShaderBrushAccess::semantic_hash(value.snapshot);
                    } else {
                        return EffectCacheKeyHash{}(value);
                    }
                },
                key);
        }
    };

    struct RenderResourceKeyEqual final {
        [[nodiscard]] bool operator()(
            const RenderResourceKey& a,
            const RenderResourceKey& b) const noexcept {
            if (a.index() != b.index()) return false;
            return std::visit(
                [](const auto& left, const auto& right) noexcept -> bool {
                    using Left = std::decay_t<decltype(left)>;
                    using Right = std::decay_t<decltype(right)>;
                    if constexpr (!std::is_same_v<Left, Right>) {
                        return false;
                    } else if constexpr (std::is_same_v<Left, ImageTextureCacheKey>) {
                        return left == right;
                    } else if constexpr (std::is_same_v<Left, RuntimeShaderKey>) {
                        return ShaderBrushAccess::semantic_equal(
                            left.snapshot, right.snapshot);
                    } else {
                        return left == right;
                    }
                },
                a, b);
        }
    };
    [[nodiscard]] static std::size_t retained_storage_bytes(
        const ImageTexture& texture) noexcept {
        // The decoded Image backing is shared application/resource state and is
        // not charged again. Charge only renderer-owned storage that this
        // materialization path can create and retain. Use a conservative
        // saturation bound: an RGBA base level is 4 bytes/pixel and a complete
        // mip chain is strictly below twice that base storage.
        const auto rgba_storage = [](Rect source, bool mipmapped) noexcept {
            constexpr std::size_t kBytesPerPixel = 4U;
            constexpr std::size_t kOverBudget = kMaxAccountedBytes + 1U;
            constexpr std::size_t kMaxPixels =
                kMaxAccountedBytes / kBytesPerPixel;

            const double width = std::ceil(static_cast<double>(source.w));
            const double height = std::ceil(static_cast<double>(source.h));
            if (!(width >= 1.0) || !(height >= 1.0) ||
                width > static_cast<double>(kMaxPixels) ||
                height > static_cast<double>(kMaxPixels)) {
                return kOverBudget;
            }

            const auto w = static_cast<std::size_t>(width);
            const auto h = static_cast<std::size_t>(height);
            if (h != 0U && w > kMaxPixels / h) return kOverBudget;
            const std::size_t base = w * h * kBytesPerPixel;
            if (!mipmapped) return base;
            if (base > kMaxAccountedBytes / 2U) return kOverBudget;
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

        const bool clamp_x =
            texture.tile_mode_x() == TextureTileMode::Clamp;
        const bool clamp_y =
            texture.tile_mode_y() == TextureTileMode::Clamp;
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

    void retain_frame_resource(
        const std::shared_ptr<const CachedResource>& resource,
        bool retained) {
        if (!frame_active_ || !resource) return;

        if (!retained) {
            transient_frame_leases_.push_back(resource);
            return;
        }

        for (std::size_t index = 0; index < retained_frame_lease_count_; ++index) {
            if (retained_frame_leases_[index].get() == resource.get()) return;
        }

        if (retained_frame_lease_count_ < retained_frame_leases_.size()) {
            retained_frame_leases_[retained_frame_lease_count_++] = resource;
            return;
        }

        transient_frame_leases_.push_back(resource);
    }

    void release_frame_resources() noexcept {
        for (std::size_t index = 0; index < retained_frame_lease_count_; ++index) {
            retained_frame_leases_[index].reset();
        }
        retained_frame_lease_count_ = 0;
        transient_frame_leases_.clear();
    }
    using ResourceCache = RenderResourceCache<
        RenderResourceKey,
        CachedResource,
        RenderResourceKeyHash,
        RenderResourceKeyEqual>;

    ResourceCache resources_;
    std::array<std::shared_ptr<const CachedResource>,
               kMaxRetainedEntries> retained_frame_leases_{};
    std::vector<std::shared_ptr<const CachedResource>> transient_frame_leases_;
    std::size_t retained_frame_lease_count_{};
    bool frame_active_{};
};

} // namespace ui::detail
