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
/// exposed by this public API. Parsing may allocate and perform XML work; it is
/// a resource-preparation/UI-domain operation, never a real-time audio callback
/// operation.
///
/// V1 SVG resources are static and self-contained. NativeUI does not fetch
/// external file/network resources or drive SVG animation. Rendering through
/// `CanvasContext2D::draw_svg()` preserves the icon's intrinsic aspect ratio
/// using a centered contain-style fit, clips to the supplied destination, and
/// treats non-positive or non-finite destination rectangles as safe no-ops.
class SvgIcon {
public:
    /// Construct an invalid, empty handle. `intrinsic_size()` returns {0, 0}.
    SvgIcon() = default;

    /// Parse encoded SVG bytes into an immutable owned DOM.
    ///
    /// `encoded` is borrowed only for this call. Empty/malformed SVG, parser
    /// failure, or lack of a finite positive intrinsic size returns an invalid
    /// icon. The parsed container size is used when valid; a finite positive
    /// root `viewBox` width/height is the fallback intrinsic size. Parsing may
    /// allocate and XML-parse; C++ allocation exceptions may propagate.
    [[nodiscard]] static SvgIcon parse(std::span<const std::byte> encoded);

    /// Whether this handle owns a parsed DOM with valid intrinsic geometry.
    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(data_); }
    /// Equivalent to `valid()`.
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
    /// Finite positive intrinsic dimensions in SVG user/logical units; zero when invalid.
    [[nodiscard]] Size intrinsic_size() const noexcept;

    /// Handle-identity equality: copied handles compare equal; separately parsed
    /// byte-identical SVG documents are not required to compare equal.
    friend bool operator==(const SvgIcon&, const SvgIcon&) = default;

private:
    explicit SvgIcon(std::shared_ptr<const detail::SvgData> data)
        : data_(std::move(data)) {}

    std::shared_ptr<const detail::SvgData> data_;

    friend struct detail::SvgAccess;
};

/// Resource-cache outcome for SVG loading.
enum class SvgLoadError {
    /// Resource bytes were found and parsed into a valid SvgIcon.
    None,
    /// The ResourceProvider returned std::nullopt for the exact identifier.
    NotFound,
    /// Bytes were returned, but parsing/intrinsic-geometry validation failed.
    ParseFailed,
};

/// Owned result returned by SvgCache::load().
struct SvgLoadResult {
    /// Parsed shared handle on success; invalid on failure.
    SvgIcon icon;
    /// Stable cached outcome for the requested resource identifier.
    SvgLoadError error{SvgLoadError::None};

    /// Success requires both SvgLoadError::None and a valid SvgIcon handle.
    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SvgLoadError::None && icon.valid();
    }
};

/// Reusable parsed-SVG cache associated with one ResourceProvider.
///
/// Successful icons and failures are cached by resource identifier so widget
/// paint paths never perform resource I/O or XML parsing repeatedly. `clear()`
/// explicitly invalidates this cache only; caches owned by other UI/plugin
/// instances are unaffected.
///
/// The provider is borrowed and **must outlive the SvgCache**. `load()` and
/// `clear()` are not synchronized and belong to the UI/resource-preparation
/// domain. Do not call them from a real-time audio callback.
class SvgCache {
public:
    /// Bind a cache to `provider`; the provider is borrowed and must outlive the cache.
    ///
    /// Construction establishes no ownership cycle and performs no provider or
    /// parser callback. Internal cache allocation/setup may throw.
    explicit SvgCache(ResourceProvider& provider);

    /// Destroy cached entries without destroying the borrowed provider.
    ///
    /// SvgIcon handles already copied out remain alive through shared parsed backing.
    ~SvgCache();

    /// SvgCache has unique mutable cache identity and is not copyable.
    SvgCache(const SvgCache&) = delete;
    /// SvgCache has unique mutable cache identity and is not copy-assignable.
    SvgCache& operator=(const SvgCache&) = delete;

    /// Transfer cache contents and the borrowed provider association.
    ///
    /// The moved-from cache is valid but empty/unbound: load() reports NotFound.
    /// Existing SvgIcon handles copied out before the move remain independently valid.
    SvgCache(SvgCache&&) noexcept;

    /// Transfer cache contents/provider association with the same moved-from contract.
    ///
    /// Any entries previously owned by this cache are released first; copied-out
    /// SvgIcon handles keep their backing alive.
    SvgCache& operator=(SvgCache&&) noexcept;

    /// Load once by exact resource identifier and cache both success and failure.
    ///
    /// `resource_id` is borrowed only for this synchronous call and copied into
    /// the cache key on a miss. No normalization, case folding or fallback lookup
    /// is added by SvgCache. `std::nullopt` maps to NotFound; returned bytes that
    /// fail SvgIcon::parse(), including an empty vector, map to ParseFailed.
    ///
    /// A cached entry suppresses provider/parse retries until `clear()`. Provider
    /// callbacks and parsing execute synchronously in the caller's UI/resource-
    /// preparation domain. Allocation/provider exceptions propagate and are not
    /// converted to SvgLoadError or cached as a terminal result.
    [[nodiscard]] SvgLoadResult load(std::string_view resource_id);
    /// Return the number of cached identifiers, including cached failure entries.
    ///
    /// This is an unsynchronized read of this cache instance and performs no
    /// provider/parser work.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Remove all cached successes/failures without affecting the provider.
    ///
    /// Existing SvgIcon values copied out keep their parsed DOM alive. A later
    /// load of the same identifier may call the provider/parser again.
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
