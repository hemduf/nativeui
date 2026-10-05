#include "test_support.hpp"

#include <nativeui/detail/overlay_commands.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/switch.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Real Core routing/overlay oracle. These fixtures use the actual private
// component-to-UI protocol; there is no replacement UI/Tree or popup engine.
struct LeafRecord {
  ui::NodeId id{ui::kInvalidNodeId};
  int context_events{}, key_events{}, focus_in{}, focus_out{};
  bool focused{};
  ui::Point last_position{};
  ui::Rect focus_bounds{};
};

class Leaf final : public ui::Component {
public:
  explicit Leaf(std::shared_ptr<LeafRecord> record) : record_(std::move(record)) {}
  bool focusable() const noexcept override { return true; }
  void mount(ui::MountContext& context) override { record_->id = context.node_id(); }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80, 40}; }
  void focus_changed(bool focused, ui::FocusContext& context) override {
    record_->focused = focused;
    record_->focus_bounds = context.bounds();
    focused ? ++record_->focus_in : ++record_->focus_out;
  }
  ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
    if (event.type == ui::InputType::ContextMenu) {
      ++record_->context_events;
      record_->last_position = event.position;
    }
    if (event.type == ui::InputType::KeyDown) ++record_->key_events;
    return ui::EventResult::Ignored;
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::shared_ptr<LeafRecord> record_;
};

ui::Spec leaf(const std::shared_ptr<LeafRecord>& record) {
  return {[record] { return std::make_unique<Leaf>(record); }, {}};
}

class PopupBody final : public ui::Component {
public:
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {24, 12}; }
  void paint(ui::PaintContext&) const override {}
};

struct SourceRecord {
  ui::NodeId id{ui::kInvalidNodeId};
  bool mounted{}, accept{true}, session_valid{true}, open_on_down{};
  int context_events{}, taken{}, shown{}, invalid_shown{}, unmounted{}, down_events{};
  std::optional<ui::Rect> requested_bounds;
  std::optional<ui::Rect> fixed_bounds;
  std::optional<ui::detail::OverlayComponentCommand> pending;
  ui::OverlayHandle handle;
  std::function<void()> on_request;
};

void queue_show(const std::shared_ptr<SourceRecord>& record) {
  ui::OverlaySpec popup;
  popup.anchor = record->id;
  popup.mode = ui::OverlayMode::NonModal;
  popup.pointer_policy = ui::OverlayPointerPolicy::Ignore;
  popup.placement = ui::OverlayPlacement::AnchorBelow;
  popup.content = {[] { return std::make_unique<PopupBody>(); }, {}};
  const std::weak_ptr<SourceRecord> weak = record;
  record->pending = ui::detail::OverlayComponentCommand::show(
      std::move(popup), [weak](ui::OverlayHandle handle) {
        if (auto live = weak.lock()) {
          if (!handle.valid()) { ++live->invalid_shown; return; }
          ++live->shown;
          live->handle = std::move(handle);
        }
      });
}

class Source final : public ui::Component,
                     public ui::detail::OverlayCommandSource,
                     public ui::detail::OverlayAnchorPolicy {
public:
  explicit Source(std::shared_ptr<SourceRecord> record) : record_(std::move(record)) {}
  bool pointer_targetable() const noexcept override { return true; }
  void mount(ui::MountContext& context) override {
    record_->id = context.node_id();
    record_->mounted = true;
  }
  void unmount(ui::LifecycleContext&) override {
    record_->mounted = false;
    ++record_->unmounted;
    // Keep pending in the externally owned record so an accidental drain of a
    // retired source is observable. Removal must be protected by retained ID.
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    for (auto& placement : placements) placement.bounds = bounds;
  }
  ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
    if (event.type == ui::InputType::KeyDown && event.key == ui::Key::Down && record_->open_on_down) {
      auto record = record_;
      ++record->down_events;
      record->requested_bounds = record->fixed_bounds.value_or(context.bounds());
      queue_show(record);
      return ui::EventResult::Handled;
    }
    if (event.type != ui::InputType::ContextMenu) return ui::EventResult::Ignored;
    auto record = record_;
    ++record->context_events;
    if (!record->accept) return ui::EventResult::Ignored;
    record->requested_bounds = record->fixed_bounds.value_or(
        ui::Rect{event.position.x, event.position.y, 0, 0});
    queue_show(record);
    auto callback = record->on_request;
    if (callback) callback();
    return ui::EventResult::Handled;
  }
  std::optional<ui::detail::OverlayComponentCommand> take_overlay_command() override {
    ++record_->taken;
    return std::exchange(record_->pending, std::nullopt);
  }
  bool overlay_session_valid() const noexcept override {
    return record_->mounted && record_->session_valid;
  }
  // Core19 additive seams are present: keep explicit overrides under the
  // project's strict compiler warnings.
  bool has_pending_overlay_command() const noexcept override {
    return record_->pending.has_value();
  }
  std::optional<ui::Rect> overlay_anchor_bounds() const noexcept override {
    return record_->requested_bounds;
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::shared_ptr<SourceRecord> record_;
};

