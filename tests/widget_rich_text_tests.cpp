#include "test_support.hpp"
#include <nativeui/enabled.hpp>
#include <nativeui/if.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/rich_text.hpp>
#include <stdexcept>
#include "include/core/SkSurface.h"
#include "include/core/SkImageInfo.h"
namespace ui {
struct TreeTestAccess {
  static Rect action_bounds(Tree& tree,const std::string& name) {
    std::vector<Node*> nodes{tree.root_.get()};
    while(!nodes.empty()) {
      auto* node=nodes.back();nodes.pop_back();
      const auto info=node->component->semantics();
      if(info.role==SemanticRole::Button && info.name==name)return node->bounds;
      for(auto& child:node->children)nodes.push_back(child.get());
    }
    throw test::Failure("RichText action missing");
  }
};
}

namespace {
ui::InputEvent up(ui::Key key) {
  auto value = test::key(key);
  value.type = ui::InputType::KeyUp;
  return value;
}
ui::Rect action_bounds(ui::Tree &tree, const std::string &name) {
  return ui::TreeTestAccess::action_bounds(tree,name);
}
void validation_and_semantics() {
  for (const auto &spans : std::vector<std::vector<ui::RichTextSpan>>{
           {{.text = "action", .on_activate = [] {}}},
           {{.id = "duplicate", .text = "one", .on_activate = [] {}},
            {.id = "duplicate", .text = "two", .on_activate = [] {}}}}) {
    bool caught{};
    try {
      ui::UI tree{ui::RichText{spans}};
    } catch (const std::invalid_argument &) {
      caught = true;
    }
    NUI_CHECK(caught);
  }
  ui::UI tree{ui::RichText{std::vector<ui::RichTextSpan>{
      {.text = "Read "},
      {.id = "guide", .text = "guide", .on_activate = [] {}},
      {.id = "empty", .text = "", .on_activate = [] {}},
      {.text = " now"}}}};
  int actions{}, texts{};
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (!info)
      continue;
    if (info->role == ui::SemanticRole::Text) {
      ++texts;
      NUI_CHECK(info->text_value == "Read guide now");
      NUI_CHECK(info->actions.empty());
    }
    if (info->role == ui::SemanticRole::Button) {
      ++actions;
      NUI_CHECK(info->name == "guide" &&
                info->supports(ui::SemanticAction::Activate));
    }
  }
  NUI_CHECK(actions == 1 && texts == 1);
}
void keyboard_repeat_throw_and_read_only() {
  int first{}, second{};
  bool fail = true;
  ui::State<bool> read_only{true};
  ui::UI tree{ui::ReadOnly{
      read_only, ui::RichText{std::vector<ui::RichTextSpan>{
                {.text = "Read "},
                {.id = "first",
                 .text = "first",
                 .on_activate =
                     [&] {
                       ++first;
                       if (fail)
                         throw std::runtime_error("action");
                     }},
                {.text = " then "},
                {.id = "second", .text = "second", .on_activate = [&] {
                   ++second;
                 }}}}}};
  test::MockPlatform platform;
  tree.resize({240, 80});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && first == 1 && second == 0);
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(first == 1);
  tree.dispatch(up(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(first == 2);
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(second == 0);
  tree.dispatch(up(ui::Key::Space), platform);
  NUI_CHECK(second == 1);
  tree.dispatch(test::key(ui::Key::Tab, true), platform);
  tree.dispatch(up(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(first == 3 && second == 1);
  NUI_CHECK(!platform.text_input_active && platform.clipboard_write_count == 0);
}
void remove_and_disable_pressed() {
  ui::State<bool> present{true}, enabled{true};
  int calls{};
  ui::Tree tree{ui::compile(ui::make_spec(ui::If{
      present, ui::Enabled{enabled, ui::RichText{std::vector<ui::RichTextSpan>{
                                        {.text = "plain "},
                                        {.id = "action",
                                         .text = "action",
                                         .on_activate = [&] { ++calls; }}}}}}))};
  test::MockPlatform platform;
  tree.mount();tree.layout({200,70});tree.activate_focus(platform);
  auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200,70));
  NUI_CHECK(surface);
  const auto frame=[&]{tree.layout({200,70});tree.paint(*surface->getCanvas(),platform);};
  frame();
  auto b = action_bounds(tree, "action");
  auto down = test::pointer(ui::InputType::PointerDown, b.x + b.w * .5f,
                            b.y + b.h * .5f);
  tree.dispatch(down, platform);
  enabled.set(false);
  frame();
  auto release = down;
  release.type = ui::InputType::PointerUp;
  tree.dispatch(release, platform);
  NUI_CHECK(calls == 0 && platform.pointer_capture_begin_count ==
                              platform.pointer_capture_end_count);
  enabled.set(true);
  frame();
  b = action_bounds(tree, "action");
  down.position = {b.x + b.w * .5f, b.y + b.h * .5f};
  release.position = down.position;
  tree.dispatch(down, platform);
  present.set(false);
  frame();
  tree.dispatch(release, platform);
  NUI_CHECK(calls == 0 && platform.pointer_capture_begin_count ==
                              platform.pointer_capture_end_count);
  present.set(true);
  frame();
  // If removal retires the focused NodeId. A fresh pointer contact focuses
  // the newly mounted action; cancelling it must not activate the action.
  b = action_bounds(tree, "action");
  down.position = {b.x + b.w * .5f, b.y + b.h * .5f};
  tree.dispatch(down, platform);
  auto cancel = down;
  cancel.type = ui::InputType::PointerCancel;
  tree.dispatch(cancel, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1 && platform.pointer_capture_begin_count ==
                            platform.pointer_capture_end_count);
  tree.deactivate_focus(platform);tree.unmount();
}
void unicode_reflow_multi_instance_and_zero_width() {
  const std::string value =
      "office A\xcc\x81 👨‍👩‍👧‍👦 العربية "
      "abcdefghijklmnopqrstuvwxyz";
  auto spec =
      ui::RichText{std::vector<ui::RichTextSpan>{{.text = value}}}.spec();
  ui::UI wide{spec}, narrow{spec};
  const auto a = wide.measure(ui::Constraints::loose({500, 1000})).preferred;
  const auto b = narrow.measure(ui::Constraints::loose({60, 1000})).preferred;
  const auto z = narrow.measure(ui::Constraints::loose({0, 1000})).preferred;
  NUI_CHECK(b.w <= 60 && b.h > a.h && std::isfinite(z.h) && z.w == 0);
  wide.resize({500, 180});
  narrow.resize({60, 500});
  ui::HeadlessRenderer wr{{500, 180}, 1}, nr{{60, 500}, 2};
  NUI_CHECK(wr.render(wide) && nr.render(narrow));
  const auto after =
      wide.measure(ui::Constraints::loose({500, 1000})).preferred;
  NUI_CHECK(a.w == after.w && a.h == after.h);
}
void one_spec_two_uis_have_independent_key_contacts() {
  int calls{};
  auto spec = ui::RichText{
      std::vector<ui::RichTextSpan>{
          {.id = "action", .text = "Action", .on_activate = [&] {
             ++calls;
           }}}}.spec();
  ui::UI left{spec}, right{spec};
  test::MockPlatform lp, rp;
  left.resize({160, 80});
  right.resize({70, 100});
  left.activate(lp);
  right.activate(rp);
  left.dispatch(test::key(ui::Key::Enter), lp);
  right.dispatch(test::key(ui::Key::Enter), rp);
  NUI_CHECK(calls == 2);
  left.dispatch(test::key(ui::Key::Enter), lp);
  NUI_CHECK(calls == 2);
  right.dispatch(test::key(ui::Key::Space), rp);
  left.deactivate(lp);
  right.dispatch(up(ui::Key::Space), rp);
  NUI_CHECK(calls == 3);
  NUI_CHECK(lp.pointer_capture_begin_count == lp.pointer_capture_end_count &&
            rp.pointer_capture_begin_count == rp.pointer_capture_end_count);
}
void an_older_pointer_release_cannot_complete_a_newer_contact() {
  int calls{};
  ui::UI tree{ui::RichText{std::vector<ui::RichTextSpan>{
      {.id = "action", .text = "Action", .on_activate = [&] { ++calls; }}}}};
  test::MockPlatform platform;
  tree.resize({120, 70});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{120, 70}, 1};
  NUI_CHECK(renderer.render(tree));
  auto first = test::pointer(ui::InputType::PointerDown, 10, 10);
  first.pointer.id = 1;
  first.pointer.type = ui::PointerType::Touch;
  first.pointer.primary = true;
  auto second = first;
  second.pointer.id = 2;
  tree.dispatch(first, platform);
  tree.dispatch(second, platform);
  first.type = ui::InputType::PointerUp;
  tree.dispatch(first, platform);
  NUI_CHECK(calls == 0);
  second.type = ui::InputType::PointerUp;
  tree.dispatch(second, platform);
  NUI_CHECK(calls == 1);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void suite() {
  an_older_pointer_release_cannot_complete_a_newer_contact();
  one_spec_two_uis_have_independent_key_contacts();
  validation_and_semantics();
  keyboard_repeat_throw_and_read_only();
  remove_and_disable_pressed();
  unicode_reflow_multi_instance_and_zero_width();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "keyboard")
    return test::run("rich_text_keyboard", keyboard_repeat_throw_and_read_only);
  return test::run("widget_rich_text", suite);
}
