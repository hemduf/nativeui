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
/// Encoded bytes are copied during decode, so the caller does not need to keep
/// the source buffer alive. Renderer-specific ownership stays behind ImageData
/// and is never exposed through the public API. Decoding may allocate and is a
/// resource-preparation/UI-domain operation, never a real-time audio callback
/// operation.
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
          destination_(other.destination_) {
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
        other.reset();
        return *this;
    }

    ~ImageTexture() noexcept = default;

    [[nodiscard]] bool valid() const noexcept { return image_.valid(); }
    [[nodiscard]] const Image& image() const noexcept { return image_; }
    [[nodiscard]] Rect source() const noexcept { return source_; }
    [[nodiscard]] Rect destination() const noexcept { return destination_; }

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
    }

    Image image_;
    Rect source_{};
    Rect destination_{};
};

static_assert(std::is_nothrow_copy_constructible_v<ImageTexture>);
static_assert(std::is_nothrow_copy_assignable_v<ImageTexture>);
static_assert(std::is_nothrow_move_constructible_v<ImageTexture>);
static_assert(std::is_nothrow_move_assignable_v<ImageTexture>);
static_assert(std::is_nothrow_destructible_v<ImageTexture>);

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
