#include <nativeui/image.hpp>
#include <nativeui/paint.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "detail/image_texture_test_seams.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

namespace ui::detail {

struct ImageData {
    sk_sp<SkImage> image;
    Size size{};
};

struct ImageAccess {
    [[nodiscard]] static const std::shared_ptr<const ImageData>& data(const Image& image) noexcept {
        return image.data_;
    }
};

namespace {

#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
std::size_t g_image_decode_calls{};
std::size_t g_image_texture_materialization_calls{};
ImageTextureMaterializationFailurePoint g_image_texture_failure{
    ImageTextureMaterializationFailurePoint::None};
#endif

[[nodiscard]] bool finite_rect(Rect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.w) && std::isfinite(rect.h);
}

[[nodiscard]] bool drawable_rect(Rect rect) noexcept {
    return finite_rect(rect) && rect.w > 0.0f && rect.h > 0.0f;
}

[[nodiscard]] Rect full_source(const ImageData& data) noexcept {
    return Rect{0.0f, 0.0f, data.size.w, data.size.h};
}

[[nodiscard]] Rect contained_destination(Rect source, Rect destination) noexcept {
    const float scale = std::min(destination.w / source.w, destination.h / source.h);
    const float width = source.w * scale;
    const float height = source.h * scale;
    return Rect{
        destination.x + (destination.w - width) * 0.5f,
        destination.y + (destination.h - height) * 0.5f,
        width,
        height,
    };
}

[[nodiscard]] Rect covered_source(Rect source, Rect destination) noexcept {
    const float source_aspect = source.w / source.h;
    const float destination_aspect = destination.w / destination.h;

    if (source_aspect > destination_aspect) {
        const float width = source.h * destination_aspect;
        return Rect{
            source.x + (source.w - width) * 0.5f,
            source.y,
            width,
            source.h,
        };
    }

    if (source_aspect < destination_aspect) {
        const float height = source.w / destination_aspect;
        return Rect{
            source.x,
            source.y + (source.h - height) * 0.5f,
            source.w,
            height,
        };
    }

    return source;
}

void draw_resolved_image(Painter& painter,
                         const ImageData& data,
                         Rect source,
                         Rect destination,
                         ImageFit fit) {
    if (!data.image || !drawable_rect(source) || !drawable_rect(destination)) return;

    source = intersect(source, full_source(data));
    if (!drawable_rect(source)) return;

    switch (fit) {
        case ImageFit::Fill:
            break;
        case ImageFit::Contain:
            destination = contained_destination(source, destination);
            break;
        case ImageFit::Cover:
            source = covered_source(source, destination);
            break;
    }

    const auto src = SkRect::MakeXYWH(source.x, source.y, source.w, source.h);
    const auto dst = SkRect::MakeXYWH(
        destination.x, destination.y, destination.w, destination.h);
    painter.canvas().drawImageRect(
        data.image.get(),
        src,
        dst,
        SkSamplingOptions(SkFilterMode::kLinear),
        nullptr,
        SkCanvas::kStrict_SrcRectConstraint);
}

} // namespace


#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
[[nodiscard]] std::size_t image_decode_call_count_for_test() noexcept {
    return g_image_decode_calls;
}

[[nodiscard]] std::size_t image_texture_materialization_call_count_for_test() noexcept {
    return g_image_texture_materialization_calls;
}

[[nodiscard]] bool image_backing_is_lazy_for_test(const Image& image) noexcept {
    const auto& data = ImageAccess::data(image);
    return data && data->image && data->image->isLazyGenerated();
}

void set_image_texture_materialization_failure_for_test(
    ImageTextureMaterializationFailurePoint point) noexcept {
    g_image_texture_failure = point;
}

void record_image_decode_for_test() noexcept {
    ++g_image_decode_calls;
}
#endif

sk_sp<SkShader> materialize_image_texture(const ImageTexture& texture) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    ++g_image_texture_materialization_calls;
    if (g_image_texture_failure ==
        ImageTextureMaterializationFailurePoint::BeforeShader) {
        throw std::bad_alloc{};
    }
    if (g_image_texture_failure ==
        ImageTextureMaterializationFailurePoint::ForceNullShader) {
        return {};
    }
