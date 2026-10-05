#include "test_support.hpp"

#include <nativeui/checkbox.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/knob.hpp>
#include <nativeui/range_slider.hpp>
#include <nativeui/rating.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/slider.hpp>
#include <nativeui/toggle.hpp>
#include <nativeui/toggle_button.hpp>

#include <functional>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {
enum class Policy { Enabled, ReadOnly, Deactivate };
std::string_view family_mode = "all";
std::string_view policy_mode = "all";

std::string_view policy_name(Policy policy) {
    if (policy == Policy::Enabled) return "enabled";
    if (policy == Policy::ReadOnly) return "read_only";
    return "deactivate";
}
ui::InputEvent key_up(ui::Key key) {
    auto event = test::key(key);
    event.type = ui::InputType::KeyUp;
    return event;
}

template <class Value>
void publication_fixture(std::string_view family,Policy policy,ui::State<Value>& value,
                         ui::Spec control,ui::Size size,
                         const std::vector<ui::InputEvent>& preparation,
                         const ui::InputEvent& publication,
                         const std::vector<ui::InputEvent>& completion = {}) {
    std::cout << "ORACLE " << family << ' ' << policy_name(policy) << '\n';
    const Value initial = value.get();
    int notifications{};
    auto subscription = value.observe([&](const Value&) { ++notifications; });
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{false};
    ui::UI tree{ui::Enabled{enabled,ui::ReadOnly{read_only,std::move(control)}}};
    test::MockPlatform platform;
    tree.resize(size);
    tree.activate(platform);
    ui::HeadlessRenderer renderer{size,1.0f};
    NUI_CHECK(renderer.render(tree));
    for (const auto& event : preparation) tree.dispatch(event,platform);
    NUI_CHECK(value.get() == initial);
    NUI_CHECK(notifications == 0);
    NUI_CHECK(renderer.render(tree));

    bool armed{};
    bool observed_before_publication{};
    int boundaries{};
    tree.set_invalidation_callback([&](ui::Rect) {
        if (!std::exchange(armed,false)) return;
        ++boundaries;
        // This proves the fault occurs before State publication, not in its observer.
        observed_before_publication = value.get() == initial && notifications == 0;
        if (policy == Policy::Enabled) enabled.set(false);
        else if (policy == Policy::ReadOnly) read_only.set(true);
        else tree.deactivate(platform);
    });
    armed = true;
    tree.dispatch(publication,platform);
    NUI_CHECK(boundaries == 1);
    NUI_CHECK(observed_before_publication);
    NUI_CHECK(value.get() == initial);
    NUI_CHECK(notifications == 0);

    tree.clear_invalidation_callback();
    // Cancel any retained interaction before a new application-authorized attempt.
    tree.deactivate(platform);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
    enabled.set(true);
    read_only.set(false);
    tree.activate(platform);
    tree.resize(size);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(value.get() == initial && notifications == 0);
    for (const auto& event : preparation) tree.dispatch(event,platform);
    NUI_CHECK(renderer.render(tree));
    tree.dispatch(publication,platform);
    NUI_CHECK(!(value.get() == initial));
    NUI_CHECK(notifications == 1);
    for (const auto& event : completion) tree.dispatch(event,platform);
    tree.deactivate(platform);
    NUI_CHECK(notifications == 1);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}

void family_suite(std::string_view family,Policy policy) {
    const auto black = ui::Color{0.0f,0.0f,0.0f,1.0f};
    const auto red = ui::Color{1.0f,0.0f,0.0f,1.0f};
    if (family == "checkbox") {
        ui::State<bool> value{false};
        ui::CheckboxStyle style;
        style.base.box_fill = black;
        style.pressed.box_fill = red;
        publication_fixture(family,policy,value,ui::make_spec(ui::Checkbox{value,"Option"}.style(style)),
                            {220.0f,54.0f},{test::key(ui::Key::Space)},key_up(ui::Key::Space));
    } else if (family == "toggle") {
        ui::State<bool> value{false};
        ui::ToggleStyle style;
        style.base.fill = black;
        style.pressed.fill = red;
        publication_fixture(family,policy,value,ui::make_spec(ui::Toggle{"Option",value}.style(style)),
                            {220.0f,54.0f},{},test::key(ui::Key::Space),{key_up(ui::Key::Space)});
    } else if (family == "knob") {
        ui::State<float> value{0.5f};
        publication_fixture(family,policy,value,ui::make_spec(ui::Knob{"Value",value}),
                            {176.0f,182.0f},{},test::key(ui::Key::Right));
    } else if (family == "slider") {
        ui::State<float> value{0.2f};
        ui::SliderStyle style;
        style.base.thumb = black;
        style.pressed.thumb = red;
        const auto down = test::pointer(ui::InputType::PointerDown,160.0f,20.0f);
        const auto up = test::pointer(ui::InputType::PointerUp,160.0f,20.0f);
        publication_fixture(family,policy,value,ui::make_spec(ui::Slider{value}.style(style)),
                            {200.0f,40.0f},{},down,{up});
    } else if (family == "range_slider") {
        ui::State<ui::RangeValue> value{ui::RangeValue{0.2f,0.8f}};
        ui::SliderStyle style;
        style.base.thumb = black;
        style.pressed.thumb = red;
        const auto down = test::pointer(ui::InputType::PointerDown,65.0f,20.0f);
        const auto up = test::pointer(ui::InputType::PointerUp,65.0f,20.0f);
        publication_fixture(family,policy,value,ui::make_spec(ui::RangeSlider{value}.style(style)),
                            {200.0f,40.0f},{},down,{up});
    } else if (family == "rating") {
        ui::State<double> value{1.0};
        publication_fixture(family,policy,value,ui::make_spec(ui::Rating{"Note",value}),
                            {150.0f,30.0f},{},test::key(ui::Key::Right));
    } else if (family == "toggle_button") {
        ui::State<bool> value{false};
        ui::ToggleButtonStyle style;
        style.base.fill = black;
        style.pressed.fill = red;
        publication_fixture(family,policy,value,ui::make_spec(ui::ToggleButton{"Bold",value}.style(style)),
                            {120.0f,40.0f},{test::key(ui::Key::Space)},key_up(ui::Key::Space));
    }
}

void suite() {
    constexpr std::string_view families[]{"checkbox","toggle","knob","slider","range_slider","rating","toggle_button"};
    constexpr Policy policies[]{Policy::Enabled,Policy::ReadOnly,Policy::Deactivate};
    bool matched{};
    for (const auto family : families) {
        if (family_mode != "all" && family_mode != family) continue;
        for (const auto policy : policies) {
            if (policy_mode != "all" && policy_mode != policy_name(policy)) continue;
            matched = true;
            family_suite(family,policy);
        }
    }
    NUI_CHECK(matched);
}
}

int main(int argc,char** argv) {
    if (argc > 1) family_mode = argv[1];
    if (argc > 2) policy_mode = argv[2];
    return test::run("value_publication_boundary",&suite);
}
