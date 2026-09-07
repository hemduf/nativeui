#include "test_support.hpp"

namespace {

void suite() {
    ui::State<bool> enabled{true};
    ui::UI tree{
        ui::Padding{4.0f,
            ui::Toggle{"Enabled", enabled}}
    };

    ui::HeadlessRenderer renderer{{96.0f, 48.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 96);
    NUI_CHECK(renderer.pixel_height() == 48);
    NUI_CHECK(renderer.rgba_pixels().size() == 96U * 48U * 4U);

    // NativeUI paints an opaque background, so a successful raster frame must
    // contain non-zero alpha without needing a display server or GL context.
    const auto background = renderer.pixel(0, 0);
    NUI_CHECK(background.a != 0);

    // The same logical surface at 2x produces exactly twice the physical
    // dimensions while keeping UI layout coordinates logical.
    renderer.resize({96.0f, 48.0f}, 2.0f);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel_width() == 192);
    NUI_CHECK(renderer.pixel_height() == 96);
    NUI_CHECK(renderer.rgba_pixels().size() == 192U * 96U * 4U);
    NUI_CHECK(renderer.pixel(0, 0).a != 0);

    // A state-only repaint is consumable headlessly without a layout invalidation.
    enabled.set(false);
    NUI_CHECK(tree.paint_dirty());
    NUI_CHECK(!tree.layout_dirty());
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(!tree.dirty());
}

} // namespace

int main() { return test::run("headless", &suite); }
