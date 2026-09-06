#pragma once

#include <nativeui/geometry.hpp>
#include <nativeui/ui.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace ui {

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
    explicit HeadlessRenderer(Size logical_size, float scale_factor = 1.0f);
    ~HeadlessRenderer();

    HeadlessRenderer(const HeadlessRenderer&) = delete;
    HeadlessRenderer& operator=(const HeadlessRenderer&) = delete;
    HeadlessRenderer(HeadlessRenderer&&) noexcept;
    HeadlessRenderer& operator=(HeadlessRenderer&&) noexcept;

    void resize(Size logical_size, float scale_factor = 1.0f);
    [[nodiscard]] bool render(UI& ui);

    [[nodiscard]] Size logical_size() const noexcept;
    [[nodiscard]] float scale_factor() const noexcept;
    [[nodiscard]] int pixel_width() const noexcept;
    [[nodiscard]] int pixel_height() const noexcept;

    /// Tightly packed RGBA8888 pixels, top-to-bottom, left-to-right.
    [[nodiscard]] const std::vector<std::uint8_t>& rgba_pixels() const noexcept;
    [[nodiscard]] Rgba8 pixel(int x, int y) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui
