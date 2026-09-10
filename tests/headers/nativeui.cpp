#include <nativeui/nativeui.hpp>
#include <nativeui/semantics.hpp>

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
