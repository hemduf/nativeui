#include "test_support.hpp"

#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/text_input.hpp>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {

struct SourceRecord {
  ui::NodeId anchor{ui::kInvalidNodeId};
  ui::OverlayHandle handle;
  std::optional<ui::detail::OverlayComponentCommand> pending;
  bool open_on_change{};
  int queued{}, taken{}, shown{}, invalid_shown{}, closed{}, bubbled{};
};

void queue_show(const std::shared_ptr<SourceRecord>& record) {
  ui::OverlaySpec popup;
  popup.anchor = record->anchor;
  popup.mode = ui::OverlayMode::NonModal;
  popup.pointer_policy = ui::OverlayPointerPolicy::Ignore;
  // Keep focus on the editor and let Escape reach ordinary retained routing.
  popup.dismiss_on_escape = false;
  popup.content = ui::Label{"Suggestion"}.spec();
  const std::weak_ptr<SourceRecord> weak = record;
  record->pending = ui::detail::OverlayComponentCommand::show(
      std::move(popup), [weak](ui::OverlayHandle handle) {
        auto record = weak.lock();
        if (!record) return;
        if (!handle.valid()) { ++record->invalid_shown; return; }
        record->handle = std::move(handle);
        ++record->shown;
      });
  ++record->queued;
}

class Source final : public ui::Component,
                     public ui::detail::OverlayCommandSource {
public:
  Source(std::shared_ptr<SourceRecord> record, ui::Binding<std::string> value)
      : record_(std::move(record)), value_(std::move(value)) {}
  void mount(ui::MountContext& context) override {
    record_->anchor = context.node_id();
    const std::weak_ptr<SourceRecord> weak = record_;
    subscription_ = value_.observe([weak](const std::string&) {
      auto record = weak.lock();
      if (record && std::exchange(record->open_on_change, false)) queue_show(record);
    });
  }
  void unmount(ui::LifecycleContext&) override { subscription_ = {}; }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    for (auto& child : placements) child.bounds = bounds;
  }
  ui::EventResult input(const ui::InputEvent&, ui::InputContext&) override {
    ++record_->bubbled;
    return ui::EventResult::Ignored;
  }
  void paint(ui::PaintContext&) const override {}
  std::optional<ui::detail::OverlayComponentCommand> take_overlay_command() override {
    ++record_->taken;
    return std::exchange(record_->pending, std::nullopt);
  }

private:
  std::shared_ptr<SourceRecord> record_;
  ui::Binding<std::string> value_;
  ui::Binding<std::string>::Subscription subscription_;
};

ui::Spec source(const std::shared_ptr<SourceRecord>& record,
                ui::Binding<std::string> value, ui::Spec child) {
  return {[record, value] { return std::make_unique<Source>(record, value); },
          {std::move(child)}};
}

enum class Trigger { Text, Composition, Command };

void dispatch_open(ui::UI& tree, test::MockPlatform& platform,
                   const std::shared_ptr<SourceRecord>& nearest, Trigger trigger) {
  if (trigger == Trigger::Command) {
    // Populate actual TextInput undo history before observing the opening edit.
    tree.dispatch(test::text("seed"), platform);
  } else if (trigger == Trigger::Composition) {
    ui::InputEvent start;
    start.type = ui::InputType::Composition;
    start.composition.type = ui::CompositionType::Start;
    tree.dispatch(start, platform);
  }
  nearest->open_on_change = true;
  if (trigger == Trigger::Text) {
    tree.dispatch(test::text("x"), platform);
  } else if (trigger == Trigger::Composition) {
    ui::InputEvent commit;
    commit.type = ui::InputType::Composition;
    commit.composition.type = ui::CompositionType::Commit;
    commit.composition.text = "composition";
    tree.dispatch(commit, platform);
  } else {
    ui::InputEvent undo;
    undo.type = ui::InputType::Command;
    undo.command = ui::Command::Undo;
    tree.dispatch(undo, platform);
  }
}

void ancestor_source_drains_after_consumed_editor_events(Trigger trigger,
                                                         ui::Key close_key) {
  ui::State<std::string> value{"base"};
  auto outer = std::make_shared<SourceRecord>();
  auto nearest = std::make_shared<SourceRecord>();
  auto editor = ui::TextInput{"Editor", value}.spec();
  auto inner = source(nearest, value.binding(), std::move(editor));
  ui::UI tree{source(outer, value.binding(), std::move(inner))};
  test::MockPlatform platform;
  tree.resize({320.0f, 180.0f});
  tree.activate(platform);

  // An outer pending command must remain untouched while the nearer command
  // source owns the focused descendant, even if a handled input never bubbles.
  queue_show(outer);
  const auto focus = tree.component_semantics(nearest->anchor + 1);
  NUI_CHECK(focus && focus->role == ui::SemanticRole::TextInput && focus->focused);
  dispatch_open(tree, platform, nearest, trigger);
  NUI_CHECK(nearest->queued == 1);
  NUI_CHECK(nearest->shown == 1 && nearest->invalid_shown == 0);
  NUI_CHECK(nearest->handle.valid() && tree.overlay_entries().size() == 1);
  NUI_CHECK(outer->taken == 0 && outer->pending && nearest->bubbled == 0);

  // A composite may prepare its controller command independently of the leaf
  // input implementation. Up and Escape are consumed by the real TextInput;
  // command discovery must still find this closest parent at the safe point.
  const std::weak_ptr<SourceRecord> weak_nearest = nearest;
  nearest->pending = ui::detail::OverlayComponentCommand::close_then_invoke(
      nearest->handle, nearest->anchor, false,
      [weak_nearest] { if (auto live = weak_nearest.lock()) ++live->closed; });
  tree.dispatch(test::key(close_key), platform);
  NUI_CHECK(nearest->closed == 1 && !nearest->handle.valid());
  NUI_CHECK(tree.overlay_entries().empty() && !nearest->pending);
  NUI_CHECK(outer->taken == 0 && outer->pending);
  tree.dispatch(test::key(close_key), platform);
  NUI_CHECK(nearest->closed == 1 && tree.overlay_entries().empty());
}

void text_then_up() {
  ancestor_source_drains_after_consumed_editor_events(Trigger::Text, ui::Key::Up);
}
void composition_then_escape() {
  ancestor_source_drains_after_consumed_editor_events(Trigger::Composition, ui::Key::Escape);
}
void command_then_up() {
  ancestor_source_drains_after_consumed_editor_events(Trigger::Command, ui::Key::Up);
}
void suite() { text_then_up(); composition_then_escape(); command_then_up(); }

} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "text") return test::run("overlay_ancestor_text", &text_then_up);
  if (mode == "composition") return test::run("overlay_ancestor_composition", &composition_then_escape);
  if (mode == "command") return test::run("overlay_ancestor_command", &command_then_up);
  return test::run("overlay_ancestor_commands", &suite);
}
