#pragma once

#include "image_texture_cache_key.hpp"
#include "render_resource_cache.hpp"

#include "include/core/SkShader.h"

#include <array>
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
            0U,
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