ui::Spec source(const std::shared_ptr<SourceRecord>& record, ui::Spec child) {
  return {[record] { return std::make_unique<Source>(record); }, {std::move(child)}};
}

struct LegacyRecord {
  int context_events{}, taken{};
  bool inherited_pending{};
};

class LegacySource final : public ui::Component,
                           public ui::detail::OverlayCommandSource {
public:
  explicit LegacySource(std::shared_ptr<LegacyRecord> record) : record_(std::move(record)) {}
  bool pointer_targetable() const noexcept override { return true; }
  void mount(ui::MountContext&) override {
    record_->inherited_pending = has_pending_overlay_command();
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    for (auto& placement : placements) placement.bounds = bounds;
  }
  ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
    if (event.type == ui::InputType::ContextMenu) ++record_->context_events;
    return ui::EventResult::Ignored;
  }
  std::optional<ui::detail::OverlayComponentCommand> take_overlay_command() override {
    ++record_->taken;
    return std::nullopt;
  }
  // Intentionally INHERIT has_pending_overlay_command(): the compatibility
  // default is true even when a historical source has nothing to take.
  void paint(ui::PaintContext&) const override {}
private:
  std::shared_ptr<LegacyRecord> record_;
};

ui::Spec legacy_source(const std::shared_ptr<LegacyRecord>& record, ui::Spec child) {
  return {[record] { return std::make_unique<LegacySource>(record); }, {std::move(child)}};
}

class Columns final : public ui::Component {
public:
  explicit Columns(std::optional<ui::Binding<float>> first_width = std::nullopt)
      : first_width_(std::move(first_width)) {}
  void mount(ui::MountContext& context) override {
    if (first_width_) {
      auto invalidate = context.layout_invalidator();
      subscription_ = first_width_->observe([invalidate](float) { invalidate(); });
    }
  }
  void unmount(ui::LifecycleContext&) override { subscription_ = {}; }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {320, 80}; }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    const float first = first_width_ ? first_width_->get() : bounds.w * 0.5f;
    if (!placements.empty()) placements[0].bounds = {bounds.x, bounds.y, first, 80};
    if (placements.size() > 1) placements[1].bounds = {bounds.x + first, bounds.y, bounds.w - first, 80};
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::optional<ui::Binding<float>> first_width_;
  ui::Binding<float>::Subscription subscription_;
};

ui::Spec columns(ui::Spec left, ui::Spec right) {
  return {[] { return std::make_unique<Columns>(); }, {std::move(left), std::move(right)}};
}

void check_single_overlay(const ui::UI& tree, const std::shared_ptr<SourceRecord>& owner,
                          ui::Rect expected) {
  const auto entries = tree.overlay_entries();
  NUI_CHECK(entries.size() == 1);
  NUI_CHECK(entries.front().resolved && entries.front().anchor == owner->id);
  NUI_CHECK(owner->shown == 1 && owner->invalid_shown == 0 && owner->handle.valid());
  NUI_CHECK_NEAR(entries.front().bounds.x, expected.x, 0.001f);
  NUI_CHECK_NEAR(entries.front().bounds.y, expected.y, 0.001f);
  NUI_CHECK_NEAR(entries.front().bounds.w, expected.w, 0.001f);
  NUI_CHECK_NEAR(entries.front().bounds.h, expected.h, 0.001f);
}

