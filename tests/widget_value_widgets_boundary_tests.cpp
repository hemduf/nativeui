#include "test_support.hpp"
#include <nativeui/calendar.hpp>
#include <nativeui/date_input.hpp>
#include <nativeui/time_input.hpp>
#include <nativeui/token_field.hpp>

namespace {
auto day(unsigned date) {
  return std::chrono::sys_days{std::chrono::year{2026} / 10 /
                               std::chrono::day{date}};
}

class ReleasePlatform final : public ui::PlatformServices {
public:
  void set_clipboard_text(std::string_view) override {}
  void request_clipboard_text() override {}
  void begin_pointer_capture() noexcept override { ++captures; }
  void end_pointer_capture() noexcept override {
    ++releases;
    auto callback = std::move(on_release);
    if (callback) {
      try {
        callback();
      } catch (...) {
        failed = true;
      }
    }
  }
  std::function<void()> on_release;
  int captures{}, releases{};
  bool failed{};
};

void calendar_native_release_preserves_the_external_cursor() {
  ui::State<ui::Calendar::Value> value{day(4)};
  int changes{};
  ui::UI tree{ui::Calendar{"Day", value}.on_change([&](auto) { ++changes; })};
  ReleasePlatform platform;
  tree.resize({340, 320});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 30, 125), platform);
  NUI_CHECK(platform.captures == 1);
  platform.on_release = [&] { value.set(day(10)); };
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(platform.releases == 1 && !platform.failed);
  NUI_CHECK(value.get() == day(10) && changes == 0);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == day(10) && changes == 0);
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == day(11) && changes == 1);
}

ui::NodeId text_editor(const ui::UI &tree) {
  for (ui::NodeId id = 1; id < 128; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->role == ui::SemanticRole::TextInput &&
        info->name == "Tags")
      return id;
  }
  throw test::Failure("TokenField editor missing");
}

void token_overflow_draft_has_one_separator_between_segments() {
  ui::State<std::vector<std::string>> tokens{std::vector<std::string>{}};
  int publications{};
  auto observer = tokens.observe([&](const auto &) { ++publications; });
  ui::UI tree{ui::TokenField{"Tags", tokens}.maximum_tokens(1)};
  test::MockPlatform platform;
  tree.resize({360, 160});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{360, 160}, 1};
  NUI_CHECK(renderer.render(tree));
  const auto editor = text_editor(tree);
  tree.dispatch(test::text("a,b,c,tail"), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>{"a"} && publications == 1);
  auto info = tree.component_semantics(editor);
  NUI_CHECK(info && info->text_value == "b,c,tail");
  tokens.set({});
  NUI_CHECK(renderer.render(tree));
  info = tree.component_semantics(editor);
  NUI_CHECK(info && info->text_value == "b,c,tail");
  tree.dispatch(test::text(","), platform);
  NUI_CHECK(tokens.get() == std::vector<std::string>{"b"});
  info = tree.component_semantics(editor);
  NUI_CHECK(info && info->text_value == "c,tail");
}

void time_focus_request_does_not_publish_an_old_segment_snapshot() {
  using V = ui::TimeInput::Value;
  const auto external = std::chrono::seconds{7 * 3600 + 8 * 60 + 9};
  ui::State<V> value{std::chrono::seconds{12 * 3600 + 34 * 60 + 9}};
  int changes{};
  ui::UI tree{ui::TimeInput{"Time", value}.on_change([&](auto) { ++changes; })};
  test::MockPlatform platform;
  tree.resize({300, 80});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{300, 80}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed = true;
  tree.set_invalidation_callback([&] {
    if (armed) {
      armed = false;
      value.set(external);
    }
  });
  tree.dispatch(test::text("3"), platform);
  NUI_CHECK(!armed && value.get() == external && changes == 0);
  tree.dispatch(test::key(ui::Key::Up), platform);
  NUI_CHECK(value.get() == external + std::chrono::seconds{60} && changes == 1);
}

void date_external_update_replaces_the_open_calendar_draft() {
  ui::State<ui::DateInput::Value> value{day(4)};
  int changes{};
  ui::UI tree{ui::DateInput{"Day", value}.on_change([&](auto) { ++changes; })};
  test::MockPlatform platform;
  tree.resize({500, 450});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  auto release = test::key(ui::Key::Enter);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Right), platform);
  value.set(day(20));
  ui::HeadlessRenderer renderer{{500, 450}, 1};
  NUI_CHECK(renderer.render(tree) && changes == 0);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Right), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == day(21) && changes == 1);
  NUI_CHECK(tree.overlay_entries().empty());
}

void suite() {
  calendar_native_release_preserves_the_external_cursor();
  token_overflow_draft_has_one_separator_between_segments();
  time_focus_request_does_not_publish_an_old_segment_snapshot();
  date_external_update_replaces_the_open_calendar_draft();
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "calendar_release")
    return test::run("calendar_release",
                     &calendar_native_release_preserves_the_external_cursor);
  if (argc == 2 && std::string_view{argv[1]} == "token_overflow")
    return test::run("token_overflow",
                     &token_overflow_draft_has_one_separator_between_segments);
  if (argc == 2 && std::string_view{argv[1]} == "time_focus")
    return test::run(
        "time_focus",
        &time_focus_request_does_not_publish_an_old_segment_snapshot);
  if (argc == 2 && std::string_view{argv[1]} == "date_external")
    return test::run("date_external",
                     &date_external_update_replaces_the_open_calendar_draft);
  return test::run("widget_value_widgets_boundary", &suite);
}
