#include "test_support.hpp"
#include <nativeui/editable_text.hpp>
#include <nativeui/find_bar.hpp>
#include <nativeui/number_input.hpp>
#include <nativeui/search_field.hpp>
namespace {
void external_write_during_number_step_invalidation_wins() {
  ui::State<double> value{2};
  ui::UI tree{ui::NumberInput{"Number", value}.range(0, 10)};
  test::MockPlatform platform;
  tree.resize({460, 82});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{460, 82}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false))
      value.set(9);
  });
  tree.dispatch(test::key(ui::Key::Up), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && value.get() == 9);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(value.get() == 8);
}
void external_write_during_search_clear_invalidation_wins() {
  ui::State<std::string> query{"old"};
  ui::UI tree{ui::SearchField{"Search", query}};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{480, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false))
      query.set("external");
  });
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && query.get() == "external");
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(query.get().empty());
}
void external_write_during_editable_accept_invalidation_wins() {
  ui::State<std::string> value{"old"};
  auto controller = std::make_shared<ui::EditableTextController>();
  ui::UI tree{ui::EditableText{"Name", value}.controller(controller)};
  test::MockPlatform platform;
  tree.resize({420, 80});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::F2), platform);
  tree.dispatch(test::text("draft"), platform);
  ui::HeadlessRenderer renderer{{420, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (!controller->editing() && std::exchange(armed, false))
      value.set("external");
  });
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && value.get() == "external");
  tree.dispatch(test::key(ui::Key::F2), platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == "external");
}
void external_current_during_find_invalidation_wins() {
  ui::State<bool> open{true};
  ui::State<std::string> query{"x"};
  ui::State<std::size_t> matches{3};
  ui::State<std::optional<std::size_t>> current{0};
  int calls{};
  ui::UI tree{ui::FindBar{open, query, matches, current}.on_navigate(
      [&](auto) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({800, 80});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{800, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false))
      current.set(2);
  });
  ui::InputEvent event;
  event.type = ui::InputType::Command;
  event.command = ui::Command::FindNext;
  tree.dispatch(event, platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && current.get() == 2 && calls == 0);
  tree.dispatch(event, platform);
  NUI_CHECK(current.get() == 0 && calls == 1);
}
void external_write_during_number_escape_invalidation_wins() {
  ui::State<double> value{2};
  ui::UI tree{ui::NumberInput{"Number", value}.range(0, 100)};
  test::MockPlatform platform;
  tree.resize({460, 82});
  tree.activate(platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("5"), platform);
  NUI_CHECK(value.get() == 5);
  ui::HeadlessRenderer renderer{{460, 82}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false))
      value.set(73);
  });
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && value.get() == 73);
  tree.dispatch(test::key(ui::Key::Down), platform);
  NUI_CHECK(value.get() == 72);
}
void read_only_during_search_clear_blocks_the_late_write() {
  ui::State<std::string> query{"old"};
  ui::State<bool> read_only{false};
  ui::UI tree{ui::ReadOnly{read_only, ui::SearchField{"Search", query}}};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{480, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false)) {
      read_only.set(true);
      tree.resize({480, 80});
    }
  });
  tree.dispatch(test::key(ui::Key::Escape), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && query.get() == "old");
  read_only.set(false);
  tree.resize({480, 80});
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(query.get().empty());
}
void search_submit_is_cancelled_by_an_external_write_during_invalidation() {
  ui::State<std::string> query{"old"};
  int calls{};
  ui::UI tree{ui::SearchField{"Q", query}.on_submit([&](const auto &text) {
    ++calls;
    NUI_CHECK(text == query.get());
  })};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{480, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (std::exchange(armed, false))
      query.set("external");
  });
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!armed && calls == 0 && query.get() == "external");
  auto release = test::key(ui::Key::Enter);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1);
}
struct CopySubmitProbe {
  bool armed{};
  int calls{};
};
struct CopySubmit {
  ui::Binding<std::string> source;
  std::shared_ptr<CopySubmitProbe> probe;
  CopySubmit(ui::Binding<std::string> binding,
             std::shared_ptr<CopySubmitProbe> state)
      : source(std::move(binding)), probe(std::move(state)) {}
  CopySubmit(const CopySubmit &other)
      : source(other.source), probe(other.probe) {
    if (std::exchange(probe->armed, false))
      source.set("external");
  }
  void operator()(const std::string &text) const {
    ++probe->calls;
    NUI_CHECK(text == source.get());
  }
};
void search_submit_is_cancelled_by_an_external_write_during_callback_copy() {
  ui::State<std::string> query{"old"};
  auto probe = std::make_shared<CopySubmitProbe>();
  ui::UI tree{ui::SearchField{"Q", query}.on_submit(
      CopySubmit{query.binding(), probe})};
  test::MockPlatform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  probe->armed = true;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(!probe->armed && probe->calls == 0 && query.get() == "external");
}
void suite() {
  search_submit_is_cancelled_by_an_external_write_during_invalidation();
  search_submit_is_cancelled_by_an_external_write_during_callback_copy();
  external_write_during_number_escape_invalidation_wins();
  read_only_during_search_clear_blocks_the_late_write();
  external_write_during_number_step_invalidation_wins();
  external_write_during_search_clear_invalidation_wins();
  external_write_during_editable_accept_invalidation_wins();
  external_current_during_find_invalidation_wins();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2) {
    const std::string_view mode = argv[1];
    if (mode == "search_submit")
      return test::run(
          "input_transaction_search_submit",
          &search_submit_is_cancelled_by_an_external_write_during_invalidation);
    if (mode == "search_submit_copy")
      return test::run(
          "input_transaction_search_submit_copy",
          &search_submit_is_cancelled_by_an_external_write_during_callback_copy);
    if (mode == "number")
      return test::run("input_transaction_number",
                       &external_write_during_number_step_invalidation_wins);
    if (mode == "search")
      return test::run("input_transaction_search",
                       &external_write_during_search_clear_invalidation_wins);
    if (mode == "editable")
      return test::run(
          "input_transaction_editable",
          &external_write_during_editable_accept_invalidation_wins);
    if (mode == "number_escape")
      return test::run("input_transaction_number_escape",
                       &external_write_during_number_escape_invalidation_wins);
    if (mode == "search_read_only")
      return test::run("input_transaction_search_read_only",
                       &read_only_during_search_clear_blocks_the_late_write);
    if (mode == "find")
      return test::run("input_transaction_find",
                       &external_current_during_find_invalidation_wins);
  }
  return test::run("widget_input_transactions", &suite);
}
