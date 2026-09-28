#pragma once

#include "image_texture_cache_key.hpp"
#include "render_resource_cache.hpp"

#include "include/core/SkShader.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace ui::detail {

class RenderResourceMaterializationContext final {
public:
    struct ImageTextureAcquisition final {
        sk_sp<SkShader> shader;
        std::shared_ptr<const sk_sp<SkShader>> frame_lease;
        bool hit{false};
        bool retained{false};

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(shader);
        }
    };

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

        auto acquisition = image_shaders_.acquire(
            *key,
            retained_storage_bytes(texture),
            [&factory]() -> std::shared_ptr<sk_sp<SkShader>> {
                auto shader = std::forward<Factory>(factory)();
                if (!shader) return {};
                return std::make_shared<sk_sp<SkShader>>(std::move(shader));
            });
        if (!acquisition) return {};

        retain_frame_resource(acquisition.resource, acquisition.retained);
        return {
            *acquisition.resource,
            std::move(acquisition.resource),
            acquisition.hit,
            acquisition.retained};
    }

    [[nodiscard]] std::size_t retained_entries() const noexcept {
        return image_shaders_.retained_entries();
    }

    [[nodiscard]] std::size_t retained_accounted_bytes() const noexcept {
        return image_shaders_.retained_accounted_bytes();
    }

    void clear() noexcept {
        release_frame_resources();
        frame_active_ = false;
        image_shaders_.clear();
    }

private:
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
        const std::shared_ptr<const sk_sp<SkShader>>& resource,
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
    using ImageShaderCache = RenderResourceCache<
        ImageTextureCacheKey,
        sk_sp<SkShader>,
        ImageTextureCacheKeyHash>;

    ImageShaderCache image_shaders_;
    std::array<std::shared_ptr<const sk_sp<SkShader>>,
               kMaxRetainedEntries> retained_frame_leases_{};
    std::vector<std::shared_ptr<const sk_sp<SkShader>>> transient_frame_leases_;
    std::size_t retained_frame_lease_count_{};
    bool frame_active_{};
};

} // namespace ui::detail
