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
    ui::State<float> drive{0.5f};
    ui::State<bool> bound_enabled{false};
    ui::UI ui_tree{
        ui::Column{
            ui::Header{"T048 core consumer"},
            ui::Knob{"Drive", drive.binding()},
            ui::Toggle{"Enabled", enabled},
            ui::Toggle{"Bound enabled", bound_enabled.binding()},
        }.padding(8.0f).gap(4.0f)};

    // T138: prove the installed umbrella exposes Binding<T> directly and the
    // relocated consumer can use the same source for read/write/observation.
    ui::State<int> binding_source{7};
    auto binding = binding_source.binding();
    if (!binding.valid() || binding.get() != 7) return 4;
    binding.set(8);
    if (binding_source.get() != 8) return 5;
    int binding_observed = 0;
    auto binding_subscription = binding.observe([&](const int& value) {
        binding_observed = value;
    });
    binding_source.set(9);
    if (!binding_subscription.active() || binding_observed != 9 || binding.get() != 9) return 6;

    ui::State<std::string> legacy_text{"legacy"};
    ui::State<std::string> bound_text{"bound"};
    [[maybe_unused]] auto legacy_text_spec = ui::TextInput{"Legacy text", legacy_text}.spec();
    [[maybe_unused]] auto binding_text_spec = ui::TextInput{"Binding text", bound_text.binding()}.spec();

    ui::HeadlessRenderer renderer{{160.0f, 80.0f}};
    if (!renderer.render(ui_tree)) return 1;
    if (renderer.pixel_width() != 160 || renderer.pixel_height() != 80) return 2;
    return renderer.rgba_pixels().empty() ? 3 : 0;
}