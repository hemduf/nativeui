#pragma once

#include "image_texture_cache_key.hpp"
#include "render_resource_cache.hpp"

#include "include/core/SkShader.h"

#include <cstddef>
#include <memory>
#include <utility>

namespace ui::detail {

class RenderResourceMaterializationContext final {
public:
    static constexpr std::size_t kMaxRetainedEntries = 512;
    static constexpr std::size_t kMaxAccountedBytes =
        128U * 1024U * 1024U;

    RenderResourceMaterializationContext()
        : image_shaders_({
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

    template <class Factory>
    [[nodiscard]] sk_sp<SkShader> acquire_image_texture(
        const ImageTexture& texture,
        Factory&& factory) {
        const auto key = image_texture_cache_key(texture);
        if (!key) {
            return std::forward<Factory>(factory)();
        }

        auto acquisition = image_shaders_.acquire(
            *key,
            0U,
            [&factory]() -> std::shared_ptr<sk_sp<SkShader>> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<sk_sp<SkShader>>(std::move(shader));
            });
        return acquisition ? *acquisition.resource : sk_sp<SkShader>{};
    }

    [[nodiscard]] std::size_t retained_entries() const noexcept {
        return image_shaders_.retained_entries();
    }

    [[nodiscard]] std::size_t retained_accounted_bytes() const noexcept {
        return image_shaders_.retained_accounted_bytes();
    }

    void clear() noexcept {
        image_shaders_.clear();
    }

private:
    using ImageShaderCache = RenderResourceCache<
        ImageTextureCacheKey,
        sk_sp<SkShader>,
        ImageTextureCacheKeyHash>;

    ImageShaderCache image_shaders_;
};

} // namespace ui::detail
