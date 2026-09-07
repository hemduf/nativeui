#pragma once

#include <nativeui/geometry.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace ui {

class Painter;
class ResourceProvider;
class SvgIcon;

namespace detail {
struct SvgData;
struct SvgAccess;
void draw_svg(Painter& painter, const SvgIcon& icon, Rect destination);
} // namespace detail

/// Copyable backend-neutral handle to a parsed scalable SVG resource.
///
/// Parsing copies the encoded SVG bytes into the private Skia-backed DOM, so
/// callers do not retain source-buffer lifetime obligations. No Skia type is
/// exposed by this public API.
class SvgIcon {
public:
    SvgIcon() = default;

    [[nodiscard]] static SvgIcon parse(std::span<const std::byte> encoded);

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(data_); }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] Size intrinsic_size() const noexcept;

    friend bool operator==(const SvgIcon&, const SvgIcon&) = default;

private:
    explicit SvgIcon(std::shared_ptr<const detail::SvgData> data)
        : data_(std::move(data)) {}

    std::shared_ptr<const detail::SvgData> data_;

    friend struct detail::SvgAccess;
};

enum class SvgLoadError {
    None,
    NotFound,
    ParseFailed,
};

struct SvgLoadResult {
    SvgIcon icon;
    SvgLoadError error{SvgLoadError::None};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SvgLoadError::None && icon.valid();
    }
};

/// Reusable parsed-SVG cache associated with one ResourceProvider.
///
/// Successful icons and failures are cached by resource identifier so widget
/// paint paths never perform resource I/O or XML parsing repeatedly. `clear()`
/// explicitly invalidates the cache.
class SvgCache {
public:
    explicit SvgCache(ResourceProvider& provider);
    ~SvgCache();

    SvgCache(const SvgCache&) = delete;
    SvgCache& operator=(const SvgCache&) = delete;
    SvgCache(SvgCache&&) noexcept;
    SvgCache& operator=(SvgCache&&) noexcept;

    [[nodiscard]] SvgLoadResult load(std::string_view resource_id);
    [[nodiscard]] std::size_t size() const noexcept;
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
