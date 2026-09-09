#include "test_support.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace {

ui::InputEvent key_up(ui::Key key) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyUp;
    event.key = key;
    return event;
}

bool pixel_near(ui::Rgba8 pixel, ui::Color color, int tolerance = 4) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(value * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

bool same_pixel(ui::Rgba8 a, ui::Rgba8 b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
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

    {
        ui::State<bool> checked{false};
        ui::State<bool> enabled{false};
        ui::UI tree{ui::Enabled{enabled, ui::Checkbox{checked, "Disabled"}}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Ignored);
        NUI_CHECK(!checked.get());
    }

    {
        ui::State<bool> checked{false};
        ui::State<bool> read_only{true};
        int writes = 0;
        auto observer = checked.observe([&](const bool&) { ++writes; });
        ui::UI tree{ui::ReadOnly{read_only, ui::Checkbox{checked, "Read only"}}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(key_up(ui::Key::Space), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(!checked.get());
        NUI_CHECK(writes == 0);
    }

    // T059 owns the cancellation caused by becoming unavailable. T031 must not
    // leave a second capture/cancel behind.
    {
        ui::State<bool> checked{false};
        ui::State<bool> enabled{true};
        ui::UI tree{ui::Enabled{enabled, ui::Checkbox{checked, "Transition"}}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        enabled.set(false);
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
        NUI_CHECK(!checked.get());
    }
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

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 3);

    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 1);
    tree.dispatch(test::key(ui::Key::Left), platform);
    NUI_CHECK(selected.get() == 3);

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
    NUI_CHECK(selected.get() == 99);
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 2);
}

void radio_read_only_contract() {
    test::MockPlatform platform;
    ui::State<int> selected{1};
    ui::State<bool> read_only{true};
    int writes = 0;
    auto observer = selected.observe([&](const int&) { ++writes; });
    ui::RadioGroup<int> group{selected};

    ui::UI tree{ui::ReadOnly{
        read_only,
        ui::Column{
            ui::RadioButton{group, 1, "One"},
            ui::RadioButton{group, 2, "Two"},
            ui::RadioButton{group, 3, "Three"}
        }
    }};
    tree.resize({220.0f, 140.0f});
    tree.activate(platform);

    // Column has 24 px default padding, so target an actual option rather than
    // the wrapper padding. The previous (20,20) probe correctly hit no focusable
    // child and exposed a test-coordinate error on every platform.
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerDown, 40.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerUp, 40.0f, 40.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(selected.get() == 1);
    NUI_CHECK(writes == 0);

    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(selected.get() == 1);
    NUI_CHECK(writes == 0);

    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(selected.get() == 1);
    NUI_CHECK(writes == 0);

    read_only.set(false);
    tree.dispatch(test::key(ui::Key::Space), platform);
    tree.dispatch(key_up(ui::Key::Space), platform);
    NUI_CHECK(selected.get() == 2);
    NUI_CHECK(writes == 1);
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
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(left_selected.get() == 2);
        NUI_CHECK(right_selected.get() == 1);
    }

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

void observer_reentrancy_is_single_activation() {
    test::MockPlatform platform;

    {
        ui::State<bool> checked{false};
        int notifications = 0;
        auto observer = checked.observe([&](const bool& value) {
            ++notifications;
            if (value) checked.set(false);
        });
        ui::UI tree{ui::Checkbox{checked, "Reentrant"}};
        tree.resize({180.0f, 64.0f});
        tree.activate(platform);
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 20.0f), platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 20.0f, 20.0f), platform);
        NUI_CHECK(!checked.get());
        NUI_CHECK(notifications == 2);
    }

    {
        ui::State<int> selected{1};
        int notifications = 0;
        auto observer = selected.observe([&](const int& value) {
            ++notifications;
            if (value == 2) selected.set(3);
        });
        ui::RadioGroup<int> group{selected};
        ui::UI tree{ui::Column{
            ui::RadioButton{group, 1, "One"},
            ui::RadioButton{group, 2, "Two"},
            ui::RadioButton{group, 3, "Three"}
        }};
        tree.resize({180.0f, 120.0f});
        tree.activate(platform);
        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(selected.get() == 3);
        NUI_CHECK(notifications == 2);
    }
}

void visual_state_goldens() {
    constexpr ui::Size size{180.0f, 64.0f};
    ui::HeadlessRenderer renderer{size, 1.0f};

    // A single root control fills the headless viewport, so both indicators are
    // vertically centered at y=32 rather than at their 32px intrinsic height.
    ui::State<bool> checked{false};
    ui::UI checkbox{ui::Checkbox{checked, "Visual"}};
    NUI_CHECK(renderer.render(checkbox));
    NUI_CHECK(pixel_near(renderer.pixel(12, 32), ui::colors::input));
    const auto checkbox_normal_border = renderer.pixel(3, 32);

    checked.set(true);
    NUI_CHECK(renderer.render(checkbox));
    // Sample checked fill away from both checkmark strokes and rounded edges.
    NUI_CHECK(pixel_near(renderer.pixel(17, 35), ui::colors::accent));

    test::MockPlatform platform;
    checkbox.resize(size);
    checkbox.activate(platform);
    NUI_CHECK(renderer.render(checkbox));
    const auto checkbox_focused_border = renderer.pixel(3, 32);
    NUI_CHECK(!same_pixel(checkbox_normal_border, checkbox_focused_border));

    ui::State<bool> checkbox_enabled{false};
    ui::UI disabled_checkbox{
        ui::Enabled{checkbox_enabled, ui::Checkbox{checked, "Disabled"}}
    };
    NUI_CHECK(renderer.render(disabled_checkbox));
    NUI_CHECK(pixel_near(renderer.pixel(17, 35), ui::colors::input));

    ui::State<int> selected{2};
    ui::RadioGroup<int> group{selected};
    ui::UI radio{ui::RadioButton{group, 1, "Visual"}};
    NUI_CHECK(renderer.render(radio));
    NUI_CHECK(pixel_near(renderer.pixel(12, 32), ui::colors::input));
    const auto radio_normal_outer = renderer.pixel(20, 32);

    selected.set(1);
    NUI_CHECK(renderer.render(radio));
    NUI_CHECK(pixel_near(renderer.pixel(12, 32), ui::colors::accent));

    radio.resize(size);
    radio.activate(platform);
    NUI_CHECK(renderer.render(radio));
    const auto radio_focused_outer = renderer.pixel(20, 32);
    NUI_CHECK(!same_pixel(radio_normal_outer, radio_focused_outer));

    ui::State<bool> radio_enabled{false};
    ui::RadioGroup<int> disabled_group{selected};
    ui::UI disabled_radio{
        ui::Enabled{radio_enabled, ui::RadioButton{disabled_group, 1, "Disabled"}}
    };
    NUI_CHECK(renderer.render(disabled_radio));
    NUI_CHECK(pixel_near(renderer.pixel(12, 32), ui::colors::textMuted));
}

void suite() {
    checkbox_activation_contract();
    checkbox_availability_contract();
    radio_typed_selection_contract();
    radio_navigation_and_tab_entry();
    radio_no_match_tab_fallback();
    radio_read_only_contract();
    radio_group_isolation_and_remount();
    observer_reentrancy_is_single_activation();
    visual_state_goldens();
}

} // namespace

int main() { return test::run("checkbox_radio", &suite); }
