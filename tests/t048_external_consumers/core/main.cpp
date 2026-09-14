#include <nativeui/nativeui.hpp>

int main() {
    ui::State<bool> enabled{true};
    ui::UI ui_tree{
        ui::Column{
            ui::Header{"T048 core consumer"},
            ui::Toggle{"Enabled", enabled},
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

    ui::HeadlessRenderer renderer{{160.0f, 80.0f}};
    if (!renderer.render(ui_tree)) return 1;
    if (renderer.pixel_width() != 160 || renderer.pixel_height() != 80) return 2;
    return renderer.rgba_pixels().empty() ? 3 : 0;
}
