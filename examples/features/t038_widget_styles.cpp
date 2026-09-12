#include "example_support.hpp"

namespace {

[[nodiscard]] bool same_color(ui::Color a, ui::Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct DemoState {
    ui::State<float> value{0.62f};
};

ui::ButtonStyle demo_button_style() {
    ui::ButtonStyle style;
    style.base.fill = ui::Color{0.10f, 0.14f, 0.20f, 1.0f};
    style.hovered.fill = ui::Color{0.15f, 0.22f, 0.32f, 1.0f};
    style.pressed.fill = ui::Color{0.24f, 0.58f, 0.86f, 1.0f};
    style.focused.border = ui::Color{0.62f, 0.82f, 1.0f, 1.0f};
    return style;
}

ui::SliderStyle demo_slider_style() {
    ui::SliderStyle style;
    style.base.active = ui::Color{0.24f, 0.58f, 0.86f, 1.0f};
    style.hovered.active = ui::Color{0.38f, 0.72f, 0.98f, 1.0f};
    return style;
}

ui::UI make_ui(DemoState& state) {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Typed Widget Styles"},
        ui::Label{
            "Each widget resolves the same typed visual-state precedence while keeping style data instance-local."
        }.size(12.0f),
        ui::Button{"Hover / press / focus", [] {}}.style(demo_button_style()),
        ui::Slider{state.value}.range(0.0f, 1.0f).style(demo_slider_style())
    }.gap(14.0f)};
}

