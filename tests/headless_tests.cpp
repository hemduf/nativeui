#include "test_support.hpp"

namespace {

void suite() {
    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};

    // Seed the raster surface, then render an otherwise empty retained tree.
    // The renderer owns the same black framebuffer clear as the GPU path;
    // Tree itself must add neither a styled background nor instructional text.
    ui::UI seed{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D& canvas) {
                canvas.fill_rect(
                    ui::Rect{0.0f, 0.0f, 96.0f, 48.0f},
                    ui::Color{0.15f, 0.35f, 0.65f, 1.0f});
            }}
    };
    NUI_CHECK(renderer.render(seed));
    const auto seeded_pixels = renderer.rgba_pixels();

    ui::UI empty{
        ui::Canvas{
            ui::Size{96.0f, 48.0f},
            [](ui::CanvasContext2D&) {}}
    };
    NUI_CHECK(renderer.render(empty));
    NUI_CHECK(renderer.rgba_pixels() != seeded_pixels);

    bool only_renderer_clear = true;
    const auto& empty_pixels = renderer.rgba_pixels();
    for (std::size_t i = 0; i < empty_pixels.size(); i += 4) {
        if (empty_pixels[i] != 0 || empty_pixels[i + 1] != 0 ||
            empty_pixels[i + 2] != 0 || empty_pixels[i + 3] != 255) {
            only_renderer_clear = false;
            break;
        }
    }
    NUI_CHECK(only_renderer_clear);

    ui::State<bool> enabled{true};
    ui::UI tree{
        ui::Padding{4.0f,
            ui::Toggle{"Enabled", enabled}}
    };

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 96);
    NUI_CHECK(renderer.pixel_height() == 48);
    NUI_CHECK(renderer.rgba_pixels().size() == 96U * 48U * 4U);

    // The same logical surface at 2x produces exactly twice the physical
    // dimensions while keeping UI layout coordinates logical.
    renderer.resize({96.0f, 48.0f}, 2.0f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 192);
    NUI_CHECK(renderer.pixel_height() == 96);
    NUI_CHECK(renderer.rgba_pixels().size() == 192U * 96U * 4U);

    // A state-only repaint is consumable headlessly without a layout invalidation.
    enabled.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());
}

} // namespace

int main() { return test::run("headless", &suite); }
