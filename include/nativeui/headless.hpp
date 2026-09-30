#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ui {

/// One owned physical-pixel sample returned by HeadlessRenderer::pixel().
///
/// Channels are the exact bytes stored in the renderer's premultiplied-alpha
/// RGBA8888 snapshot, each in the inclusive range 0..255. The value owns no
/// renderer storage and remains valid across later renders, resizes or teardown.
struct Rgba8 {
    /// Premultiplied red channel.
    std::uint8_t r{};
    /// Premultiplied green channel.
    std::uint8_t g{};
    /// Premultiplied blue channel.
    std::uint8_t b{};
    /// Alpha channel.
    std::uint8_t a{};
};

/// Deterministic offscreen renderer backed by a Skia raster surface.
///
/// The renderer owns its raster surface and latest copied pixel snapshot but
/// never owns the UI passed to render(). Logical geometry uses NativeUI logical
/// UI pixels; scale_factor converts those logical units to physical raster
/// pixels. The raster format is premultiplied-alpha RGBA8888.
///
/// HeadlessRenderer has no internal synchronization. Construction, resize,
/// render and snapshot access must be serialized with the owning UI/main-thread
/// domain; render() executes normal component layout/paint code and may invoke
/// application paint behavior. Recursive/concurrent rendering of the same
/// renderer/UI is outside the contract. The API may allocate and is not suitable
/// for an audio/DSP real-time callback.
class HeadlessRenderer {
public:
    /// Create an offscreen raster target description.
    ///
    /// `logical_size` is expressed in logical UI pixels. `scale_factor` is a
    /// dimensionless physical-pixels-per-logical-pixel multiplier. Both logical
    /// extents and the scale must be finite and strictly positive or construction
    /// throws std::invalid_argument. Physical dimensions are independently
    /// rounded to the nearest integer and clamped to at least one pixel.
    ///
    /// Construction owns configuration only; rgba_pixels() remains empty until
    /// the first successful render(). Allocation failure propagates normally.
    explicit HeadlessRenderer(Size logical_size, float scale_factor = 1.0f);

    /// Release the raster surface and copied snapshot.
    ///
    /// Any reference previously returned by rgba_pixels() becomes invalid.
    /// No UI lifetime is owned or extended by the renderer.
    ~HeadlessRenderer();

    /// HeadlessRenderer is uniquely owned and cannot be copied.
    HeadlessRenderer(const HeadlessRenderer&) = delete;
    /// HeadlessRenderer is uniquely owned and cannot be copy-assigned.
    HeadlessRenderer& operator=(const HeadlessRenderer&) = delete;

    /// Transfer the raster surface/configuration/snapshot without throwing.
    ///
    /// References into the source snapshot must be treated as invalid after the
    /// move. The moved-from renderer is valid only for destruction or assignment.
    HeadlessRenderer(HeadlessRenderer&&) noexcept;

    /// Transfer ownership from another renderer without throwing.
    ///
    /// Existing snapshot references into either object must not be retained
    /// across the assignment. The moved-from source is valid only for
    /// destruction or assignment.
    HeadlessRenderer& operator=(HeadlessRenderer&&) noexcept;

    /// Replace the logical viewport and raster scale.
    ///
    /// Units and validation match the constructor. Validation completes before
    /// configuration mutation, so invalid input throws std::invalid_argument
    /// without changing the previous configuration. A successful call discards
    /// both the backing surface and copied snapshot even when the requested
    /// values equal the current ones; rgba_pixels() is empty until render()
    /// succeeds again. Existing snapshot references are invalidated.
    void resize(Size logical_size, float scale_factor = 1.0f);

    /// Resize `ui` to logical_size(), paint it offscreen and publish a copied
    /// premultiplied-alpha RGBA8888 snapshot.
    ///
    /// `ui` is borrowed only for this synchronous call and is never retained.
    /// Rendering uses the UI's ordinary resize/layout/paint path: viewport state
    /// can therefore change and component paint code can run. The raster is
    /// cleared before painting and scale_factor() is applied before UI paint.
    ///
    /// Returns false only when the raster surface, backend canvas or readable
    /// pixel view cannot be obtained. A false return does not synthesize pixels;
    /// after an earlier success the previous copied snapshot remains available.
    /// Allocation failures and component/layout/paint exceptions propagate
    /// instead of being translated to false, and work completed before such an
    /// exception is not transactionally rolled back.
    [[nodiscard]] bool render(UI& ui);

    /// Current NativeUI viewport size in logical UI pixels.
    [[nodiscard]] Size logical_size() const noexcept;
    /// Physical raster pixels per logical UI pixel.
    [[nodiscard]] float scale_factor() const noexcept;
    /// Current rounded physical raster width in pixels.
    [[nodiscard]] int pixel_width() const noexcept;
    /// Current rounded physical raster height in pixels.
    [[nodiscard]] int pixel_height() const noexcept;

    /// Borrow the latest successful tightly packed RGBA8888 snapshot.
    ///
    /// Rows are top-to-bottom and pixels left-to-right, with four bytes per
    /// physical pixel in R,G,B,A order and premultiplied alpha. The vector is
    /// empty before the first successful render() and after every successful
    /// resize(). A later successful render may reallocate it; resize, move and
    /// destruction also invalidate the borrowed reference. Copy the vector when
    /// bytes must outlive the current renderer snapshot.
    [[nodiscard]] const std::vector<std::uint8_t>& rgba_pixels() const noexcept;

    /// Return one owned sample from the latest successful snapshot.
    ///
    /// `x` and `y` are zero-based physical raster coordinates with origin at
    /// the top-left. Throws std::out_of_range for coordinates outside the current
    /// physical raster or when no successful snapshot exists yet. The returned
    /// Rgba8 is independent of renderer lifetime and later renders.
    [[nodiscard]] Rgba8 pixel(int x, int y) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