#endif

    if (!texture.valid()) return {};

    const auto& data = ImageAccess::data(texture.image());
    if (!data || !data->image) return {};

    const auto source = texture.source();
    const auto destination = texture.destination();
    const SkRect subset =
        SkRect::MakeXYWH(source.x, source.y, source.w, source.h);
    const SkRect destination_rect =
        SkRect::MakeXYWH(destination.x, destination.y,
                         destination.w, destination.h);
    const SkMatrix local_matrix =
        SkMatrix::RectToRectOrIdentity(subset, destination_rect);

    auto shader = data->image->makeShader(
        SkTileMode::kClamp,
        SkTileMode::kClamp,
        SkSamplingOptions(SkFilterMode::kLinear));
    if (!shader) return {};

    // CoordClamp bounds coordinates, while the image shader still owns the
    // linear filter footprint. Clamp to the centers of the first/last texels
    // intersected by the selected source rectangle so bilinear sampling cannot
    // pull a neighboring texel from outside that selection. This reproduces
    // strict source-subrect isolation without depending on Skia's private
    // SkImageShader::MakeSubset API.
    const float source_right = source.x + source.w;
    const float source_bottom = source.y + source.h;
    const SkRect sampling_domain = SkRect::MakeLTRB(
        std::floor(source.x) + 0.5f,
        std::floor(source.y) + 0.5f,
        std::ceil(source_right) - 0.5f,
        std::ceil(source_bottom) - 0.5f);

    shader = SkShaders::CoordClamp(std::move(shader), sampling_domain);
    if (!shader) return {};

    return shader->makeWithLocalMatrix(local_matrix);
}

void draw_image(Painter& painter, const Image& image, Rect destination, ImageFit fit) {
    const auto& data = ImageAccess::data(image);
    if (!data) return;
    draw_resolved_image(painter, *data, full_source(*data), destination, fit);
}

void draw_image(Painter& painter,
                const Image& image,
                Rect source,
                Rect destination,
                ImageFit fit) {
    const auto& data = ImageAccess::data(image);
    if (!data) return;
    draw_resolved_image(painter, *data, source, destination, fit);
}

} // namespace ui::detail

namespace ui {

Image Image::decode(std::span<const std::byte> encoded) {
#if defined(NATIVEUI_ENABLE_TEST_SEAMS)
    detail::record_image_decode_for_test();
#endif
    if (encoded.empty()) return {};

    auto bytes = SkData::MakeWithCopy(encoded.data(), encoded.size());
    if (!bytes) return {};

    auto deferred = SkImages::DeferredFromEncodedData(std::move(bytes));
    if (!deferred) return {};

    // Publish only a realized raster image. DeferredFromEncodedData may parse
    // metadata successfully while postponing pixel decode until first draw,
    // which would violate Image's resource-preparation contract and T083's
    // zero-decode paint path.
    auto image = deferred->makeRasterImage(
        nullptr, SkImage::kDisallow_CachingHint);
    if (!image) return {};

    auto data = std::make_shared<detail::ImageData>();
    data->size = Size{static_cast<float>(image->width()), static_cast<float>(image->height())};
    data->image = std::move(image);
    return Image{std::move(data)};
}

Size Image::size() const noexcept {
    return data_ ? data_->size : Size{};
}

struct ImageCache::Impl {
    explicit Impl(ResourceProvider& provider_in)
        : provider(&provider_in) {}

    ResourceProvider* provider{};
    std::unordered_map<std::string, ImageLoadResult> entries;
};

ImageCache::ImageCache(ResourceProvider& provider)
    : impl_(std::make_unique<Impl>(provider)) {}

ImageCache::~ImageCache() = default;
ImageCache::ImageCache(ImageCache&&) noexcept = default;
ImageCache& ImageCache::operator=(ImageCache&&) noexcept = default;

ImageLoadResult ImageCache::load(std::string_view resource_id) {
    if (!impl_ || !impl_->provider) {
        return {{}, ImageLoadError::NotFound};
    }

    const std::string key{resource_id};
    if (const auto existing = impl_->entries.find(key); existing != impl_->entries.end()) {
        return existing->second;
    }

    ImageLoadResult result;
    const auto encoded = impl_->provider->load(resource_id);
    if (!encoded) {
        result.error = ImageLoadError::NotFound;
    } else {
        result.image = Image::decode(*encoded);
        if (!result.image.valid()) {
            result.error = ImageLoadError::DecodeFailed;
        }
    }

    impl_->entries.emplace(key, result);
    return result;
}

std::size_t ImageCache::size() const noexcept {
    return impl_ ? impl_->entries.size() : 0U;
}

void ImageCache::clear() {
    if (impl_) impl_->entries.clear();
}

} // namespace ui
