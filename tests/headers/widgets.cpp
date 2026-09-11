#include <nativeui/widgets.hpp>

#include <functional>
#include <string>
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
}
