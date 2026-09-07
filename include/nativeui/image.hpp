#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

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

/// Supplies encoded resource bytes by application-defined identifier.
///
/// NativeUI deliberately does not expose filesystem loading through widgets or
/// ImageCache. Applications can source bytes from files, bundles, archives or
/// generated memory while keeping that policy outside the UI toolkit.
/// Implementations are called from the UI/resource-preparation domain unless a
/// concrete provider explicitly documents a stronger thread-safety contract.
class ResourceProvider {
public:
    virtual ~ResourceProvider() = default;

    [[nodiscard]] virtual std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) = 0;
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
