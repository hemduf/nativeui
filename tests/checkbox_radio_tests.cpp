#include "test_support.hpp"

#include <cstdlib>
#include <string>

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

void checkbox_activation_contract() {
    test::MockPlatform platform;
    ui::State<bool> checked{false};
    ui::UI tree{ui::Checkbox{checked, "Enable"}};
    tree.resize({180.0f, 64.0f});
    tree.activate(platform);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform);
    NUI_CHECK(!checked.get());

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(checked.get());

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerCancel, 20.0f, 20.0f), platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    NUI_CHECK(checked.get());

    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(checked.get());
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(!checked.get());

    tree.dispatch(test::key(ui::Key::Enter), platform);
    tree.dispatch(key_up(ui::Key::Enter), platform);
    NUI_CHECK(!checked.get());
}

void checkbox_availability_contract() {
    test::MockPlatform platform;

    ui::State<bool> checked{false};
    ui::State<bool> enabled{false};
    ui::UI disabled_tree{
        ui::Enabled{enabled, ui::Checkbox{checked, "Disabled"}}
    };
    disabled_tree.resize({180.0f, 64.0f});
    disabled_tree.activate(platform);
    NUI_CHECK(disabled_tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(!checked.get());

    ui::State<bool> read_only{true};
    ui::UI read_only_tree{
        ui::ReadOnly{read_only, ui::Checkbox{checked, "Read only"}}
    };
    read_only_tree.resize({180.0f, 64.0f});
    read_only_tree.activate(platform);
    read_only_tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
    read_only_tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
    read_only_tree.dispatch(test::key(ui::Key::Space), platform);
    read_only_tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(!checked.get());
}

enum class Choice { A, B, C, Missing };

void radio_typed_selection_contract() {
    test::MockPlatform platform;

    {
        ui::State<int> selected{2};
        ui::RadioGroup<int> group{selected};
        ui::UI tree{ui::Row{
            ui::RadioButton{group, 1, "One"},
            ui::RadioButton{group, 2, "Two"},
            ui::RadioButton{group, 3, "Three"}
        }};
        tree.resize({360.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(selected.get() == 1);
    }

    {
        ui::State<Choice> selected{Choice::Missing};
        ui::RadioGroup<Choice> group{selected};
        ui::UI tree{ui::Column{
            ui::RadioButton{group, Choice::A, "A"},
            ui::RadioButton{group, Choice::B, "B"},
            ui::RadioButton{group, Choice::C, "C"}
        }};
        tree.resize({200.0f, 150.0f});
        tree.activate(platform);
        NUI_CHECK(selected.get() == Choice::Missing);
    }

    {
        ui::State<std::string> selected{"beta"};
        ui::RadioGroup<std::string> group{selected};
        ui::UI tree{ui::Column{
            ui::RadioButton{group, std::string{"alpha"}, "Alpha"},
            ui::RadioButton{group, std::string{"beta"}, "Beta"}
        }};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);
        NUI_CHECK(selected.get() == "beta");
    }
}

void radio_navigation_and_tab_entry() {
    test::MockPlatform platform;
    ui::State<int> selected{1};
    ui::State<bool> middle_enabled{false};
    ui::RadioGroup<int> group{selected};

    ui::UI tree{ui::Column{
        ui::Button{"Before", [] {}},
        ui::RadioButton{group, 1, "One"},
        ui::Enabled{middle_enabled, ui::RadioButton{group, 2, "Two"}},
        ui::RadioButton{group, 3, "Three"},
        ui::Button{"After", [] {}}
    }};
    tree.resize({220.0f, 220.0f});
    tree.activate(platform);

    // Tab enters at the selected enabled option. Right then skips the disabled
    // middle option and selects/focuses Three.
    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 3);

    // Wrap within the group.
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 1);
    tree.dispatch(test::key(ui::Key::Left), platform);
    NUI_CHECK(selected.get() == 3);

    // A normal Tab from inside the group leaves the group in one step.
    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(selected.get() == 3);
}

void radio_no_match_tab_fallback() {
    test::MockPlatform platform;
    ui::State<int> selected{99};
    ui::RadioGroup<int> group{selected};
    ui::UI tree{ui::Column{
        ui::Button{"Before", [] {}},
        ui::RadioButton{group, 1, "One"},
        ui::RadioButton{group, 2, "Two"}
    }};
    tree.resize({220.0f, 140.0f});
    tree.activate(platform);

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 2);
}

void radio_group_isolation_and_remount() {
    test::MockPlatform platform;
    ui::State<int> left_selected{1};
    ui::State<int> right_selected{1};
    ui::RadioGroup<int> left{left_selected};
    ui::RadioGroup<int> right{right_selected};

    {
        ui::UI tree{ui::Column{
            ui::RadioButton{left, 1, "Same"},
            ui::RadioButton{left, 2, "Same"},
            ui::RadioButton{right, 1, "Same"},
            ui::RadioButton{right, 2, "Same"}
        }};
        tree.resize({220.0f, 180.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 60.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 60.0f), platform);
        NUI_CHECK(left_selected.get() == 2);
        NUI_CHECK(right_selected.get() == 1);
    }

    // Reordering/remounting does not rewrite or transfer selection by address.
    {
        ui::UI tree{ui::Column{
            ui::RadioButton{left, 2, "Second"},
            ui::RadioButton{left, 1, "First"}
        }};
        tree.resize({220.0f, 100.0f});
        tree.activate(platform);
        NUI_CHECK(left_selected.get() == 2);
    }
}

void suite() {
    checkbox_activation_contract();
    checkbox_availability_contract();
    radio_typed_selection_contract();
    radio_navigation_and_tab_entry();
    radio_no_match_tab_fallback();
    radio_group_isolation_and_remount();
}

} // namespace

int main() { return test::run("checkbox_radio", &suite); }
