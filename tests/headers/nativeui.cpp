#include <nativeui/nativeui.hpp>
#include <nativeui/semantics.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

void nativeui_header_compile_nativeui() {
    ui::Theme theme = ui::default_theme();
    (void)theme;

    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Custom;
    ui::SemanticNodeSnapshot snapshot;
    snapshot.info = std::move(info);
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

void nativeui_header_compile_t062_tooltip() {
    // T062's public contract is text-first: help text, then the decorated child.
    // Delay customization stays fluent so the v1 surface has one construction
    // shape rather than an extra positional-duration overload.
    auto ticket_shape = ui::make_spec(
        ui::Tooltip{"Reset to default", ui::Spacer{24.0f, 12.0f}}
            .delay(std::chrono::milliseconds{500}));
    auto default_delay = ui::make_spec(
        ui::Tooltip{"Reset to default", ui::Spacer{24.0f, 12.0f}});
    ui::Tooltip lvalue{"Lvalue delay", ui::Spacer{24.0f, 12.0f}};
    lvalue.delay(std::chrono::milliseconds{100});
    (void)ticket_shape;
    (void)default_delay;
    (void)lvalue;
}

void nativeui_header_compile_t063_dialog(ui::UI& tree) {
    ui::Dialog dialog{tree};

    ui::DialogSpec spec;
    spec.title = "Confirm";
    spec.body = ui::make_spec(ui::Spacer{120.0f, 80.0f});
    spec.actions.push_back(ui::DialogAction{
        "confirm", "Confirm", true, ui::DialogActionRole::Default});
    spec.actions.push_back(ui::DialogAction{
        "cancel", "Cancel", true, ui::DialogActionRole::Cancel});

    const ui::DialogShowResult shown = dialog.show(
        std::move(spec), [](ui::DialogResult result) { (void)result; });
    (void)shown;
    (void)dialog.active();
    (void)dialog.close();
}
