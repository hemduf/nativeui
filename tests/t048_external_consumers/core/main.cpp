#include <nativeui/nativeui.hpp>

int main() {
    ui::State<bool> enabled{true};
    ui::UI ui_tree{
        ui::Column{
            ui::Header{"T048 core consumer"},
            ui::Toggle{"Enabled", enabled},
        }.padding(8.0f).gap(4.0f)};

    ui::HeadlessRenderer renderer{{160.0f, 80.0f}};
    if (!renderer.render(ui_tree)) return 1;
    if (renderer.pixel_width() != 160 || renderer.pixel_height() != 80) return 2;
    return renderer.rgba_pixels().empty() ? 3 : 0;
}