void hit_target_differs_from_focus_and_opens_at_request_point() {
  auto left_leaf = std::make_shared<LeafRecord>(), right_leaf = std::make_shared<LeafRecord>();
  auto focused_source = std::make_shared<SourceRecord>(), requested = std::make_shared<SourceRecord>();
  ui::UI tree{columns(source(focused_source, leaf(left_leaf)), source(requested, leaf(right_leaf)))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  NUI_CHECK(left_leaf->focused && !right_leaf->focused);
  focused_source->requested_bounds = ui::Rect{10, 10, 0, 0};
  queue_show(focused_source);

  NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(right_leaf->context_events == 1 && left_leaf->context_events == 0);
  NUI_CHECK(requested->taken == 1 && requested->context_events == 1);
  NUI_CHECK(focused_source->taken == 0 && focused_source->pending && focused_source->shown == 0);
  NUI_CHECK(left_leaf->focused && !right_leaf->focused && left_leaf->focus_out == 0);
  NUI_CHECK(platform.pointer_capture_begin_count == 0 && platform.pointer_capture_end_count == 0);
  check_single_overlay(tree, requested, {230, 30, 24, 12});
}

void source_path_uses_layout_prepared_for_this_input() {
  ui::State<float> first_width{160};
  auto left_leaf = std::make_shared<LeafRecord>(), right_leaf = std::make_shared<LeafRecord>();
  auto old_hit = std::make_shared<SourceRecord>(), current_hit = std::make_shared<SourceRecord>();
  ui::Spec stage{
      [binding = first_width.binding()] { return std::make_unique<Columns>(binding); },
      {source(old_hit, leaf(left_leaf)), source(current_hit, leaf(right_leaf))}};
  ui::UI tree{std::move(stage)}; test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  old_hit->requested_bounds = ui::Rect{5, 5, 0, 0}; queue_show(old_hit);
  first_width.set(80);
  NUI_CHECK(tree.layout_dirty());
  // No resize/paint between the model change and this request: x120 belongs to
  // the right cell only after authoritative input layout has been prepared.
  NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 120, 30), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(right_leaf->context_events == 1 && left_leaf->context_events == 0);
  NUI_CHECK(current_hit->context_events == 1 && current_hit->taken == 1);
  NUI_CHECK(old_hit->taken == 0 && old_hit->pending && old_hit->shown == 0);
  NUI_CHECK(left_leaf->focused && !right_leaf->focused);
  check_single_overlay(tree, current_hit, {120, 30, 24, 12});
}

