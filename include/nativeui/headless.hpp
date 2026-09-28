#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ui {

/// One RGBA8888 pixel sample returned by HeadlessRenderer::pixel().
struct Rgba8 {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

/// Deterministic offscreen renderer backed by a Skia raster surface. This has
/// no Pugl/OpenGL/display-server dependency and uses the same logical-coordinate
/// convention as the windowed renderer.
class HeadlessRenderer {
public:
    /// Create an offscreen raster target.
    ///
    /// Both logical extents and scale_factor must be finite and > 0; invalid
    /// input throws std::invalid_argument. Physical dimensions are rounded from
    /// logical_size * scale_factor and clamped to at least one pixel.
    explicit HeadlessRenderer(Size logical_size, float scale_factor = 1.0f);
    ~HeadlessRenderer();

    HeadlessRenderer(const HeadlessRenderer&) = delete;
    HeadlessRenderer& operator=(const HeadlessRenderer&) = delete;
    HeadlessRenderer(HeadlessRenderer&&) noexcept;
    HeadlessRenderer& operator=(HeadlessRenderer&&) noexcept;

    /// Replace logical size/scale and discard the existing raster surface and
    /// cached pixel buffer. Validation matches the constructor.
    void resize(Size logical_size, float scale_factor = 1.0f);

    /// Resize the supplied UI to logical_size(), paint it offscreen and copy a
    /// tightly packed RGBA8888 snapshot into rgba_pixels().
    ///
    /// Returns false when the raster surface/canvas/pixel view cannot be created.
    /// Component/layout/paint exceptions are not converted to false.
    [[nodiscard]] bool render(UI& ui);

    /// Current NativeUI logical viewport size.
    [[nodiscard]] Size logical_size() const noexcept;
    /// Logical-to-physical raster scale.
    [[nodiscard]] float scale_factor() const noexcept;
    /// Rounded physical raster width in pixels.
    [[nodiscard]] int pixel_width() const noexcept;
    /// Rounded physical raster height in pixels.
    [[nodiscard]] int pixel_height() const noexcept;

    /// Borrow the most recent tightly packed RGBA8888 snapshot, top-to-bottom
    /// and left-to-right. Rendering/resizing/moving/destroying this renderer may
    /// invalidate the returned reference.
    [[nodiscard]] const std::vector<std::uint8_t>& rgba_pixels() const noexcept;

    /// Read one physical pixel from the most recent snapshot.
    /// Throws std::out_of_range when x/y are outside the raster bounds.
    [[nodiscard]] Rgba8 pixel(int x, int y) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
