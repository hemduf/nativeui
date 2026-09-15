#include <nativeui/nativeui.hpp>

#include <string>
#include <vector>

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

    // T141: installed/relocated consumers compile both first-class Binding and
    // legacy State syntax for retained dynamic composition and focus scopes.
    ui::State<bool> visible{true};
    ui::State<int> page{1};
    ui::State<std::vector<int>> items{{1, 2}};
    ui::State<bool> focus_active{true};

    [[maybe_unused]] auto binding_if = ui::If{visible.binding(), ui::Label{"Binding If"}}.spec();
    [[maybe_unused]] auto legacy_if = ui::If{visible, ui::Label{"Legacy If"}}.spec();
    [[maybe_unused]] auto binding_switch = ui::Switch<int>{page.binding()}
        .when(1, ui::Label{"Binding Switch"})
        .otherwise(ui::Label{"Binding fallback"})
        .spec();
    [[maybe_unused]] auto legacy_switch = ui::Switch<int>{page}
        .when(1, ui::Label{"Legacy Switch"})
        .otherwise(ui::Label{"Legacy fallback"})
        .spec();
    [[maybe_unused]] auto binding_each = ui::ForEach<int>{
        items.binding(),
        [](int value) { return value; },
        [](int value) { return ui::Label{std::to_string(value)}; }}.spec();
    [[maybe_unused]] auto legacy_each = ui::ForEach<int>{
        items,
        [](int value) { return value; },
        [](int value) { return ui::Label{std::to_string(value)}; }}.spec();
    [[maybe_unused]] auto binding_focus = ui::FocusScope{
        focus_active.binding(), ui::Label{"Binding focus"}}.spec();
    [[maybe_unused]] auto legacy_focus = ui::FocusScope{
        focus_active, ui::Label{"Legacy focus"}}.spec();

    ui::HeadlessRenderer renderer{{160.0f, 80.0f}};
    if (!renderer.render(ui_tree)) return 1;
    if (renderer.pixel_width() != 160 || renderer.pixel_height() != 80) return 2;
    return renderer.rgba_pixels().empty() ? 3 : 0;
}