int self_test() {
    const auto theme = ui::default_theme();

    ui::ScrollbarStyle style;
    style.base.track = ui::Color{0.10f, 0.12f, 0.16f, 1.0f};
    style.base.thumb = ui::Color{0.42f, 0.58f, 0.78f, 1.0f};
    style.base.thickness = 9.0f;
    style.base.minimum_thumb = 21.0f;
    style.base.corner_radius = 4.5f;
    style.hovered.thumb = ui::Color{0.52f, 0.70f, 0.94f, 1.0f};
    style.pressed.thumb = ui::Color{0.78f, 0.88f, 1.0f, 1.0f};
    style.disabled.thumb = ui::Color{0.28f, 0.30f, 0.34f, 1.0f};

    const auto inherited = ui::default_scrollbar_style(theme);
    const auto hovered = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = true, .hovered = true});
    const auto pressed = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = true, .hovered = true, .pressed = true});
    const auto disabled = ui::resolve_scrollbar_style(
        inherited,
        style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true});

    if (!same_color(hovered.thumb, *style.hovered.thumb)) {
        return example::fail("hovered ScrollbarStyle did not resolve the hover patch");
    }
    if (!same_color(pressed.thumb, *style.pressed.thumb)) {
        return example::fail("pressed ScrollbarStyle did not win over hover");
    }
    if (!same_color(disabled.thumb, *style.disabled.thumb)) {
        return example::fail("disabled ScrollbarStyle did not win over pressed/hover");
    }
    if (hovered.thickness != 9.0f || pressed.thickness != 9.0f ||
        disabled.thickness != 9.0f || hovered.minimum_thumb != 21.0f ||
        hovered.corner_radius != 4.5f) {
        return example::fail("paint-only interaction variants changed scrollbar geometry");
    }

    ui::ComboBoxStyle combo_style;
    combo_style.base.fill = ui::Color{0.09f, 0.11f, 0.15f, 1.0f};
    combo_style.base.control_height = 34.0f;
    combo_style.base.minimum_width = 132.0f;
    combo_style.hovered.border = ui::Color{0.28f, 0.52f, 0.76f, 1.0f};
    combo_style.pressed.border = ui::Color{0.50f, 0.72f, 0.94f, 1.0f};
    combo_style.disabled.text = ui::Color{0.33f, 0.35f, 0.39f, 1.0f};
    combo_style.focused.border = ui::Color{0.70f, 0.86f, 1.0f, 1.0f};

    const auto combo_hovered = ui::resolve_combo_box_style(
        ui::default_combo_box_style(theme),
        combo_style,
        ui::VisualState{.enabled = true, .hovered = true, .focused = true});
    const auto combo_disabled = ui::resolve_combo_box_style(
        ui::default_combo_box_style(theme),
        combo_style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true});
    if (!same_color(combo_hovered.border, *combo_style.focused.border) ||
        !same_color(combo_disabled.text, *combo_style.disabled.text)) {
        return example::fail("ComboBoxStyle state resolution precedence is incorrect");
    }
    if (combo_hovered.control_height != 34.0f || combo_disabled.control_height != 34.0f ||
        combo_hovered.minimum_width != 132.0f || combo_disabled.minimum_width != 132.0f) {
        return example::fail("ComboBoxStyle paint states changed geometry");
    }

    ui::MenuItemStyle item_style;
    item_style.base.fill = ui::Color{0.09f, 0.11f, 0.15f, 1.0f};
    item_style.base.text = ui::Color{0.88f, 0.90f, 0.94f, 1.0f};
    item_style.base.row_height = 34.0f;
    item_style.selected.fill = ui::Color{0.18f, 0.38f, 0.60f, 1.0f};
    item_style.hovered.fill = ui::Color{0.14f, 0.24f, 0.36f, 1.0f};
    item_style.pressed.fill = ui::Color{0.22f, 0.44f, 0.66f, 1.0f};
    item_style.disabled.text = ui::Color{0.33f, 0.35f, 0.39f, 1.0f};

    const auto item_selected = ui::resolve_menu_item_style(
        ui::default_menu_item_style(theme),
        item_style,
        ui::VisualState{.enabled = true, .hovered = true, .selected = true});
    const auto item_disabled = ui::resolve_menu_item_style(
        ui::default_menu_item_style(theme),
        item_style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true, .selected = true});
    if (!same_color(item_selected.fill, *item_style.hovered.fill) ||
        !same_color(item_disabled.text, *item_style.disabled.text)) {
        return example::fail("MenuItemStyle state resolution precedence is incorrect");
    }
    if (item_selected.row_height != 34.0f || item_disabled.row_height != 34.0f) {
        return example::fail("MenuItemStyle paint states changed geometry");
    }

    // T038 consumer contract: the T035 anchor and popup-row families must
    // consume the typed recipes, not merely expose standalone resolver helpers.
    ui::State<int> combo_selection{1};
    ui::ComboBoxStyle consumer_anchor_style;
    consumer_anchor_style.base.minimum_width = 181.0f;
    consumer_anchor_style.base.control_height = 47.0f;
    consumer_anchor_style.base.fill = ui::Color{0.03f, 0.07f, 0.12f, 1.0f};
    ui::MenuItemStyle consumer_item_style;
    consumer_item_style.base.row_height = 41.0f;
    consumer_item_style.selected.fill = ui::Color{0.82f, 0.16f, 0.22f, 1.0f};

    ui::UI styled_combo{ui::ComboBox<int>{
        combo_selection,
        {{1, "One", true}, {2, "Two", true}}}
        .style(consumer_anchor_style)
        .item_style(consumer_item_style)};
    const auto styled_combo_metrics = styled_combo.measure();
    if (styled_combo_metrics.preferred.w != 181.0f ||
        styled_combo_metrics.preferred.h != 47.0f) {
        return example::fail("ComboBox did not consume typed anchor geometry");
    }

    ui::UI styled_menu{ui::PopupMenu{
        "Menu",
        {ui::PopupMenuItem::action("Action", [] {})}}
        .style(consumer_anchor_style)
        .item_style(consumer_item_style)};
    const auto styled_menu_metrics = styled_menu.measure();
    if (styled_menu_metrics.preferred.w != 181.0f ||
        styled_menu_metrics.preferred.h != 47.0f) {
        return example::fail("PopupMenu did not consume typed anchor geometry");
    }

    example::Platform styled_platform;
    styled_combo.resize({240.0f, 180.0f});
    styled_combo.activate(styled_platform);
    ui::HeadlessRenderer styled_renderer{{240.0f, 180.0f}, 1.0f};
    if (!ui::handled(styled_combo.dispatch(example::key(ui::Key::Down), styled_platform)) ||
        !styled_renderer.render(styled_combo)) {
        return example::fail("styled ComboBox popup render failed");
    }
    const auto styled_open = styled_renderer.rgba_pixels();

    ui::State<int> default_item_selection{1};
    ui::UI default_item_combo{ui::ComboBox<int>{
        default_item_selection,
        {{1, "One", true}, {2, "Two", true}}}
        .style(consumer_anchor_style)};
    example::Platform default_item_platform;
    default_item_combo.resize({240.0f, 180.0f});
    default_item_combo.activate(default_item_platform);
    ui::HeadlessRenderer default_item_renderer{{240.0f, 180.0f}, 1.0f};
    if (!ui::handled(default_item_combo.dispatch(example::key(ui::Key::Down), default_item_platform)) ||
        !default_item_renderer.render(default_item_combo)) {
        return example::fail("default MenuItem popup render failed");
    }
    if (styled_open == default_item_renderer.rgba_pixels()) {
        return example::fail("MenuItemStyle did not affect retained ComboBox popup rendering");
    }

    ui::ListViewStyle list_style;
    list_style.base.row_fill = ui::Color{0.08f, 0.10f, 0.14f, 1.0f};
    list_style.base.row_horizontal_inset = 5.0f;
    list_style.hovered.row_fill = ui::Color{0.15f, 0.22f, 0.32f, 1.0f};
    list_style.selected.row_fill = ui::Color{0.20f, 0.42f, 0.66f, 1.0f};
    list_style.disabled.row_fill = ui::Color{0.12f, 0.13f, 0.15f, 1.0f};

    const auto list_selected = ui::resolve_list_view_style(
        ui::default_list_view_style(theme),
        list_style,
        ui::VisualState{.enabled = true, .selected = true});
    const auto list_disabled = ui::resolve_list_view_style(
        ui::default_list_view_style(theme),
        list_style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true, .selected = true});
    if (!same_color(list_selected.row_fill, *list_style.selected.row_fill) ||
        !same_color(list_disabled.row_fill, *list_style.disabled.row_fill)) {
        return example::fail("ListViewStyle selected/disabled resolution is incorrect");
    }
    if (list_selected.row_horizontal_inset != 5.0f ||
        list_disabled.row_horizontal_inset != 5.0f) {
        return example::fail("ListViewStyle paint states changed row geometry");
    }

    ui::TabsStyle tabs_style;
    tabs_style.base.header_height = 42.0f;
    tabs_style.base.text = ui::Color{0.72f, 0.76f, 0.82f, 1.0f};
    tabs_style.selected.tab_fill = ui::Color{0.14f, 0.20f, 0.28f, 1.0f};
    tabs_style.hovered.tab_fill = ui::Color{0.18f, 0.28f, 0.40f, 1.0f};
    tabs_style.disabled.text = ui::Color{0.33f, 0.35f, 0.39f, 1.0f};
    tabs_style.focused.header_border = ui::Color{0.70f, 0.86f, 1.0f, 1.0f};

    const auto tabs_hovered = ui::resolve_tabs_style(
        ui::default_tabs_style(theme),
        tabs_style,
        ui::VisualState{.enabled = true, .hovered = true, .focused = true});
    const auto tabs_disabled = ui::resolve_tabs_style(
        ui::default_tabs_style(theme),
        tabs_style,
        ui::VisualState{.enabled = false, .hovered = true, .pressed = true, .selected = true});
    if (!same_color(tabs_hovered.tab_fill, *tabs_style.hovered.tab_fill) ||
        !same_color(tabs_hovered.header_border, *tabs_style.focused.header_border) ||
        !same_color(tabs_disabled.text, *tabs_style.disabled.text)) {
        return example::fail("TabsStyle interaction/orthogonal resolution is incorrect");
    }
    if (tabs_hovered.header_height != 42.0f || tabs_disabled.header_height != 42.0f) {
        return example::fail("TabsStyle paint states changed header geometry");
    }

    DemoState state;
    auto tree = make_ui(state);
    tree.resize({640.0f, 320.0f});
    ui::HeadlessRenderer renderer{{640.0f, 320.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("typed style demo render failed");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T038 Widget Styles", {640.0f, 320.0f});
}
