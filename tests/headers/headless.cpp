#include <nativeui/headless.hpp>

void nativeui_header_headless_compile() {
    ui::HeadlessRenderer renderer{{32.0f, 16.0f}};
    (void)renderer.pixel_width();
}
