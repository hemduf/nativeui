#include <nativeui/image.hpp>
#include <nativeui/paint.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "detail/image_texture_test_seams.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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

[[nodiscard]] SkTileMode backend_tile_mode(TextureTileMode mode) noexcept {
    switch (mode) {
        case TextureTileMode::Clamp: return SkTileMode::kClamp;
        case TextureTileMode::Repeat: return SkTileMode::kRepeat;
        case TextureTileMode::Mirror: return SkTileMode::kMirror;
        case TextureTileMode::Decal: return SkTileMode::kDecal;
    }
    // Forged public enum values are outside the contract. Fail deterministic.
    return SkTileMode::kClamp;
}

[[nodiscard]] SkFilterMode backend_filter(TextureFilter filter) noexcept {
    switch (filter) {
        case TextureFilter::Nearest: return SkFilterMode::kNearest;
        case TextureFilter::Linear: return SkFilterMode::kLinear;
    }
    return SkFilterMode::kLinear;
}

[[nodiscard]] SkMipmapMode backend_mipmap(TextureMipmap mipmap) noexcept {
    switch (mipmap) {
        case TextureMipmap::None: return SkMipmapMode::kNone;
        case TextureMipmap::Nearest: return SkMipmapMode::kNearest;
        case TextureMipmap::Linear: return SkMipmapMode::kLinear;
    }
    return SkMipmapMode::kNone;
}

[[nodiscard]] SkSamplingOptions backend_sampling(TextureSampling sampling) noexcept {
    return SkSamplingOptions{
        backend_filter(sampling.filter()),
        backend_mipmap(sampling.mipmap())};
}

[[nodiscard]] bool exact_identity(Transform2D transform) noexcept {
    return transform.m00 == 1.0f &&
           transform.m01 == 0.0f &&
           transform.m02 == 0.0f &&
           transform.m10 == 0.0f &&
           transform.m11 == 1.0f &&
           transform.m12 == 0.0f;
}

[[nodiscard]] bool compose_texture_local_matrix(
    const SkMatrix& source_to_destination,
    Transform2D texture_to_local,
    SkMatrix& result) noexcept {
    // Keep the identity path byte-for-byte equivalent to T083-T085.
    if (exact_identity(texture_to_local)) {
        result = source_to_destination;
        return result.isFinite() && result.invert().has_value();
    }

    const Transform2D base{
        source_to_destination.getScaleX(),
        source_to_destination.getSkewX(),
        source_to_destination.getTranslateX(),
        source_to_destination.getSkewY(),
        source_to_destination.getScaleY(),
        source_to_destination.getTranslateY(),
    };
    const Transform2D composed = texture_to_local * base;
    result = SkMatrix::MakeAll(
        composed.m00, composed.m01, composed.m02,
        composed.m10, composed.m11, composed.m12,
        0.0f, 0.0f, 1.0f);

    // ImageTexture::valid() already owns semantic transform validity through
    // T098. This check is only the existing backend representability boundary
    // for the complete source -> destination -> texture_to_local mapping.
    return result.isFinite() && result.invert().has_value();
}

[[nodiscard]] bool is_full_source(Rect source, const ImageData& data) noexcept {
    return source.x == 0.0f && source.y == 0.0f &&
           source.w == data.size.w && source.h == data.size.h;
}

[[nodiscard]] bool integral_source(Rect source) noexcept {
    return std::floor(source.x) == source.x &&
           std::floor(source.y) == source.y &&
           std::floor(source.w) == source.w &&
           std::floor(source.h) == source.h;
}

struct MipSource {
    sk_sp<SkImage> image;
    SkRect source{};
};

