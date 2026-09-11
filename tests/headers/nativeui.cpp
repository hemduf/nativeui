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

void nativeui_header_compile_t038_button_style(const ui::Theme& theme) {
    ui::ButtonStyle style{};
    style.base.fill = ui::Color{0.1f, 0.2f, 0.3f, 1.0f};
    style.hovered.fill = ui::Color{0.2f, 0.3f, 0.4f, 1.0f};
    style.pressed.border_width = 3.0f;
    style.disabled.text = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    style.focused.border = ui::Color{0.9f, 0.8f, 0.1f, 1.0f};
    style.read_only.text_size = 12.0f;

    auto inherited = ui::default_button_style(theme);
    const auto resolved = ui::resolve_button_style(
        inherited,
        style,
        ui::VisualState{.enabled = true, .hovered = true, .focused = true});
    (void)resolved;
}

void nativeui_header_compile_t038_choice_styles(const ui::Theme& theme) {
    ui::CheckboxStyle checkbox{};
    checkbox.base.box_fill = ui::Color{0.1f, 0.2f, 0.3f, 1.0f};
    checkbox.checked.box_fill = ui::Color{0.2f, 0.5f, 0.8f, 1.0f};
    checkbox.hovered.box_border = ui::Color{0.7f, 0.4f, 0.2f, 1.0f};
    checkbox.pressed.box_border = ui::Color{0.9f, 0.3f, 0.2f, 1.0f};
    checkbox.disabled.text = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    checkbox.focused.box_border_width = 2.0f;
    checkbox.read_only.checkmark = ui::Color{0.6f, 0.6f, 0.6f, 1.0f};
    const auto resolved_checkbox = ui::resolve_checkbox_style(
        ui::default_checkbox_style(theme),
        checkbox,
        ui::VisualState{.enabled = true, .focused = true, .checked = true});
    (void)resolved_checkbox;

    ui::RadioStyle radio{};
    radio.base.outer_fill = ui::Color{0.2f, 0.2f, 0.2f, 1.0f};
    radio.selected.mark_fill = ui::Color{0.2f, 0.5f, 0.8f, 1.0f};
    radio.hovered.outer_fill = ui::Color{0.7f, 0.4f, 0.2f, 1.0f};
    radio.pressed.outer_fill = ui::Color{0.9f, 0.3f, 0.2f, 1.0f};
    radio.disabled.text = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    radio.focused.outer_radius = 10.0f;
    radio.read_only.mark_fill = ui::Color{0.6f, 0.6f, 0.6f, 1.0f};
    const auto resolved_radio = ui::resolve_radio_style(
        ui::default_radio_style(theme),
        radio,
        ui::VisualState{.enabled = true, .focused = true, .selected = true});
    (void)resolved_radio;
}

void nativeui_header_compile_t038_slider_style(const ui::Theme& theme) {
    ui::SliderStyle style{};
    style.base.track = ui::Color{0.1f, 0.1f, 0.1f, 1.0f};
    style.base.active = ui::Color{0.2f, 0.5f, 0.8f, 1.0f};
    style.base.thumb = ui::Color{0.8f, 0.8f, 0.8f, 1.0f};
    style.base.track_thickness = 5.0f;
    style.base.thumb_diameter = 16.0f;
    style.hovered.active = ui::Color{0.3f, 0.6f, 0.9f, 1.0f};
    style.pressed.thumb = ui::Color{1.0f, 1.0f, 1.0f, 1.0f};
    style.disabled.active = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    style.read_only.active = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    style.focused.focus_ring = ui::Color{0.9f, 0.7f, 0.1f, 1.0f};

    const auto resolved = ui::resolve_slider_style(
        ui::default_slider_style(theme),
        style,
        ui::VisualState{.enabled = true, .hovered = true, .focused = true});
    (void)resolved;
}

