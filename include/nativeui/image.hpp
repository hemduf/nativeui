#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/resource.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ui {

class Painter;
class Image;

enum class ImageFit {
    Fill,
    Contain,
    Cover,
};

/// Per-axis behavior for ImageTexture coordinates outside one destination period.
enum class TextureTileMode {
    Clamp,  ///< Extend the nearest selected-source edge.
    Repeat, ///< Repeat complete periods using floor-based periodic coordinates.
    Mirror, ///< Repeat periods while mirroring every odd mathematical period.
    Decal,  ///< Return transparent black outside the base period.
};

/// Filtering used inside one selected image level.
enum class TextureFilter {
    Nearest,
    Linear,
};

/// Selection/interpolation policy for lower-resolution image levels.
enum class TextureMipmap {
    None,
    Nearest,
    Linear,
};

/// Interpretation applied when an ImageTexture samples decoded image channels.
enum class TextureInterpretation {
    Color, ///< Color-managed RGB with normal alpha coverage semantics.
    Data,  ///< Raw normalized channels with no RGB color conversion or implicit premultiplication.
};

/// Small backend-neutral ImageTexture sampling description.
///
/// Mutation is value-only and never allocates, decodes Image data, generates
/// mipmaps, creates backend resources, invokes callbacks, or touches a cache.
class TextureSampling {
public:
    constexpr TextureSampling() noexcept = default;

    constexpr TextureSampling& set_filter(TextureFilter filter) noexcept {
        filter_ = filter;
        return *this;
    }

    constexpr TextureSampling& set_mipmap(TextureMipmap mipmap) noexcept {
        mipmap_ = mipmap;
        return *this;
    }

    [[nodiscard]] constexpr TextureFilter filter() const noexcept {
        return filter_;
    }

    [[nodiscard]] constexpr TextureMipmap mipmap() const noexcept {
        return mipmap_;
    }

private:
    TextureFilter filter_{TextureFilter::Linear};
    TextureMipmap mipmap_{TextureMipmap::None};
};

static_assert(std::is_trivially_copyable_v<TextureSampling>);
static_assert(noexcept(std::declval<TextureSampling&>().set_filter(
    TextureFilter::Nearest)));
static_assert(noexcept(std::declval<TextureSampling&>().set_mipmap(
    TextureMipmap::Linear)));
static_assert(noexcept(std::declval<const TextureSampling&>().filter()));
static_assert(noexcept(std::declval<const TextureSampling&>().mipmap()));

enum class ImageLoadError {
    None,
    NotFound,
    DecodeFailed,
};

namespace detail {
struct ImageData;
struct ImageAccess;
void draw_image(Painter& painter, const Image& image, Rect destination, ImageFit fit);
void draw_image(Painter& painter,
                const Image& image,
                Rect source,
                Rect destination,
                ImageFit fit);
} // namespace detail

/// Copyable backend-neutral handle to a decoded image resource.
///
/// Encoded bytes are copied and fully raster-decoded before the Image is
/// published, so the caller does not need to keep the source buffer alive and
/// paint paths never perform encoded-image decoding. Renderer-specific ownership
/// stays behind ImageData and is never exposed through the public API. Decoding
/// may allocate and is a resource-preparation/UI-domain operation, never a
/// real-time audio callback operation.
class Image {
public:
    Image() = default;

    [[nodiscard]] static Image decode(std::span<const std::byte> encoded);

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(data_); }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] Size size() const noexcept;

    friend bool operator==(const Image&, const Image&) = default;

private:
    explicit Image(std::shared_ptr<const detail::ImageData> data)
        : data_(std::move(data)) {}

    std::shared_ptr<const detail::ImageData> data_;

    friend struct detail::ImageAccess;
};


