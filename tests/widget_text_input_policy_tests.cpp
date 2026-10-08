#include "../src/detail/widget_text_input_policy.hpp"
#include "test_support.hpp"

#include <stdexcept>

namespace {
struct Record {
  int edits{};
  std::vector<std::weak_ptr<ui::detail::TextInputSession>> sessions;
  int blur_calls{};
};
ui::Spec editor(ui::Binding<std::string> value,
                std::shared_ptr<Record> record) {
  return {[value = std::move(value), record = std::move(record)] {
            auto component = std::make_unique<ui::TextInputComponent>(
                "Search", value, "", 0,
                ui::TextInputComponent::SubmitCallback{}, ui::TextInputStyle{});
            auto policy = std::make_shared<ui::detail::TextInputPolicy>();
            auto connection =
                std::make_shared<std::weak_ptr<ui::detail::TextInputSession>>();
            policy->before_input =
                [connection,
                 value](const ui::InputEvent &event, ui::InputContext &context,
                        const ui::detail::TextInputSnapshot &snapshot) mutable
                -> std::optional<ui::EventResult> {
              if (event.type != ui::InputType::KeyDown ||
                  event.key != ui::Key::Escape)
                return {};
              const auto current = connection->lock();
              if (!current || !current->mounted())
                return ui::EventResult::Ignored;
              current->cancel_capture();
              if (snapshot.composition_active) {
                current->cancel_composition();
                context.invalidate();
                return ui::EventResult::Handled;
              }
              current->replace("");
              context.invalidate();
              value.set("");
              return ui::EventResult::Handled;
            };
            policy->committed_edit = [record](ui::detail::TextInputSnapshot) {
              ++record->edits;
            };
            policy->focus_changed =
                [record](bool focused, ui::detail::TextInputSnapshot snapshot) {
                  if (!focused) {
                    ++record->blur_calls;
                    NUI_CHECK(!snapshot.composition_active &&
                              !snapshot.focused);
                  }
                };
            *connection =
                ui::detail::TextInputAccess::configure(*component, policy);
            record->sessions.push_back(*connection);
            return component;
          },
          {}};
}
void isolated_sessions_and_stale_operations_are_safe() {
  ui::State<std::string> value{"one"};
  auto record = std::make_shared<Record>();
  const auto spec = editor(value.binding(), record);
  auto first = std::make_unique<ui::UI>(ui::Spec{spec});
  ui::UI second{ui::Spec{spec}};
  test::MockPlatform platform;
  first->resize({420, 82});
  first->activate(platform);
  second.resize({420, 82});
  NUI_CHECK(record->sessions.size() == 2);
  const auto a = record->sessions[0].lock(), b = record->sessions[1].lock();
  NUI_CHECK(a && b && a != b && a->mounted() && b->mounted());
  a->replace("draft", std::pair<std::size_t, std::size_t>{0, 2});
  NUI_CHECK(value.get() == "one" && a->snapshot().text == "draft");
  NUI_CHECK(a->snapshot().anchor == 0 && a->snapshot().cursor == 2);
  NUI_CHECK(b->snapshot().text == "one");
  first.reset();
  NUI_CHECK(!a->mounted());
  a->replace("stale");
  a->select(0, 99);
  a->cancel_composition();
  a->cancel_capture();
  a->reset_baseline();
  a->request_focus();
  NUI_CHECK(value.get() == "one" && b->snapshot().text == "one");
}
void composition_escape_then_clear_throw_and_recover() {
  ui::State<std::string> value{"query"};
  auto record = std::make_shared<Record>();
  ui::UI tree{editor(value.binding(), record)};
  test::MockPlatform platform;
  tree.resize({420, 82});
  tree.activate(platform);
  ui::InputEvent composition;
  composition.type = ui::InputType::Composition;
  composition.composition.type = ui::CompositionType::Start;
  tree.dispatch(composition, platform);
  composition.composition.type = ui::CompositionType::Update;
  composition.composition.text = "marked";
  tree.dispatch(composition, platform);
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == "query");
  NUI_CHECK(!record->sessions[0].lock()->snapshot().composition_active);
  bool fail = true;
  auto subscription = value.observe([&](const std::string &) {
    if (fail)
      throw std::runtime_error("injected policy publication");
  });
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Escape), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get().empty());
  fail = false;
  tree.dispatch(test::text("next"), platform);
  NUI_CHECK(value.get() == "next");
}
void refresh_recovers_a_skipped_observer_without_publishing() {
  auto value = std::make_unique<ui::State<std::string>>("old long source");
  bool fail = true;
  auto observer = value->observe([&](const auto &) {
    if (fail)
      throw std::runtime_error("observer before editor");
  });
  auto record = std::make_shared<Record>();
  ui::UI tree{editor(value->binding(), record)};
  test::MockPlatform platform;
  tree.resize({420, 82});
  tree.activate(platform);
  const auto session = record->sessions[0].lock();
  session->select(0, 99);
  bool caught{};
  try {
    value->set("x");
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && session->snapshot().text == "old long source");
  fail = false;
  session->refresh_source();
  NUI_CHECK(session->snapshot().text == "x");
  NUI_CHECK(session->snapshot().cursor <= 1 && session->snapshot().anchor <= 1);
  value.reset();
  session->refresh_source();
  NUI_CHECK(session->snapshot().text == "x" && session->snapshot().read_only);
}
void committed_edits_have_durable_generations_even_after_a_source_exception() {
  ui::State<std::string> value{"same"};
  bool fail = true;
  auto observer = value.observe([&](const auto &) {
    if (fail)
      throw std::runtime_error("draft observer before policy");
  });
  auto record = std::make_shared<Record>();
  ui::UI tree{editor(value.binding(), record)};
  test::MockPlatform platform;
  tree.resize({420, 82});
  tree.activate(platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree.dispatch(all, platform);
  tree.dispatch(test::text("same"), platform);
  auto session = record->sessions[0].lock();
  NUI_CHECK(record->edits == 1 && session->snapshot().edit_generation == 1);
  tree.dispatch(all, platform);
  bool caught{};
  try {
    tree.dispatch(test::text("different"), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == "different" && record->edits == 1 &&
            session->snapshot().edit_generation == 2);
  fail = false;
  session->refresh_source();
  NUI_CHECK(session->snapshot().edit_generation == 2);
  tree.dispatch(test::text("!"), platform);
  NUI_CHECK(record->edits == 2 && session->snapshot().edit_generation == 3);
}
void suite() {
  committed_edits_have_durable_generations_even_after_a_source_exception();
  refresh_recovers_a_skipped_observer_without_publishing();
  isolated_sessions_and_stale_operations_are_safe();
  composition_escape_then_clear_throw_and_recover();
}
} // namespace
int main() { return test::run("widget_text_input_policy", &suite); }
