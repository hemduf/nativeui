#include "test_support.hpp"
#include <nativeui/sidebar.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>

namespace {
ui::SidebarStyle compact_style() {
  ui::SidebarStyle value;
  value.padding = 0;
  value.gap = 0;
  return value;
}
void render(ui::UI &tree, ui::Size size = {200, 220}) {
  ui::HeadlessRenderer renderer{size, 1};
  NUI_CHECK(renderer.render(tree));
}
void key(ui::UI &tree, test::MockPlatform &platform, ui::Key value) {
  tree.dispatch(test::key(value), platform);
}
void click(ui::UI &tree, test::MockPlatform &platform, float x, float y) {
  tree.dispatch(test::pointer(ui::InputType::PointerDown, x, y), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, x, y), platform);
}
std::optional<std::pair<ui::NodeId, ui::SemanticInfo>>
semantics_named(ui::UI &tree, const std::string &name) {
  for (ui::NodeId id = 1; id < 256; ++id)
    if (const auto info = tree.component_semantics(id);
        info && info->name == name)
      return std::pair{id, *info};
  return {};
}
void sections_root_items_navigation_and_unknown_selection() {
  ui::State<std::optional<int>> selected{std::nullopt};
  ui::State<bool> open{true};
  std::vector<int> navigated;
  ui::UI tree{
      ui::Sidebar<int>{selected}
          .item(10, "Root")
          .section("mail", "Mail", open)
          .item(20, "Inbox")
          .item(21, "Disabled", false)
          .item(22, "Sent")
          .style(compact_style())
          .on_navigate([&](const int &value) { navigated.push_back(value); })};
  test::MockPlatform platform;
  tree.resize({200, 220});
  tree.activate(platform);
  render(tree);
  key(tree, platform, ui::Key::Home);
  NUI_CHECK(selected.get() == 10 && navigated == std::vector<int>{10});
  key(tree, platform, ui::Key::Down);
  NUI_CHECK(selected.get() == 10 && navigated.size() == 1);
  key(tree, platform, ui::Key::Right);
  NUI_CHECK(selected.get() == 20 && navigated.size() == 2);
  key(tree, platform, ui::Key::Down);
  NUI_CHECK(selected.get() == 22 && navigated.size() == 3);
  key(tree, platform, ui::Key::Enter);
  NUI_CHECK(navigated == std::vector<int>({10, 20, 22, 22}));
  click(tree, platform, 35, 45);
  NUI_CHECK(!open.get() && selected.get() == 22 && navigated.size() == 4);
  const auto header = semantics_named(tree, "Mail");
  const auto child = semantics_named(tree, "Inbox");
  NUI_CHECK(header && child &&
            header->second.expanded == ui::SemanticExpandedState::Collapsed);
  NUI_CHECK(tree.component_availability(child->first)->visibility ==
            ui::VisibilityMode::Collapsed);
  key(tree, platform, ui::Key::Right);
  NUI_CHECK(open.get() && selected.get() == 22 && navigated.size() == 4);
  selected.set(999);
  render(tree);
  NUI_CHECK(selected.get() == 999 && navigated.size() == 4);
  const auto info = semantics_named(tree, "Sent");
  NUI_CHECK(info && !info->second.selected);
  key(tree, platform, ui::Key::Home);
  NUI_CHECK(selected.get() == 10 && navigated.size() == 5);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void independent_instances_do_not_share_contacts_or_geometry() {
  ui::State<std::optional<int>> selected{std::nullopt};
  int calls = 0;
  auto spec = ui::make_spec(ui::Sidebar<int>{selected}
                                .item(1, "One")
                                .item(2, "Two")
                                .style(compact_style())
                                .on_navigate([&](const int &) { ++calls; }));
  ui::UI first{ui::Spec{spec}}, second{ui::Spec{spec}};
  test::MockPlatform a, b;
  first.resize({200, 80});
  second.resize({300, 80});
  first.activate(a);
  second.activate(b);
  render(first, {200, 80});
  render(second, {300, 80});
  first.dispatch(test::pointer(ui::InputType::PointerDown, 30, 15), a);
  second.dispatch(test::pointer(ui::InputType::PointerUp, 30, 15), b);
  NUI_CHECK(!selected.get() && calls == 0);
  first.dispatch(test::pointer(ui::InputType::PointerUp, 30, 15), a);
  NUI_CHECK(selected.get() == 1 && calls == 1);
  click(second, b, 250, 48);
  NUI_CHECK(selected.get() == 2 && calls == 2);
  NUI_CHECK(a.pointer_capture_begin_count == a.pointer_capture_end_count &&
            b.pointer_capture_begin_count == b.pointer_capture_end_count);
}
void expired_selection_and_open_bindings_are_separate_domains() {
  auto owner =
      std::make_unique<ui::State<std::optional<int>>>(std::optional<int>{1});
  ui::State<bool> open{true};
  int calls = 0;
  ui::UI tree{ui::Sidebar<int>{owner->binding()}
                  .section("group", "Group", open)
                  .item(1, "One")
                  .item(2, "Two")
                  .style(compact_style())
                  .on_navigate([&](const int &) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({200, 150});
  tree.activate(platform);
  render(tree, {200, 150});
  owner.reset();
  render(tree, {200, 150});
  click(tree, platform, 20, 10);
  NUI_CHECK(!open.get() && calls == 0);
  click(tree, platform, 20, 10);
  NUI_CHECK(open.get());
  click(tree, platform, 20, 76);
  NUI_CHECK(calls == 0);
  const auto item = semantics_named(tree, "One");
  NUI_CHECK(item && item->second.read_only && item->second.selected);
  auto section_owner = std::make_unique<ui::State<bool>>(true);
  ui::State<std::optional<int>> live{std::nullopt};
  ui::UI other{ui::Sidebar<int>{live}
                   .section("group", "Group", section_owner->binding())
                   .item(3, "Three")
                   .style(compact_style())};
  other.resize({200, 120});
  other.activate(platform);
  render(other, {200, 120});
  section_owner.reset();
  click(other, platform, 20, 10);
  key(other, platform, ui::Key::Down);
  NUI_CHECK(live.get() == 3);
  const auto header = semantics_named(other, "Group");
  NUI_CHECK(header &&
            header->second.expanded == ui::SemanticExpandedState::Expanded &&
            header->second.read_only);
}
void first_observer_fault_recovers_closed_sections_without_navigation() {
  ui::State<std::optional<int>> selected{1};
  ui::State<bool> open{true};
  bool fail = true;
  int calls = 0;
  const auto observer = open.observe([&](bool) {
    if (std::exchange(fail, false))
      throw std::runtime_error("first open observer fault");
  });
  ui::UI tree{ui::Sidebar<int>{selected}
                  .section("group", "Group", open)
                  .item(1, "One")
                  .style(compact_style())
                  .on_navigate([&](const int &) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({200, 120});
  tree.activate(platform);
  render(tree, {200, 120});
  bool caught = false;
  try {
    open.set(false);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  render(tree, {200, 120});
  const auto header = semantics_named(tree, "Group");
  const auto item = semantics_named(tree, "One");
  NUI_CHECK(caught && header && item &&
            header->second.expanded == ui::SemanticExpandedState::Collapsed &&
            calls == 0 && selected.get() == 1);
  NUI_CHECK(tree.component_availability(item->first)->visibility ==
            ui::VisibilityMode::Collapsed);
  key(tree, platform, ui::Key::Right);
  NUI_CHECK(open.get() && calls == 0);
}
struct CopyProbe {
  bool armed{};
  int calls{};
};
struct ThrowOnCopy {
  std::shared_ptr<CopyProbe> state;
  explicit ThrowOnCopy(std::shared_ptr<CopyProbe> value)
      : state(std::move(value)) {}
  ThrowOnCopy(const ThrowOnCopy &other) : state(other.state) {
    if (std::exchange(state->armed, false))
      throw std::runtime_error("navigate copy fault");
  }
  void operator()(const int &) const { ++state->calls; }
};
void terminal_release_disarms_before_callback_copy_and_started_fault_is_not_replayed() {
  const auto probe = std::make_shared<CopyProbe>();
  ui::State<std::optional<int>> selected{std::nullopt};
  ui::UI tree{ui::Sidebar<int>{selected}
                  .item(1, "One")
                  .style(compact_style())
                  .on_navigate(ThrowOnCopy{probe})};
  test::MockPlatform platform;
  tree.resize({200, 80});
  tree.activate(platform);
  render(tree, {200, 80});
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 20, 15), platform);
  probe->armed = true;
  bool caught = false;
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 20, 15), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && selected.get() == 1 && probe->calls == 0);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 20, 15), platform);
  NUI_CHECK(probe->calls == 0);
  key(tree, platform, ui::Key::Enter);
  NUI_CHECK(probe->calls == 1);
  bool started = true;
  int calls = 0;
  ui::State<std::optional<int>> another{std::nullopt};
  ui::UI fault{ui::Sidebar<int>{another}
                   .item(2, "Two")
                   .style(compact_style())
                   .on_navigate([&](const int &) {
                     ++calls;
                     if (std::exchange(started, false))
                       throw std::runtime_error("navigate started fault");
                   })};
  fault.resize({200, 80});
  fault.activate(platform);
  render(fault, {200, 80});
  caught = false;
  try {
    click(fault, platform, 20, 15);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  render(fault, {200, 80});
  fault.dispatch(test::pointer(ui::InputType::PointerUp, 20, 15), platform);
  NUI_CHECK(caught && another.get() == 2 && calls == 1);
  key(fault, platform, ui::Key::Enter);
  NUI_CHECK(calls == 2);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void interactive_accessory_consumes_its_own_action_and_duplicate_domains_are_rejected() {
  ui::State<std::optional<int>> selected{std::nullopt};
  int actions = 0, navigation = 0;
  ui::UI tree{ui::Sidebar<int>{selected}
                  .item(1, "One", ui::Button{"Action", [&] { ++actions; }})
                  .style(compact_style())
                  .on_navigate([&](const int &) { ++navigation; })};
  test::MockPlatform platform;
  tree.resize({200, 100});
  tree.activate(platform);
  render(tree, {200, 100});
  click(tree, platform, 155, 16);
  NUI_CHECK(actions == 1 && navigation == 0 && !selected.get());
  for (int mode = 0; mode < 2; ++mode) {
    bool caught = false;
    try {
      if (!mode)
        (void)ui::make_spec(
            ui::Sidebar<int>{selected}.item(1, "a").item(1, "b"));
      else
        (void)ui::make_spec(
            ui::Sidebar<int>{selected}.section("x", "a").section("x", "b"));
    } catch (const std::invalid_argument &) {
      caught = true;
    }
    NUI_CHECK(caught);
  }
  ui::UI empty{ui::Sidebar<int>{selected}};
  render(empty, {80, 40});
}
void readonly_typeahead_and_navigation_retirement() {
  ui::State<std::optional<int>> selected{1};
  ui::State<bool> readonly{true};
  int calls = 0;
  ui::UI tree{
      ui::ReadOnly{readonly, ui::Sidebar<int>{selected}
                                 .item(1, "Alpha")
                                 .item(2, "Beta")
                                 .style(compact_style())
                                 .on_navigate([&](const int &) { ++calls; })}};
  test::MockPlatform platform;
  tree.resize({200, 80});
  tree.activate(platform);
  render(tree, {200, 80});
  ui::InputEvent input;
  input.type = ui::InputType::TextInput;
  input.text = "b";
  tree.dispatch(input, platform);
  key(tree, platform, ui::Key::Enter);
  NUI_CHECK(selected.get() == 1 && calls == 0);
  readonly.set(false);
  key(tree, platform, ui::Key::Enter);
  NUI_CHECK(selected.get() == 2 && calls == 1);
  ui::State<bool> show{true};
  ui::State<std::optional<int>> chosen{std::nullopt};
  ui::UI *owner = nullptr;
  int retired = 0;
  ui::UI other{ui::If{show, [&] {
                        return ui::make_spec(ui::Sidebar<int>{chosen}
                                                 .item(3, "Three")
                                                 .style(compact_style())
                                                 .on_navigate([&](const int &) {
                                                   ++retired;
                                                   show.set(false);
                                                   owner->resize({200, 80});
                                                 }));
                      }()}};
  owner = &other;
  other.resize({200, 80});
  other.activate(platform);
  render(other, {200, 80});
  click(other, platform, 20, 15);
  render(other, {200, 80});
  NUI_CHECK(!show.get() && chosen.get() == 3 && retired == 1);
  other.dispatch(test::pointer(ui::InputType::PointerUp, 20, 15), platform);
  NUI_CHECK(retired == 1);
}
void row_text_is_centered_and_not_clipped() {
  ui::State<std::optional<int>> selected{std::nullopt};
  auto style = compact_style();
  style.text = ui::Color{1, 1, 1, 1};
  style.surface = ui::Color{0, 0, 0, 1};
  style.rows.base.row_fill = ui::Color{0, 0, 0, 1};
  style.text_size = 14.0f;
  ui::UI tree{ui::Sidebar<int>{selected}.item(1, "NativeUI").style(style)};
  tree.resize({200, 32});
  for (float scale : {1.0f, 2.0f}) {
    ui::HeadlessRenderer renderer{{200, 32}, scale};
    NUI_CHECK(renderer.render(tree));
    int upper{}, lower{};
    for (int y = 0; y < renderer.pixel_height(); ++y)
      for (int x = 4 * static_cast<int>(scale);
           x < 100 * static_cast<int>(scale); ++x) {
        const auto pixel = renderer.pixel(x, y);
        if (pixel.r > 180 && pixel.g > 180 && pixel.b > 180)
          (y < renderer.pixel_height() / 2 ? upper : lower)++;
      }
    NUI_CHECK(upper > 10 && lower > 10);
  }
}
void suite() {
  row_text_is_centered_and_not_clipped();
  sections_root_items_navigation_and_unknown_selection();
  independent_instances_do_not_share_contacts_or_geometry();
  expired_selection_and_open_bindings_are_separate_domains();
  first_observer_fault_recovers_closed_sections_without_navigation();
  terminal_release_disarms_before_callback_copy_and_started_fault_is_not_replayed();
  interactive_accessory_consumes_its_own_action_and_duplicate_domains_are_rejected();
  readonly_typeahead_and_navigation_retirement();
}
} // namespace
int main() { return test::run("widget_sidebar", &suite); }
