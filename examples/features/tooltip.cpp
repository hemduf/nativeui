#include "example_support.hpp"
#include <nativeui/tooltip.hpp>

namespace {
ui::TooltipStyle tooltip_style() {
    ui::TooltipStyle style; style.max_width = 220.0f; style.padding = 8.0f;
    style.background = ui::Color{0.12f, 0.18f, 0.28f, 1.0f};
    return style;
}
int self_test() {
    ui::UI tree{ui::Tooltip{"Affiche une aide complémentaire", ui::Button{"Aide", [] {}}}
        .style(tooltip_style())};
    tree.resize({300.0f, 100.0f});
    const auto info = tree.component_semantics(3);
    ui::HeadlessRenderer renderer{{300.0f, 100.0f}, 1.0f};
    return info && info->description == "Affiche une aide complémentaire" && renderer.render(tree)
        ? 0 : example::fail("tooltip did not describe its anchor");
}
}
int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    ui::UI tree{ui::Padding{20.0f, ui::Tooltip{
        "L’aide apparaît après le délai commun au survol et au focus.",
        ui::Button{"Aide", [] {}}}.style(tooltip_style())}};
    return example::run_window(tree, "NativeUI Tooltip", {380.0f, 180.0f});
}
