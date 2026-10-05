#include "test_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/editable_text.hpp>
namespace {
void drafts_cancel_external_and_stem_selection() {
  ui::State<std::string> value{"Preset.oreto"};
  ui::UI tree{ui::EditableText{"Nom", value}.select_stem()};
  test::MockPlatform platform;
  tree.resize({420, 80});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::F2), platform);
  tree.dispatch(test::text("Edited"), platform);
  NUI_CHECK(value.get() == "Preset.oreto");
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "Edited.oreto");
  tree.dispatch(test::key(ui::Key::F2), platform);
  tree.dispatch(test::text("Draft"), platform);
  value.set("External.txt");
  tree.dispatch(test::key(ui::Key::Escape), platform);
  NUI_CHECK(value.get() == "External.txt");
}
void invalid_and_throwing_validator_recover_and_destroy_is_silent() {
  ui::State<std::string> value{"name"};
  int validations{};
  bool fail{};
  auto make = [&] {
    return ui::EditableText{"Nom", value}
        .validator([&](std::string_view text) -> std::optional<std::string> {
          ++validations;
          if (fail)
            throw std::runtime_error("validator fault");
          if (text.empty())
            return "Nom vide";
          return {};
        })
        .spec();
  };
  test::MockPlatform platform;
  auto tree = std::make_unique<ui::UI>(make());
  tree->resize({420, 80});
  tree->activate(platform);
  tree->dispatch(test::key(ui::Key::F2), platform);
  tree->dispatch(test::text(""), platform);
  ui::InputEvent all;
  all.type = ui::InputType::Command;
  all.command = ui::Command::SelectAll;
  tree->dispatch(all, platform);
  tree->dispatch(test::key(ui::Key::Backspace), platform);
  tree->dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "name" && validations == 1);
  tree->dispatch(test::text("valid"), platform);
  fail = true;
  bool caught{};
  try {
    tree->dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && value.get() == "name");
  fail = false;
  tree->dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(value.get() == "valid");
  tree->dispatch(test::key(ui::Key::F2), platform);
  tree->dispatch(test::text("pending"), platform);
  const auto before = validations;
  tree.reset();
  NUI_CHECK(validations == before && value.get() == "valid");
}
void controller_is_weak_single_mount_and_does_not_begin_synchronously() {
  auto controller = std::make_shared<ui::EditableTextController>();
  ui::State<std::string> value{"one"};
  auto tree = std::make_unique<ui::UI>(
      ui::EditableText{"Nom", value}.controller(controller));
  test::MockPlatform platform;
  tree->resize({420, 80});
  tree->activate(platform);
  controller->begin();
  NUI_CHECK(!controller->editing());
  bool rejected{};
  try {
    ui::UI duplicate{ui::EditableText{"Other", value}.controller(controller)};
  } catch (const std::logic_error &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
  tree.reset();
  controller->begin();
  controller->accept();
  controller->cancel();
  NUI_CHECK(!controller->editing() && value.get() == "one");
}
void rejected_controller_post_retries_only_at_dispatcher_checkpoint() {
  auto controller = std::make_shared<ui::EditableTextController>();
  ui::State<std::string> value{"one"};
  ui::detail::DispatcherOwner owner;
  test::MockPlatform platform;
  platform.dispatcher_value = owner.dispatcher();
  std::string validated;
  ui::UI tree{
      ui::EditableText{"Nom", value}
          .controller(controller)
          .validator([&](std::string_view text) -> std::optional<std::string> {
            validated = text;
            return {};
          })};
  tree.resize({420, 80});
  tree.activate(platform);
  ui::detail::DispatcherTestAccess::fail_next_post(owner.dispatcher());
  controller->begin();
  NUI_CHECK(!controller->editing());
  tree.resize({420, 80});
  NUI_CHECK(!controller->editing());
  (void)owner.checkpoint();
  NUI_CHECK(controller->editing());
  tree.dispatch(test::text("changed"), platform);
  NUI_CHECK(value.get() == "one");
  controller->accept();
  NUI_CHECK(value.get() == "one");
  (void)owner.checkpoint();
  NUI_CHECK(validated == "changed");
  NUI_CHECK(!controller->editing());
  NUI_CHECK(value.get() == "changed");
}
void accepted_controller_requests_are_cancelled_across_activation_owners() {
  ui::State<std::string> value{"one"};
  auto controller = std::make_shared<ui::EditableTextController>();
  int validations{};
  ui::detail::DispatcherOwner first, second;
  test::MockPlatform platform;
  platform.dispatcher_value = first.dispatcher();
  ui::UI tree{
      ui::EditableText{"Nom", value}
          .controller(controller)
          .validator([&](std::string_view) -> std::optional<std::string> {
            ++validations;
            return {};
          })};
  tree.resize({420, 80});
  tree.activate(platform);
  controller->begin();
  NUI_CHECK(!controller->editing());
  tree.deactivate(platform);
  (void)first.checkpoint();
  NUI_CHECK(!controller->editing() && validations == 0 && value.get() == "one");
  platform.dispatcher_value = first.dispatcher();
  tree.activate(platform);
  controller->begin();
  tree.deactivate(platform);
  platform.dispatcher_value = second.dispatcher();
  tree.activate(platform);
  (void)first.checkpoint();
  NUI_CHECK(!controller->editing());
  controller->begin();
  (void)second.checkpoint();
  NUI_CHECK(controller->editing());
  controller->cancel();
  (void)second.checkpoint();
  NUI_CHECK(!controller->editing() && validations == 0 && value.get() == "one");
}
void suite() {
  accepted_controller_requests_are_cancelled_across_activation_owners();
  rejected_controller_post_retries_only_at_dispatcher_checkpoint();
  drafts_cancel_external_and_stem_selection();
  invalid_and_throwing_validator_recover_and_destroy_is_silent();
  controller_is_weak_single_mount_and_does_not_begin_synchronously();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "controller")
    return test::run(
        "editable_text_controller",
        &rejected_controller_post_retries_only_at_dispatcher_checkpoint);
  if (argc == 2 && std::string_view{argv[1]} == "activation")
    return test::run(
        "editable_text_activation",
        &accepted_controller_requests_are_cancelled_across_activation_owners);
  return test::run("widget_editable_text", &suite);
}