/// Backend-neutral value description for an image-backed Brush source.
///
/// `source_pixels` is expressed in decoded-image pixel coordinates and may be
/// fractional. `destination` is expressed in the Painter's current local
/// logical coordinates; it defines the texture mapping, not primitive bounds or
/// clipping. Source/destination rectangles must be finite with positive size,
/// and the complete source rectangle must stay inside the Image bounds.
///
/// Invalid construction canonicalizes to an inert state (invalid Image plus zero
/// rectangles). Copying shares the immutable Image backing; no decode, backend
/// materialization, or full-raster copy occurs when constructing/copying an
/// ImageTexture or converting it to a Brush. A logically valid finite mapping
/// that cannot be represented by the renderer's affine matrix fails visibly at
/// paint time rather than silently substituting a different mapping.
///
/// The destination rectangle defines one complete texture period. Tiling is
/// independent per axis and defaults to Clamp/Clamp. Sampling defaults to Linear
/// filtering with no mipmaps, preserving the T083 behavior. Tile/sampling
/// mutation changes only this value description: it does not allocate,
/// decode/copy Image data, generate mipmaps, create backend resources, invoke
/// callbacks, or mutate process-global caches. Mipmap/backend materialization is
/// renderer-owned and never stored by the public ImageTexture value.
///
/// An optional texture-local affine transform maps the T083 destination/pattern
/// coordinates into Painter-local logical coordinates before the Painter/device
/// transform. Identity preserves T083-T085 behavior. Transform mutation is
/// allocation/decode/backend-free and semantic validity is decided exclusively
/// by Transform2D::inverse(); invalid transform values are retained verbatim so
/// callers can inspect/recover them without losing Image/mapping/sampling state.
///
/// ImageTexture itself is not a synchronization primitive; mutate an instance
/// only from the owning UI/resource-preparation domain. Texture materialization
/// is not real-time-audio-safe.
class ImageTexture {
public:
    ImageTexture() = default;

    ImageTexture(Image image, Rect destination) noexcept
        : image_(std::move(image)),
          destination_(destination) {
        const auto image_size = image_.size();
        source_ = Rect{0.0f, 0.0f, image_size.w, image_size.h};
        canonicalize();
    }

    ImageTexture(Image image, Rect source_pixels, Rect destination) noexcept
        : image_(std::move(image)),
          source_(source_pixels),
          destination_(destination) {
        canonicalize();
    }

    ImageTexture(const ImageTexture&) noexcept = default;
    ImageTexture& operator=(const ImageTexture&) noexcept = default;

    ImageTexture(ImageTexture&& other) noexcept
        : image_(std::move(other.image_)),
          source_(other.source_),
          destination_(other.destination_),
          tile_mode_x_(other.tile_mode_x_),
          tile_mode_y_(other.tile_mode_y_),
          sampling_(other.sampling_),
          interpretation_(other.interpretation_),
          transform_(other.transform_),
          transform_valid_(other.transform_valid_) {
        other.reset();
    }

    ImageTexture& operator=(ImageTexture&& other) noexcept {
        if (this == &other) {
            reset();
            return *this;
        }
        image_ = std::move(other.image_);
        source_ = other.source_;
        destination_ = other.destination_;
        tile_mode_x_ = other.tile_mode_x_;
        tile_mode_y_ = other.tile_mode_y_;
        sampling_ = other.sampling_;
        interpretation_ = other.interpretation_;
        transform_ = other.transform_;
        transform_valid_ = other.transform_valid_;
        other.reset();
        return *this;
    }

    ~ImageTexture() noexcept = default;

    [[nodiscard]] bool valid() const noexcept {
        return image_.valid() && transform_valid_;
    }
    [[nodiscard]] const Image& image() const noexcept { return image_; }
    [[nodiscard]] Rect source() const noexcept { return source_; }
    [[nodiscard]] Rect destination() const noexcept { return destination_; }

    /// Change per-axis tiling without touching shared Image/backend state.
    ImageTexture& set_tile_mode(TextureTileMode x,
                                TextureTileMode y) noexcept {
        tile_mode_x_ = x;
        tile_mode_y_ = y;
        return *this;
    }

    [[nodiscard]] TextureTileMode tile_mode_x() const noexcept {
        return tile_mode_x_;
    }

    [[nodiscard]] TextureTileMode tile_mode_y() const noexcept {
        return tile_mode_y_;
    }

    /// Change filtering/mipmap policy without touching shared Image/backend state.
    ImageTexture& set_sampling(TextureSampling sampling) noexcept {
        sampling_ = sampling;
        return *this;
    }

    [[nodiscard]] TextureSampling sampling() const noexcept {
        return sampling_;
    }

    /// Select color-managed or raw numeric sampling without mutating the shared Image.
    ImageTexture& set_interpretation(TextureInterpretation interpretation) noexcept {
        interpretation_ = interpretation;
        return *this;
    }

    [[nodiscard]] TextureInterpretation interpretation() const noexcept {
        return interpretation_;
    }

    /// Map T083 destination/pattern coordinates into Painter-local coordinates.
    /// The raw value is always retained; public validity uses the exact shared
    /// T098 Transform2D::inverse() contract.
    ImageTexture& set_transform(Transform2D texture_to_local) noexcept {
        transform_ = texture_to_local;
        transform_valid_ = transform_.inverse().has_value();
        return *this;
    }