void owned_logical_rect_is_used_and_refreshed() {
  auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
  auto record = std::make_shared<SourceRecord>();
  record->fixed_bounds = ui::Rect{195, 24, 17, 9};
  ui::UI tree{columns(leaf(left), source(record, leaf(right)))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  check_single_overlay(tree, record, {195, 33, 24, 12});

  // Geometry comes from owned policy data on every anchor synchronization,
  // rather than from the wrapper's 160x80 layout box or a stale copied point.
  record->requested_bounds = ui::Rect{201, 28, 17, 9};
  tree.resize({340, 160});
  check_single_overlay(tree, record, {201, 37, 24, 12});
  NUI_CHECK(record->taken == 1 && right->context_events == 1);
}

void declining_inner_source_allows_pending_parent() {
  auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
  auto inner = std::make_shared<SourceRecord>(), outer = std::make_shared<SourceRecord>();
  inner->accept = false;
  ui::UI tree{columns(leaf(left), source(outer, source(inner, leaf(right))))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(inner->context_events == 1 && outer->context_events == 1);
  NUI_CHECK(inner->taken == 0 && !inner->pending);
  NUI_CHECK(outer->taken == 1 && outer->shown == 1);
  check_single_overlay(tree, outer, {230, 30, 24, 12});
}

void accepting_inner_source_does_not_drain_parent() {
  auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
  auto inner = std::make_shared<SourceRecord>(), outer = std::make_shared<SourceRecord>();
  ui::UI tree{columns(leaf(left), source(outer, source(inner, leaf(right))))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  outer->requested_bounds = ui::Rect{5, 5, 0, 0}; queue_show(outer);
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  NUI_CHECK(inner->context_events == 1 && outer->context_events == 0);
  NUI_CHECK(inner->taken == 1 && outer->taken == 0 && outer->pending);
  check_single_overlay(tree, inner, {230, 30, 24, 12});
}

void callback_retires_anchor_and_same_key_replacement_needs_fresh_request() {
  ui::State<int> branch{1};
  auto left = std::make_shared<LeafRecord>(), old_leaf = std::make_shared<LeafRecord>();
  auto replacement_leaf = std::make_shared<LeafRecord>();
  auto old = std::make_shared<SourceRecord>(), replacement = std::make_shared<SourceRecord>();
  auto dynamic = ui::Switch{branch}
      .when(1, ui::keyed("same-anchor", source(old, leaf(old_leaf))))
      .when(2, ui::keyed("same-anchor", source(replacement, leaf(replacement_leaf))));
  ui::UI tree{columns(leaf(left), ui::make_spec(std::move(dynamic)))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  const ui::NodeId retired_id = old->id;
  old->on_request = [&] { branch.set(2); };
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  NUI_CHECK(old->context_events == 1 && old->unmounted == 1 && old->pending);
  NUI_CHECK(!tree.component_availability(retired_id));
  NUI_CHECK(replacement->mounted && replacement->id != retired_id);
  NUI_CHECK(old->taken == 0 && old->shown == 0 && replacement->taken == 0 && replacement->shown == 0);
  NUI_CHECK(tree.overlay_entries().empty());
  tree.resize({320, 160});
  NUI_CHECK(tree.overlay_entries().empty());

  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  NUI_CHECK(replacement_leaf->context_events == 1 && replacement->taken == 1);
  check_single_overlay(tree, replacement, {230, 30, 24, 12});
  NUI_CHECK(old->taken == 0 && old->shown == 0);
}

void callback_disables_anchor_consumes_request_without_later_replay() {
  ui::State<bool> enabled{true};
  auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
  auto record = std::make_shared<SourceRecord>();
  ui::UI tree{columns(leaf(left), ui::make_spec(ui::Enabled{enabled, source(record, leaf(right))}))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  record->on_request = [&] { enabled.set(false); };
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  NUI_CHECK(record->context_events == 1 && record->taken == 1);
  NUI_CHECK(record->shown == 0 && record->invalid_shown == 1 && !record->pending);
  NUI_CHECK(tree.overlay_entries().empty());
  record->on_request = {};
  enabled.set(true); tree.resize({320, 160});
  NUI_CHECK(tree.overlay_entries().empty() && record->shown == 0);
  tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform);
  NUI_CHECK(record->shown == 1 && record->taken == 2 && record->handle.valid());
}

template <class K = ui::Key>
void keyboard_request_targets_focus_rect(bool menu_key) {
  // A missing appended enumerator is a failing oracle, never a skipped test.
  // Keeping the reference dependent lets pointer modes compile/run before the
  // additive Key seam is installed; after installation this body uses real keys.
  if constexpr (requires { K::Menu; K::F10; }) {
    auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
    auto record = std::make_shared<SourceRecord>();
    ui::UI tree{columns(leaf(left), source(record, leaf(right)))};
    test::MockPlatform platform;
    tree.resize({320, 160}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    NUI_CHECK(!left->focused && right->focused);
    const ui::Rect focused = right->focus_bounds;
    NUI_CHECK(focused.x >= 160 && focused.w > 0 && focused.h > 0);
    record->fixed_bounds = focused;
    const auto event = test::key(menu_key ? K::Menu : K::F10, !menu_key);
    NUI_CHECK(tree.dispatch(event, platform) == ui::EventResult::Handled);
    NUI_CHECK(right->context_events == 1 && left->context_events == 0);
    NUI_CHECK(record->context_events == 1 && record->taken == 1 && right->focused);
    // UI must replace the default (0,0) key-event position with logical focus
    // geometry. The exact corner/center convention may vary; allow its edges.
    NUI_CHECK(right->last_position.x >= focused.x && right->last_position.x <= focused.x + focused.w);
    NUI_CHECK(right->last_position.y >= focused.y && right->last_position.y <= focused.y + focused.h);
    check_single_overlay(tree, record, {focused.x, focused.y + focused.h, 24, 12});
    NUI_CHECK(platform.pointer_capture_begin_count == 0 && platform.pointer_capture_end_count == 0);
  } else {
    NUI_CHECK(false && "Key::Menu/Key::F10 additive seam is absent");
  }
}

template <class K = ui::Key>
void unmodified_f10_remains_a_key() {
  if constexpr (requires { K::F10; }) {
    auto record = std::make_shared<SourceRecord>();
    auto editor = std::make_shared<LeafRecord>();
    ui::UI tree{source(record, leaf(editor))}; test::MockPlatform platform;
    tree.resize({320, 160}); tree.activate(platform);
    NUI_CHECK(tree.dispatch(test::key(K::F10), platform) == ui::EventResult::Ignored);
    NUI_CHECK(editor->key_events == 1 && editor->context_events == 0 && record->context_events == 0);
    NUI_CHECK(tree.overlay_entries().empty() && record->taken == 0);
  } else {
    NUI_CHECK(false && "Key::F10 additive seam is absent");
  }
}

void ordinary_down_creates_command_after_source_identity_is_pinned() {
  auto record = std::make_shared<SourceRecord>();
  auto editor = std::make_shared<LeafRecord>();
  record->open_on_down = true;
  record->fixed_bounds = ui::Rect{20, 15, 3, 4};
  ui::UI tree{source(record, leaf(editor))}; test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  NUI_CHECK(editor->focused && !record->pending && record->taken == 0);
  NUI_CHECK(tree.overlay_entries().empty());
  // The owned pending predicate is false at UI's pre-dispatch checkpoint.
  // Source identity must nevertheless be pinned: this actual input creates its
  // first Show command after the descendant ignored Down and it bubbled here.
  NUI_CHECK(tree.dispatch(test::key(ui::Key::Down), platform) == ui::EventResult::Handled);
  NUI_CHECK(editor->key_events == 1 && editor->context_events == 0);
  NUI_CHECK(record->down_events == 1 && record->context_events == 0 && record->taken == 1);
  NUI_CHECK(!record->pending && editor->focused);
  check_single_overlay(tree, record, {20, 19, 24, 12});
}

void empty_legacy_context_source_does_not_mask_accepting_parent() {
  auto left = std::make_shared<LeafRecord>(), right = std::make_shared<LeafRecord>();
  auto outer = std::make_shared<SourceRecord>();
  auto legacy = std::make_shared<LegacyRecord>();
  ui::UI tree{columns(leaf(left), source(outer, legacy_source(legacy, leaf(right))))};
  test::MockPlatform platform;
  tree.resize({320, 160}); tree.activate(platform);
  NUI_CHECK(legacy->inherited_pending && !outer->pending);
  NUI_CHECK(left->focused && !right->focused);
  NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::ContextMenu, 230, 30), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(right->context_events == 1 && legacy->context_events == 1 && outer->context_events == 1);
  NUI_CHECK(legacy->taken == 1 && outer->taken == 1 && !outer->pending);
  NUI_CHECK(left->focused && !right->focused && left->focus_out == 0);
  check_single_overlay(tree, outer, {230, 30, 24, 12});
}

void menu_key() { keyboard_request_targets_focus_rect(true); }
void shift_f10() { keyboard_request_targets_focus_rect(false); }
void plain_f10() { unmodified_f10_remains_a_key(); }
void suite() {
  hit_target_differs_from_focus_and_opens_at_request_point();
  owned_logical_rect_is_used_and_refreshed();
  source_path_uses_layout_prepared_for_this_input();
  declining_inner_source_allows_pending_parent();
  accepting_inner_source_does_not_drain_parent();
  callback_retires_anchor_and_same_key_replacement_needs_fresh_request();
  callback_disables_anchor_consumes_request_without_later_replay();
  menu_key(); shift_f10(); plain_f10();
  ordinary_down_creates_command_after_source_identity_is_pinned();
  empty_legacy_context_source_does_not_mask_accepting_parent();
}

} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "ordinary_down") return test::run("context_menu_ordinary_down", &ordinary_down_creates_command_after_source_identity_is_pinned);
  if (mode == "legacy_parent") return test::run("context_menu_legacy_parent", &empty_legacy_context_source_does_not_mask_accepting_parent);
  if (mode == "point") return test::run("context_menu_point", &hit_target_differs_from_focus_and_opens_at_request_point);
  if (mode == "dirty_geometry") return test::run("context_menu_dirty_geometry", &source_path_uses_layout_prepared_for_this_input);
  if (mode == "rect") return test::run("context_menu_rect", &owned_logical_rect_is_used_and_refreshed);
  if (mode == "parent") return test::run("context_menu_parent", &declining_inner_source_allows_pending_parent);
  if (mode == "inner") return test::run("context_menu_inner", &accepting_inner_source_does_not_drain_parent);
  if (mode == "retired") return test::run("context_menu_retired", &callback_retires_anchor_and_same_key_replacement_needs_fresh_request);
  if (mode == "disabled") return test::run("context_menu_disabled", &callback_disables_anchor_consumes_request_without_later_replay);
  if (mode == "menu") return test::run("context_menu_key_menu", &menu_key);
  if (mode == "shift_f10") return test::run("context_menu_shift_f10", &shift_f10);
  if (mode == "plain_f10") return test::run("context_menu_plain_f10", &plain_f10);
  return test::run("context_menu_core", &suite);
}
