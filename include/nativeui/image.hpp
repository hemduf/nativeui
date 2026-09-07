#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/resource.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
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
/// and is never exposed through the public API.
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
/// decoding on paint paths. `clear()` explicitly invalidates the cache.
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