    [[nodiscard]] Transform2D transform() const noexcept {
        return transform_;
    }

private:
    [[nodiscard]] static bool finite_rect(Rect rect) noexcept {
        return std::isfinite(rect.x) && std::isfinite(rect.y) &&
               std::isfinite(rect.w) && std::isfinite(rect.h);
    }

    [[nodiscard]] bool fields_valid() const noexcept {
        if (!image_.valid()) return false;

        const auto image_size = image_.size();
        if (!std::isfinite(image_size.w) || !std::isfinite(image_size.h) ||
            image_size.w <= 0.0f || image_size.h <= 0.0f) {
            return false;
        }

        if (!finite_rect(source_) || source_.w <= 0.0f || source_.h <= 0.0f ||
            source_.x < 0.0f || source_.y < 0.0f ||
            source_.x > image_size.w || source_.y > image_size.h ||
            source_.w > image_size.w - source_.x ||
            source_.h > image_size.h - source_.y) {
            return false;
        }

        return finite_rect(destination_) &&
               destination_.w > 0.0f && destination_.h > 0.0f;
    }

    void canonicalize() noexcept {
        if (!fields_valid()) reset();
    }

    void reset() noexcept {
        image_ = {};
        source_ = {};
        destination_ = {};
        tile_mode_x_ = TextureTileMode::Clamp;
        tile_mode_y_ = TextureTileMode::Clamp;
        sampling_ = {};
        interpretation_ = TextureInterpretation::Color;
        transform_ = Transform2D::identity();
        transform_valid_ = true;
    }

    Image image_;
    Rect source_{};
    Rect destination_{};
    TextureTileMode tile_mode_x_{TextureTileMode::Clamp};
    TextureTileMode tile_mode_y_{TextureTileMode::Clamp};
    TextureSampling sampling_{};
    TextureInterpretation interpretation_{TextureInterpretation::Color};
    Transform2D transform_{Transform2D::identity()};
    bool transform_valid_{true};
};

static_assert(std::is_nothrow_copy_constructible_v<ImageTexture>);
static_assert(std::is_nothrow_copy_assignable_v<ImageTexture>);
static_assert(std::is_nothrow_move_constructible_v<ImageTexture>);
static_assert(std::is_nothrow_move_assignable_v<ImageTexture>);
static_assert(std::is_nothrow_destructible_v<ImageTexture>);
static_assert(noexcept(std::declval<ImageTexture&>().set_tile_mode(
    TextureTileMode::Clamp, TextureTileMode::Clamp)));
static_assert(noexcept(std::declval<const ImageTexture&>().tile_mode_x()));
static_assert(noexcept(std::declval<const ImageTexture&>().tile_mode_y()));
static_assert(noexcept(std::declval<ImageTexture&>().set_sampling(
    TextureSampling{})));
static_assert(noexcept(std::declval<const ImageTexture&>().sampling()));
static_assert(noexcept(std::declval<ImageTexture&>().set_interpretation(
    TextureInterpretation::Data)));
static_assert(noexcept(std::declval<const ImageTexture&>().interpretation()));
static_assert(noexcept(std::declval<ImageTexture&>().set_transform(
    Transform2D::identity())));
static_assert(noexcept(std::declval<const ImageTexture&>().transform()));

struct ImageLoadResult {
    Image image;
    ImageLoadError error{ImageLoadError::None};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == ImageLoadError::None && image.valid();
    }
};

/// Reusable decoded-image cache associated with one ResourceProvider.
///
/// Successful images and failures are both cached by resource identifier so a
/// missing or malformed resource does not repeatedly hit application I/O or
/// decoding on paint paths. `clear()` explicitly invalidates this cache only;
/// caches owned by other UI/plugin instances are unaffected.
///
/// The provider is borrowed and **must outlive the ImageCache**. `load()` and
/// `clear()` are not synchronized and belong to the UI/resource-preparation
/// domain. Do not call them from a real-time audio callback.
class ImageCache {
public:
    explicit ImageCache(ResourceProvider& provider);
    ~ImageCache();

    ImageCache(const ImageCache&) = delete;
    ImageCache& operator=(const ImageCache&) = delete;
    ImageCache(ImageCache&&) noexcept;
    ImageCache& operator=(ImageCache&&) noexcept;

    [[nodiscard]] ImageLoadResult load(std::string_view resource_id);
    [[nodiscard]] std::size_t size() const noexcept;
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui