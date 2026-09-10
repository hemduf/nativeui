#include <nativeui/nativeui.hpp>

#include <string>
#include <vector>

void nativeui_header_compile_nativeui() {}

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

void nativeui_header_compile_t061_overlay(ui::UI& tree) {
    ui::OverlaySpec overlay;
    overlay.mode = ui::OverlayMode::NonModal;
    overlay.pointer_policy = ui::OverlayPointerPolicy::Normal;
    overlay.placement = ui::OverlayPlacement::Auto;
    overlay.dismiss_on_escape = true;
    overlay.dismiss_on_outside_pointer_down = true;
    overlay.content = ui::make_spec(ui::Spacer{24.0f, 12.0f});

    const ui::OverlayHandle handle = tree.show_overlay(std::move(overlay));
    (void)tree.close_overlay(handle);
}
