#include <nativeui/widgets.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

void nativeui_header_compile_widgets() {
    ui::State<int> selected{2};
    std::vector<ui::ComboBoxOption<int>> options{
        {1, "One", true},
        {2, "Two", false},
    };

    auto combo = ui::ComboBox<int>{selected, options}.placeholder("Choose");
    [[maybe_unused]] auto combo_spec = std::move(combo).spec();

    auto dynamic_combo = ui::ComboBox<int>{
        selected,
        [&options] { return options; }}.placeholder("Dynamic");
    [[maybe_unused]] auto dynamic_combo_spec = std::move(dynamic_combo).spec();

    int actions = 0;
    std::vector<ui::PopupMenuItem> items{
        ui::PopupMenuItem::action("Run", [&actions] { ++actions; }),
        ui::PopupMenuItem::separator(),
        ui::PopupMenuItem::action("Disabled", [] {}, false),
    };
    auto menu = ui::PopupMenu{"Actions", items};
    [[maybe_unused]] auto menu_spec = std::move(menu).spec();

    auto dynamic_menu = ui::PopupMenu{
        "Dynamic actions",
        [&items] { return items; }};
    [[maybe_unused]] auto dynamic_menu_spec = std::move(dynamic_menu).spec();

    ui::State<float> knob_state{0.5f};
    ui::State<bool> toggle_state{false};
    ui::State<std::string> text_state{"hello"};
    auto knob_binding = knob_state.binding();
    auto toggle_binding = toggle_state.binding();
    auto text_binding = text_state.binding();

    auto binding_knob = ui::Knob{"Binding knob", knob_binding};
    [[maybe_unused]] auto binding_knob_spec = std::move(binding_knob).spec();
    auto legacy_knob = ui::Knob{"Legacy knob", knob_state};
    [[maybe_unused]] auto legacy_knob_spec = std::move(legacy_knob).spec();

    auto binding_toggle = ui::Toggle{"Binding toggle", toggle_binding};
    [[maybe_unused]] auto binding_toggle_spec = std::move(binding_toggle).spec();
    auto legacy_toggle = ui::Toggle{"Legacy toggle", toggle_state};
    [[maybe_unused]] auto legacy_toggle_spec = std::move(legacy_toggle).spec();

    auto binding_text = ui::TextInput{"Binding text", text_binding};
    [[maybe_unused]] auto binding_text_spec = std::move(binding_text).spec();
    auto legacy_text = ui::TextInput{"Legacy text", text_state};
    [[maybe_unused]] auto legacy_text_spec = std::move(legacy_text).spec();
}