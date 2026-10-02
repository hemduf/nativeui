#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/raster_cache_epoch.hpp>

#include "effect_cache_key.hpp"
#include "gradient_cache_key.hpp"
#include "image_texture_cache_key.hpp"
#include "render_resource_accounting.hpp"
#include "render_resource_cache.hpp"
#include "shader_brush_access.hpp"

#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkShader.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ui::detail {

class RenderResourceMaterializationContext final {
public:
    struct RasterCacheKey final {
        NodeId node_id{};
        RasterCacheEpoch::Token token{};
        Rect local_extent{};
        float device_scale{1.0f};

        [[nodiscard]] bool operator==(const RasterCacheKey& other) const noexcept {
            return node_id == other.node_id &&
                   token.same_content(other.token) &&
                   local_extent.x == other.local_extent.x &&
                   local_extent.y == other.local_extent.y &&
                   local_extent.w == other.local_extent.w &&
                   local_extent.h == other.local_extent.h &&
                   device_scale == other.device_scale;
        }
    };

    using CachedResource =
        std::variant<sk_sp<SkShader>, sk_sp<SkImageFilter>, sk_sp<SkImage>>;

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

    static constexpr std::size_t kMaxRetainedEntries =
        kRenderResourceMaxRetainedEntries;
    static constexpr std::size_t kMaxAccountedBytes =
        kRenderResourceMaxAccountedBytes;

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
    [[nodiscard]] ImageTextureAcquisition acquire_linear_gradient(
        const LinearGradient& gradient,
        Factory&& factory) {
        if (!GradientCacheAccess::cacheable(gradient)) {
            auto shader = std::forward<Factory>(factory)();
            return {std::move(shader), {}, false, false};
        }

        const LinearGradientCacheLookup lookup{&gradient};
        auto acquisition = resources_.acquire_with_lookup(
            lookup,
            0U,
            [&gradient] {
                return RenderResourceKey{LinearGradientCacheKey{gradient}};
            },
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        return shader_acquisition(std::move(acquisition));
    }

    template <class Factory>
    [[nodiscard]] ImageTextureAcquisition acquire_radial_gradient(
        const RadialGradient& gradient,
        Factory&& factory) {
        if (!GradientCacheAccess::cacheable(gradient)) {
            auto shader = std::forward<Factory>(factory)();
            return {std::move(shader), {}, false, false};
        }

        const RadialGradientCacheLookup lookup{&gradient};
        auto acquisition = resources_.acquire_with_lookup(
            lookup,
            0U,
            [&gradient] {
                return RenderResourceKey{RadialGradientCacheKey{gradient}};
            },
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        return shader_acquisition(std::move(acquisition));
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
            image_texture_retained_storage_bytes(texture),
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        return shader_acquisition(std::move(acquisition));
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
            ShaderBrushAccess::retained_storage_bytes(snapshot),
            [&factory]() -> std::shared_ptr<CachedResource> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<CachedResource>(std::move(shader));
            });
        return shader_acquisition(std::move(acquisition));
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

    [[nodiscard]] sk_sp<SkImage> find_raster(
        const RasterCacheKey& key) {
        auto acquisition = resources_.find(RenderResourceKey{key});
        if (!acquisition) return {};
        retain_frame_resource(acquisition.resource, true);
        const auto* image = std::get_if<sk_sp<SkImage>>(acquisition.resource.get());
        return image ? *image : sk_sp<SkImage>{};
    }

    [[nodiscard]] sk_sp<SkImage> retain_raster(
        const RasterCacheKey& key,
        std::size_t accounted_bytes,
        sk_sp<SkImage> image) {
        if (!image) return {};
        auto acquisition = resources_.acquire(
            RenderResourceKey{key},
            accounted_bytes,
            [image = std::move(image)]() mutable -> std::shared_ptr<CachedResource> {
                return std::make_shared<CachedResource>(std::move(image));
            });
        if (!acquisition) return {};
        retain_frame_resource(acquisition.resource, acquisition.retained);
        const auto* retained =
            std::get_if<sk_sp<SkImage>>(acquisition.resource.get());
        return retained ? *retained : sk_sp<SkImage>{};
    }

    [[nodiscard]] std::size_t retained_entries() const noexcept {
        return resources_.retained_entries();
    }

    [[nodiscard]] std::size_t retained_accounted_bytes() const noexcept {
        return resources_.retained_accounted_bytes();
    }

    [[nodiscard]] std::size_t transient_frame_lease_capacity_for_test() const noexcept {
        return transient_frame_leases_.capacity();
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

    using RenderResourceKey = std::variant<
        ImageTextureCacheKey,
        RuntimeShaderKey,
        EffectCacheKey,
        LinearGradientCacheKey,
        RadialGradientCacheKey,
        RasterCacheKey>;

    static_assert(std::is_nothrow_destructible_v<CachedResource>,
                  "cached backend resources must tear down without throwing");
    static_assert(std::is_nothrow_destructible_v<RenderResourceKey>,
                  "render-resource cache keys must tear down without throwing");

    struct RenderResourceKeyHash final {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(
            const RenderResourceKey& key) const noexcept {
            return std::visit(
                [](const auto& value) noexcept -> std::size_t {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, ImageTextureCacheKey>) {
                        return ImageTextureCacheKeyHash{}(value);
                    } else if constexpr (std::is_same_v<Value, RuntimeShaderKey>) {
                        return ShaderBrushAccess::semantic_hash(value.snapshot);
                    } else if constexpr (std::is_same_v<Value, EffectCacheKey>) {
                        return EffectCacheKeyHash{}(value);
                    } else if constexpr (std::is_same_v<Value, LinearGradientCacheKey>) {
                        return LinearGradientCacheKeyHash{}(value);
                    } else if constexpr (std::is_same_v<Value, RadialGradientCacheKey>) {
                        return RadialGradientCacheKeyHash{}(value);
                    } else {
                        std::size_t hash = std::hash<NodeId>{}(value.node_id);
                        hash ^= std::hash<std::uint64_t>{}(value.token.generation()) +
                            0x9e3779b9U + (hash << 6U) + (hash >> 2U);
                        const float values[] = {
                            value.local_extent.x, value.local_extent.y,
                            value.local_extent.w, value.local_extent.h,
                            value.device_scale};
                        for (const float item : values) {
                            const auto part = std::hash<float>{}(item == 0.0f ? 0.0f : item);
                            hash ^= part + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
                        }
                        return hash;
                    }
                },
                key);
        }

        [[nodiscard]] std::size_t operator()(
            LinearGradientCacheLookup lookup) const noexcept {
            return LinearGradientCacheKeyHash{}(lookup);
        }

        [[nodiscard]] std::size_t operator()(
            RadialGradientCacheLookup lookup) const noexcept {
            return RadialGradientCacheKeyHash{}(lookup);
        }
    };

    struct RenderResourceKeyEqual final {
        using is_transparent = void;

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
                    } else if constexpr (std::is_same_v<Left, EffectCacheKey>) {
                        return left == right;
                    } else if constexpr (std::is_same_v<Left, LinearGradientCacheKey>) {
                        return LinearGradientCacheKeyEqual{}(left, right);
                    } else if constexpr (std::is_same_v<Left, RadialGradientCacheKey>) {
                        return RadialGradientCacheKeyEqual{}(left, right);
                    } else {
                        return left == right;
                    }
                },
                a, b);
        }

        [[nodiscard]] bool operator()(
            const RenderResourceKey& key,
            LinearGradientCacheLookup lookup) const noexcept {
            const auto* gradient =
                std::get_if<LinearGradientCacheKey>(&key);
            return gradient &&
                   LinearGradientCacheKeyEqual{}(*gradient, lookup);
        }

        [[nodiscard]] bool operator()(
            LinearGradientCacheLookup lookup,
            const RenderResourceKey& key) const noexcept {
            return (*this)(key, lookup);
        }

        [[nodiscard]] bool operator()(
            const RenderResourceKey& key,
            RadialGradientCacheLookup lookup) const noexcept {
            const auto* gradient =
                std::get_if<RadialGradientCacheKey>(&key);
            return gradient &&
                   RadialGradientCacheKeyEqual{}(*gradient, lookup);
        }

        [[nodiscard]] bool operator()(
            RadialGradientCacheLookup lookup,
            const RenderResourceKey& key) const noexcept {
            return (*this)(key, lookup);
        }
    };

    using ResourceCache = RenderResourceCache<
        RenderResourceKey,
        CachedResource,
        RenderResourceKeyHash,
        RenderResourceKeyEqual>;

    [[nodiscard]] ImageTextureAcquisition shader_acquisition(
        ResourceCache::Acquisition acquisition) {
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

        // Transient resources are allowed to exceed the retained cache bounds
        // for one active frame, so their lease count is intentionally
        // unbounded by kMaxRetainedEntries. Do not let a pathological frame
        // turn that temporary usage into permanent per-view bookkeeping
        // capacity after the resources themselves have been released.
        std::vector<std::shared_ptr<const CachedResource>>{}
            .swap(transient_frame_leases_);
    }
    ResourceCache resources_;
    std::array<std::shared_ptr<const CachedResource>,
               kMaxRetainedEntries> retained_frame_leases_{};
    std::vector<std::shared_ptr<const CachedResource>> transient_frame_leases_;
    std::size_t retained_frame_lease_count_{};
    bool frame_active_{};
};

} // namespace ui::detail