[[nodiscard]] MipSource make_isolated_mip_source(
    const ImageData& data,
    Rect source,
    TextureFilter filter) {
    if (!data.image || !drawable_rect(source)) return {};

    // A full-image pyramid cannot observe pixels outside the selected source.
    if (is_full_source(source, data)) {
        auto image = data.image->withDefaultMipmaps();
        if (!image) return {};
        return {std::move(image),
                SkRect::MakeWH(static_cast<float>(data.image->width()),
                               static_cast<float>(data.image->height()))};
    }

    // Preserve exact texels for integral subsets. Request mipmaps on the subset
    // itself, never on the parent image, so lower levels cannot blend neighboring
    // parent pixels into the public selected-source contract.
    if (integral_source(source) &&
        source.x <= static_cast<float>(std::numeric_limits<int>::max()) &&
        source.y <= static_cast<float>(std::numeric_limits<int>::max()) &&
        source.w <= static_cast<float>(std::numeric_limits<int>::max()) &&
        source.h <= static_cast<float>(std::numeric_limits<int>::max())) {
        const auto subset = SkIRect::MakeXYWH(
            static_cast<int>(source.x), static_cast<int>(source.y),
            static_cast<int>(source.w), static_cast<int>(source.h));
        auto image = data.image->makeSubset(
            nullptr, subset, SkImage::RequiredProperties{true});
        if (image && !image->hasMipmaps()) image = image->withDefaultMipmaps();
        if (!image) return {};
        return {std::move(image),
                SkRect::MakeWH(source.w, source.h)};
    }

    // Fractional T083 source rectangles cannot be represented by SkImage's
    // integer subset API. Rasterize exactly that selected region into a private
    // picture-backed image before generating its pyramid. The strict source
    // constraint ensures parent pixels outside `source` never enter any level.
    const float width_f = std::ceil(source.w);
    const float height_f = std::ceil(source.h);
    if (!std::isfinite(width_f) || !std::isfinite(height_f) ||
        width_f < 1.0f || height_f < 1.0f ||
        width_f > static_cast<float>(std::numeric_limits<int>::max()) ||
        height_f > static_cast<float>(std::numeric_limits<int>::max())) {
        return {};
    }

    const int width = static_cast<int>(width_f);
    const int height = static_cast<int>(height_f);
    const SkRect tile = SkRect::MakeWH(
        static_cast<float>(width), static_cast<float>(height));
    const SkRect subset =
        SkRect::MakeXYWH(source.x, source.y, source.w, source.h);

    SkPictureRecorder recorder;
    SkCanvas* recording = recorder.beginRecording(tile);
    if (!recording) return {};
    recording->drawImageRect(
        data.image.get(), subset, tile,
        SkSamplingOptions(backend_filter(filter)),
        nullptr, SkCanvas::kStrict_SrcRectConstraint);

    auto picture = recorder.finishRecordingAsPicture();
    if (!picture) return {};

    auto isolated = SkImages::DeferredFromPicture(
        std::move(picture), SkISize::Make(width, height),
        nullptr, nullptr, SkImages::BitDepth::kU8,
        data.image->refColorSpace());
    if (!isolated) return {};

    auto image = isolated->withDefaultMipmaps();
    if (!image) return {};
    return {std::move(image), tile};
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
    const auto source_to_destination =
        SkMatrix::Rect2Rect(subset, destination_rect);
    if (!source_to_destination) return {};

    SkMatrix local_matrix;
    if (!compose_texture_local_matrix(
            *source_to_destination, texture.transform(), local_matrix)) {
        return {};
    }

    const auto public_sampling = texture.sampling();
    const SkFilterMode filter = backend_filter(public_sampling.filter());

    if (public_sampling.mipmap() != TextureMipmap::None) {
        auto mip_source = make_isolated_mip_source(
            *data, source, public_sampling.filter());
        if (!mip_source.image) return {};

        const auto mip_source_to_destination =
            SkMatrix::Rect2Rect(mip_source.source, destination_rect);
        if (!mip_source_to_destination) return {};

        SkMatrix mip_local_matrix;
        if (!compose_texture_local_matrix(
                *mip_source_to_destination, texture.transform(), mip_local_matrix)) {
            return {};
        }

        return mip_source.image->makeShader(
            backend_tile_mode(texture.tile_mode_x()),
            backend_tile_mode(texture.tile_mode_y()),
            backend_sampling(public_sampling),
            mip_local_matrix);
    }

    const SkSamplingOptions sampling{filter};

    if (texture.tile_mode_x() == TextureTileMode::Clamp &&
        texture.tile_mode_y() == TextureTileMode::Clamp) {
        // Preserve the exact T083 Clamp/Clamp path for the default Linear/None
        // sampling policy. Other filters use the same crop isolation/mapping.
        auto shader = data->image->makeShader(
            SkTileMode::kClamp,
            SkTileMode::kClamp,
            sampling);
        if (!shader) return {};

        // CoordClamp bounds coordinates, while the image shader still owns the
        // filter footprint. Clamp to the centers of the first/last texels
        // intersected by the selected source rectangle so filtering cannot pull
        // a neighboring texel from outside that selection.
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

    // Repeat/Mirror/Decal must tile exactly the selected (possibly fractional)
    // T083 source rectangle without ever exposing neighboring image pixels.
    // SkImage::makeSubset only accepts integer bounds, while the public T083
    // contract permits fractional source rectangles. Record only the selected
    // rectangle into a picture, then tile that picture using the exact source
    // rectangle as its tileRect. The recorded draw keeps Skia's strict source
    // constraint, so pixels outside the selected source are never part of the
    // repeatable pattern.
    SkPictureRecorder recorder;
    SkCanvas* recording = recorder.beginRecording(subset);
    if (!recording) return {};
    recording->drawImageRect(
        data->image.get(),
        subset,
        subset,
        sampling,
        nullptr,
        SkCanvas::kStrict_SrcRectConstraint);

    auto picture = recorder.finishRecordingAsPicture();
    if (!picture) return {};

    // SkPictureShader rasterizes tileRect into an intermediate tile image whose
    // coordinate origin is (0, 0), even when tileRect itself has a non-zero
    // source origin. Map that tile-local rectangle to the public destination;
    // mapping the original subset here would leak its source offset into Decal
    // and periodic boundary decisions.
    const auto tile_to_destination = SkMatrix::Rect2Rect(
        SkRect::MakeWH(subset.width(), subset.height()), destination_rect);
    if (!tile_to_destination) return {};

    SkMatrix tile_local_matrix;
    if (!compose_texture_local_matrix(
            *tile_to_destination, texture.transform(), tile_local_matrix)) {
        return {};
    }

    return picture->makeShader(
        backend_tile_mode(texture.tile_mode_x()),
        backend_tile_mode(texture.tile_mode_y()),
        filter,
        &tile_local_matrix,
        &subset);
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