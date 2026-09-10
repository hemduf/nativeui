#pragma once

#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <optional>
#include <stdexcept>
#include <utility>

namespace t036_contract {

inline void public_builder_contract() {
    ui::State<std::optional<int>> list_selection{std::nullopt};
    ui::State<int> tab_selection{1};

    auto list = ui::ListView<int>{list_selection}
                    .item(1, ui::Label{"One"})
                    .item(2, ui::Label{"Two"}, false);
    auto tabs = ui::Tabs<int>{tab_selection}
                    .tab(1, "One", ui::Label{"Panel one"})
                    .tab(2, "Two", ui::Label{"Panel two"}, false);

    [[maybe_unused]] auto list_spec = std::move(list).spec();
    [[maybe_unused]] auto tabs_spec = std::move(tabs).spec();
}

inline void duplicate_key_contract() {
    ui::State<std::optional<int>> list_selection{std::nullopt};
    bool list_rejected = false;
    try {
        [[maybe_unused]] auto duplicate = ui::ListView<int>{list_selection}
            .item(7, ui::Label{"First"})
            .item(7, ui::Label{"Duplicate"});
    } catch (const std::invalid_argument&) {
        list_rejected = true;
    }
    NUI_CHECK(list_rejected);

    ui::State<int> tab_selection{1};
    bool tabs_rejected = false;
    try {
        [[maybe_unused]] auto duplicate = ui::Tabs<int>{tab_selection}
            .tab(3, "First", ui::Label{"Panel one"})
            .tab(3, "Duplicate", ui::Label{"Panel two"});
    } catch (const std::invalid_argument&) {
        tabs_rejected = true;
    }
    NUI_CHECK(tabs_rejected);
}

inline void list_keyboard_pointer_and_read_only_contract() {
    test::MockPlatform platform;

    ui::State<std::optional<int>> selected{1};
    ui::UI list{ui::ListView<int>{selected}
        .item(1, ui::Spacer{100.0f, 30.0f})
        .item(2, ui::Spacer{100.0f, 30.0f}, false)
        .item(3, ui::Spacer{100.0f, 30.0f})
        .item(4, ui::Spacer{100.0f, 30.0f})
        .item(5, ui::Spacer{100.0f, 30.0f})};
    list.resize({100.0f, 60.0f});
    list.activate(platform);

    NUI_CHECK(list.dispatch(test::key(ui::Key::Down), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() && *selected.get() == 3);
    NUI_CHECK(list.dispatch(test::key(ui::Key::Home), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() && *selected.get() == 1);
    NUI_CHECK(list.dispatch(test::key(ui::Key::End), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() && *selected.get() == 5);

    // End must reveal the final row through T034. With five 30px rows in a
    // 60px viewport the resulting offset is 90px, so y=10 maps to row 4.
    NUI_CHECK(list.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 10.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(list.dispatch(
                  test::pointer(ui::InputType::PointerUp, 20.0f, 10.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(selected.get() && *selected.get() == 4);

    ui::State<bool> read_only{true};
    ui::State<std::optional<int>> read_only_selected{1};
    ui::UI read_only_list{ui::ReadOnly{
        read_only,
        ui::ListView<int>{read_only_selected}
            .item(1, ui::Spacer{80.0f, 30.0f})
            .item(2, ui::Spacer{80.0f, 30.0f})}};
    read_only_list.resize({80.0f, 60.0f});
    read_only_list.activate(platform);
    NUI_CHECK(read_only_list.dispatch(test::key(ui::Key::Down), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(read_only_selected.get() && *read_only_selected.get() == 2);
}

inline void tabs_keyboard_pointer_focus_and_read_only_contract() {
    test::MockPlatform platform;

    ui::State<int> selected{1};
    auto first_panel = std::make_shared<test::ProbeState>();
    auto third_panel = std::make_shared<test::ProbeState>();
    ui::UI tabs{ui::Tabs<int>{selected}
        .tab(1, "One", test::Probe{first_panel})
        .tab(2, "Two", ui::Spacer{120.0f, 40.0f}, false)
        .tab(3, "Three", test::Probe{third_panel})};
    tabs.resize({300.0f, 120.0f});
    tabs.activate(platform);

    NUI_CHECK(tabs.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 3);
    NUI_CHECK(tabs.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 1);
    NUI_CHECK(tabs.dispatch(test::key(ui::Key::End), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 3);
    NUI_CHECK(tabs.dispatch(test::key(ui::Key::Home), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 1);

    NUI_CHECK(tabs.dispatch(
                  test::pointer(ui::InputType::PointerDown, 250.0f, 18.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tabs.dispatch(
                  test::pointer(ui::InputType::PointerUp, 250.0f, 18.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 3);

    // Move focus into panel 3, then switch externally. The now-collapsed panel
    // must lose focus and the composite Tabs stop must become the safe fallback.
    tabs.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(third_panel->focus_in == 1);
    selected.set(1);
    NUI_CHECK(third_panel->focus_out == 1);
    NUI_CHECK(tabs.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 3);

    ui::State<bool> read_only{true};
    ui::State<int> read_only_selected{1};
    ui::UI read_only_tabs{ui::ReadOnly{
        read_only,
        ui::Tabs<int>{read_only_selected}
            .tab(1, "One", ui::Spacer{100.0f, 40.0f})
            .tab(2, "Two", ui::Spacer{100.0f, 40.0f})}};
    read_only_tabs.resize({200.0f, 100.0f});
    read_only_tabs.activate(platform);
    NUI_CHECK(read_only_tabs.dispatch(test::key(ui::Key::Right), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(read_only_selected.get() == 2);
}

inline void missing_selection_and_two_instance_contract() {
    test::MockPlatform platform;

    ui::State<std::optional<int>> missing_list{99};
    ui::UI list{ui::ListView<int>{missing_list}
        .item(1, ui::Spacer{80.0f, 30.0f})
        .item(2, ui::Spacer{80.0f, 30.0f})};
    list.resize({80.0f, 60.0f});
    list.activate(platform);
    NUI_CHECK(missing_list.get() && *missing_list.get() == 99);

    ui::State<int> missing_tab{99};
    ui::UI tabs{ui::Tabs<int>{missing_tab}
        .tab(1, "One", ui::Spacer{100.0f, 40.0f})
        .tab(2, "Two", ui::Spacer{100.0f, 40.0f})};
    tabs.resize({200.0f, 100.0f});
    tabs.activate(platform);
    NUI_CHECK(missing_tab.get() == 99);

    ui::State<std::optional<int>> left{1};
    ui::State<std::optional<int>> right{10};
    ui::UI left_list{ui::ListView<int>{left}
        .item(1, ui::Spacer{80.0f, 30.0f})
        .item(2, ui::Spacer{80.0f, 30.0f})};
    ui::UI right_list{ui::ListView<int>{right}
        .item(10, ui::Spacer{80.0f, 30.0f})
        .item(20, ui::Spacer{80.0f, 30.0f})};
    left_list.resize({80.0f, 60.0f});
    right_list.resize({80.0f, 60.0f});
    left_list.activate(platform);
    right_list.activate(platform);
    left_list.dispatch(test::key(ui::Key::Down), platform);
    NUI_CHECK(left.get() && *left.get() == 2);
    NUI_CHECK(right.get() && *right.get() == 10);
}

inline void behavior_contract() {
    public_builder_contract();
    duplicate_key_contract();
    list_keyboard_pointer_and_read_only_contract();
    tabs_keyboard_pointer_focus_and_read_only_contract();
    missing_selection_and_two_instance_contract();
}

} // namespace t036_contract
