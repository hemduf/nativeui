#include "test_support.hpp"

#include <nativeui/calendar.hpp>
#include <nativeui/date_input.hpp>
#include <nativeui/editable_combo_box.hpp>
#include <nativeui/find_bar.hpp>
#include <nativeui/time_input.hpp>
#include <nativeui/token_field.hpp>

#include <optional>

namespace ui {
struct TreeTestAccess {
  struct Action {
    Rect bounds;
    bool enabled;
  };
  static std::optional<Action> action(Tree &tree, std::string_view name) {
    const auto visit = [&](const auto &self, const Node &node)
        -> std::optional<Action> {
      if (node.component->semantics().name == name)
        return Action{node.bounds, node.component->effective_enabled()};
      for (const auto &child : node.children)
        if (auto found = self(self, *child))
          return found;
      return {};
    };
    return visit(visit, *tree.root_);
  }
};
} // namespace ui

namespace {
void initial_composite_actions_are_enabled() {
  std::string failures;
  const auto check = [&](ui::Spec spec, std::string_view name) {
    ui::Tree tree{ui::compile(std::move(spec))};
    tree.mount();
    tree.layout({480, 400});
    test::MockPlatform platform;
    tree.activate_focus(platform);
    const auto action = ui::TreeTestAccess::action(tree, name);
    if (!action || !action->enabled || action->bounds.empty()) {
      if (!failures.empty())
        failures += ", ";
      failures += name;
    }
    tree.deactivate_focus(platform);
  };

  ui::State<ui::DateInput::Value> date{
      std::chrono::sys_days{std::chrono::year{2026} / 10 / 5}};
  check(ui::DateInput{"Date", date}.spec(), "Clear date");
  ui::State<ui::TimeInput::Value> time{std::chrono::seconds{9 * 3600}};
  check(ui::TimeInput{"Time", time}.spec(), "Clear time");
  ui::State<std::vector<std::string>> tokens{
      std::vector<std::string>{"NativeUI"}};
  check(ui::TokenField{"Tags", tokens}.spec(), "Remove NativeUI");
  ui::State<std::string> choice{"Inter"};
  check(ui::EditableComboBox{"Font", choice,
                             std::vector<std::string>{"Inter", "Menlo"}}
            .spec(),
        "Options");
  ui::State<bool> open{true};
  ui::State<std::string> query{"NativeUI"};
  ui::State<std::size_t> matches{2};
  ui::State<std::optional<std::size_t>> current{std::size_t{0}};
  check(ui::FindBar{open, query, matches, current}.spec(), "Next");
  check(ui::FindBar{open, query, matches, current}.spec(), "Close");
  ui::State<ui::Calendar::Value> calendar{date.get()};
  check(ui::Calendar{"Calendar", calendar}.spec(), "Next month");

  if (!failures.empty())
    throw test::Failure("Initially disabled composite actions: " + failures);
}
void initial_composite_actions_respond_to_pointer() {
  const auto click = [](auto make_spec, std::string_view name) {
    ui::Rect bounds;
    {
      ui::Tree tree{ui::compile(make_spec())};
      tree.mount();
      tree.layout({480, 400});
      const auto action = ui::TreeTestAccess::action(tree, name);
      NUI_CHECK(action && action->enabled && !action->bounds.empty());
      bounds = action->bounds;
    }
    ui::UI tree{make_spec()};
    tree.resize({480, 400});
    test::MockPlatform platform;
    tree.activate(platform);
    const float x = bounds.x + bounds.w * 0.5f;
    const float y = bounds.y + bounds.h * 0.5f;
    const auto down =
        tree.dispatch(test::pointer(ui::InputType::PointerDown, x, y), platform);
    const auto up =
        tree.dispatch(test::pointer(ui::InputType::PointerUp, x, y), platform);
    NUI_CHECK(ui::handled(down) || ui::handled(up));
    tree.deactivate(platform);
  };

  ui::State<ui::DateInput::Value> date{
      std::chrono::sys_days{std::chrono::year{2026} / 10 / 5}};
  click([&] { return ui::DateInput{"Date", date}.spec(); }, "Clear date");
  NUI_CHECK(!date.get());
  ui::State<ui::TimeInput::Value> time{std::chrono::seconds{9 * 3600}};
  click([&] { return ui::TimeInput{"Time", time}.spec(); }, "Clear time");
  NUI_CHECK(!time.get());
  ui::State<std::vector<std::string>> tokens{
      std::vector<std::string>{"NativeUI"}};
  click([&] { return ui::TokenField{"Tags", tokens}.spec(); },
        "Remove NativeUI");
  NUI_CHECK(tokens.get().empty());
  ui::State<std::string> choice{"Inter"};
  click([&] { return ui::EditableComboBox{"Font", choice,
                                          std::vector<std::string>{"Inter", "Menlo"}}
                       .spec(); },
        "Options");
  ui::State<ui::Calendar::Value> calendar{
      std::chrono::sys_days{std::chrono::year{2026} / 10 / 5}};
  const auto original = calendar.get();
  click([&] { return ui::Calendar{"Calendar", calendar}.spec(); },
        "Next month");
  NUI_CHECK(calendar.get() != original);
}
} // namespace

int main() {
  return test::run("widget_composite_initial_actions", [] {
    initial_composite_actions_are_enabled();
    initial_composite_actions_respond_to_pointer();
  });
}