void nativeui_header_compile_t038_progress_styles(const ui::Theme& theme) {
    ui::ProgressBarStyle progress{};
    progress.base.track = ui::Color{0.1f, 0.1f, 0.1f, 1.0f};
    progress.base.fill = ui::Color{0.2f, 0.6f, 0.8f, 1.0f};
    progress.base.border = ui::Color{0.7f, 0.7f, 0.7f, 1.0f};
    progress.base.text = ui::Color{0.9f, 0.9f, 0.9f, 1.0f};
    progress.base.border_width = 2.0f;
    progress.base.corner_radius = 5.0f;
    progress.base.fill_corner_radius = 4.0f;
    progress.base.horizontal_size = ui::Size{220.0f, 28.0f};
    progress.disabled.fill = ui::Color{0.3f, 0.3f, 0.3f, 1.0f};
    const auto resolved_progress = ui::resolve_progress_bar_style(
        ui::default_progress_bar_style(theme),
        progress,
        ui::VisualState{.enabled = false});
    (void)resolved_progress;

    ui::MeterStyle meter{};
    meter.base.track = ui::Color{0.05f, 0.05f, 0.05f, 1.0f};
    meter.base.fill = ui::Color{0.2f, 0.8f, 0.3f, 1.0f};
    meter.base.vertical_size = ui::Size{30.0f, 180.0f};
    meter.read_only.fill = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    const auto resolved_meter = ui::resolve_meter_style(
        ui::default_meter_style(theme),
        meter,
        ui::VisualState{.enabled = true, .read_only = true});
    (void)resolved_meter;
}

void nativeui_header_compile_t038_progress_widget_styles(ui::State<float>& value) {
    ui::ProgressBarStyle progress{};
    auto progress_spec = ui::make_spec(ui::ProgressBar{value}.style(progress));
    (void)progress_spec;

    ui::MeterStyle meter{};
    auto meter_spec = ui::make_spec(ui::Meter{value}.style(meter));
    (void)meter_spec;
}

void nativeui_header_compile_t038_toggle_style(const ui::Theme& theme,
                                                ui::State<bool>& value) {
    ui::ToggleStyle style{};
    style.base.fill = ui::Color{0.1f, 0.1f, 0.1f, 1.0f};
    style.base.border = ui::Color{0.3f, 0.3f, 0.3f, 1.0f};
    style.base.text = ui::Color{0.9f, 0.9f, 0.9f, 1.0f};
    style.base.track = ui::Color{0.2f, 0.2f, 0.2f, 1.0f};
    style.base.thumb = ui::Color{0.8f, 0.8f, 0.8f, 1.0f};
    style.checked.track = ui::Color{0.2f, 0.6f, 0.8f, 1.0f};
    style.hovered.border = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    style.pressed.thumb = ui::Color{1.0f, 1.0f, 1.0f, 1.0f};
    style.disabled.text = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    style.read_only.track = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    style.focused.border_width = 2.0f;

    const auto resolved = ui::resolve_toggle_style(
        ui::default_toggle_style(theme),
        style,
        ui::VisualState{.enabled = true, .focused = true, .checked = true});
    (void)resolved;

    auto spec = ui::make_spec(ui::Toggle{"Bypass", value}.style(style));
    (void)spec;
}

void nativeui_header_compile_t038_text_input_style(const ui::Theme& theme,
                                                    ui::State<std::string>& value) {
    ui::TextInputStyle style{};
    style.base.field_fill = ui::Color{0.1f, 0.1f, 0.1f, 1.0f};
    style.base.border = ui::Color{0.3f, 0.3f, 0.3f, 1.0f};
    style.base.text = ui::Color{0.9f, 0.9f, 0.9f, 1.0f};
    style.base.placeholder = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    style.base.selection = ui::Color{0.2f, 0.5f, 0.8f, 0.4f};
    style.base.caret = ui::Color{0.9f, 0.8f, 0.6f, 1.0f};
    style.hovered.border = ui::Color{0.5f, 0.5f, 0.5f, 1.0f};
    style.pressed.border = ui::Color{0.7f, 0.7f, 0.7f, 1.0f};
    style.disabled.text = ui::Color{0.4f, 0.4f, 0.4f, 1.0f};
    style.read_only.text = ui::Color{0.6f, 0.6f, 0.6f, 1.0f};
    style.focused.border_width = 2.0f;
    const auto resolved = ui::resolve_text_input_style(
        ui::default_text_input_style(theme),
        style,
        ui::VisualState{.enabled = true, .focused = true});
    (void)resolved;
    auto spec = ui::make_spec(ui::TextInput{"Name", value}.style(style));
    (void)spec;
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
