#include <nativeui/nativeui.hpp>

#include <string>
#include <vector>

void nativeui_header_compile_nativeui() {
    ui::Theme theme = ui::default_theme();
    (void)theme;
}

void nativeui_header_compile_t038_visual_state() {
    ui::VisualState state{};
    state.enabled = true;
    state.read_only = true;
    state.hovered = true;
    state.pressed = true;
    state.focused = true;
    state.selected = true;
    state.checked = true;

    const auto interaction = ui::resolve_interaction_state(state);
    (void)interaction;
}

void nativeui_header_compile_t058_conditional(ui::State<bool>& visible) {
    auto spec = ui::make_spec(ui::If{visible, ui::Spacer{1.0f, 1.0f}});
    (void)spec;
}

void nativeui_header_compile_t058_switch(ui::State<int>& selection) {
    auto spec = ui::make_spec(
        ui::Switch<int>{selection}
            .when(1, ui::Spacer{1.0f, 1.0f})
            .when(2, ui::Spacer{2.0f, 2.0f})
            .otherwise(ui::Spacer{3.0f, 3.0f}));
    (void)spec;
}

struct NativeUIHeaderT058Item {
    std::string key;
    float extent{};

    bool operator==(const NativeUIHeaderT058Item&) const = default;
};

void nativeui_header_compile_t058_for_each(
    ui::State<std::vector<NativeUIHeaderT058Item>>& items) {
    auto spec = ui::make_spec(ui::ForEach<NativeUIHeaderT058Item>{
        items,
        [](const NativeUIHeaderT058Item& item) { return item.key; },
        [](const NativeUIHeaderT058Item& item) { return ui::Spacer{item.extent, item.extent}; }});
    (void)spec;
}

void nativeui_header_compile_t058_structural_diagnostic(ui::UI& tree) {
    const auto diagnostic = tree.structural_diagnostic();
    (void)diagnostic;
}
