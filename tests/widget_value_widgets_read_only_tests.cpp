#include "test_support.hpp"
#include <nativeui/calendar.hpp>
#include <nativeui/color_picker.hpp>
#include <nativeui/color_well.hpp>
#include <nativeui/date_input.hpp>
#include <nativeui/time_input.hpp>
#include <nativeui/token_field.hpp>
namespace {
void suite() {
  test::MockPlatform platform;
  ui::State<bool> locked{true};
  const auto day = std::chrono::sys_days{std::chrono::year{2026} / 10 / 4};
  ui::State<ui::Calendar::Value> selected{day};
  ui::UI calendar{ui::ReadOnly{locked, ui::Calendar{"Day", selected}}};
  calendar.resize({320, 300});
  calendar.activate(platform);
  calendar.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(selected.get() == day);
  locked.set(false);
  calendar.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(selected.get() == day + std::chrono::days{1});
  locked.set(true);
  ui::State<ui::TimeInput::Value> time{std::chrono::seconds{12}};
  ui::UI clock{ui::ReadOnly{locked, ui::TimeInput{"Time", time}}};
  clock.resize({320, 80});
  clock.activate(platform);
  clock.dispatch(test::key(ui::Key::Up), platform);
  clock.dispatch(test::text("12"), platform);
  NUI_CHECK(time.get() == std::chrono::seconds{12});
  ui::State<std::vector<std::string>> tokens{std::vector<std::string>{"a"}};
  ui::UI tags{ui::ReadOnly{locked, ui::TokenField{"Tags", tokens}}};
  tags.resize({320, 120});
  tags.activate(platform);
  tags.dispatch(test::key(ui::Key::Backspace), platform);
  tags.dispatch(test::text("b,"), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>({"a"}));
  ui::State<ui::Color> color{ui::Color{.2f, .4f, .6f, .8f}};
  const auto original = color.get();
  ui::UI picker{ui::ReadOnly{locked, ui::ColorPicker{"Color", color}}};
  picker.resize({280, 430});
  picker.activate(platform);
  picker.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(color.get() == original);
  ui::UI well{ui::ReadOnly{locked, ui::ColorWell{"Color", color}}};
  well.resize({500, 500});
  well.activate(platform);
  well.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(well.overlay_entries().empty());
  ui::UI date{ui::ReadOnly{locked, ui::DateInput{"Day", selected}}};
  date.resize({500, 500});
  date.activate(platform);
  date.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(date.overlay_entries().empty());
}
} // namespace
int main() { return test::run("widget_value_widgets_read_only", &suite); }
