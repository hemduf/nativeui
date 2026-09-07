#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <utility>

namespace ui {

class Painter;
class Image;

enum class ImageFit {
    Fill,
    Contain,
    Cover,
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

} // namespace ui
