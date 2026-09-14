#include <nativeui/nativeui.hpp>

#include <vector>

#if __has_include("include/core/SkCanvas.h")
#error "NativeUI::Core must not expose the packaged Skia include root to consumers"
#endif

namespace {

class CustomComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {12.0f, 12.0f};
    }

    void paint(ui::PaintContext& context) const override {
        context.painter().fill_rounded_rect(
            context.bounds(), 2.0f, ui::Color{0.2f, 0.4f, 0.8f, 1.0f});
    }
};

static_assert(std::is_base_of_v<ui::Component, CustomComponent>);

} // namespace

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