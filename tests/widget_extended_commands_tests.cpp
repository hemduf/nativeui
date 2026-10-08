#include "test_support.hpp"
namespace {
void suite() {
  static_assert(static_cast<int>(ui::Key::Quit) == 19);
  static_assert(static_cast<int>(ui::Command::Redo) == 6);
  ui::InputEvent e = test::key(ui::Key::F3);
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::FindNext);
  e.shift = true;
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::FindPrevious);
  e = test::key(ui::Key::G,false,true);
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::FindNext);
  e.shift = true;
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::FindPrevious);
  e = test::key(ui::Key::F2);
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::None);
  e = test::key(ui::Key::Enter,false,true);
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::None);
  e = test::key(ui::Key::Escape);
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::None);
  e = test::key(ui::Key::F3); e.type = ui::InputType::KeyUp;
  NUI_CHECK(ui::command_from_shortcut(e) == ui::Command::None);
  int submits{}, cancels{};
  ui::State<std::string> value{""};
  ui::UI tree{ui::CommandScope{[&](ui::Command cmd) {
    if (cmd == ui::Command::Submit) { ++submits; return ui::EventResult::Handled; }
    if (cmd == ui::Command::Cancel) { ++cancels; return ui::EventResult::Handled; }
    return ui::EventResult::Ignored;
  }, ui::TextArea{"",value}}};
  test::MockPlatform p; tree.resize({200,100}); tree.activate(p);
  tree.dispatch(test::key(ui::Key::Enter),p);
  NUI_CHECK(value.get() == "\n" && submits == 0);
  e = {}; e.type = ui::InputType::Command; e.command = ui::Command::Submit;
  tree.dispatch(e,p); e.command = ui::Command::Cancel; tree.dispatch(e,p);
  NUI_CHECK(submits == 1 && cancels == 1 && value.get() == "\n");
  tree.deactivate(p);
}
}
int main() { return test::run("widget_extended_commands",&suite); }
