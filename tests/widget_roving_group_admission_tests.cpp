#include "test_support.hpp"

#include <nativeui/button.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/text_input.hpp>
#include <nativeui/toggle_button.hpp>

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {
struct TreeTestAccess {
  static std::string focused_name(Tree& tree) {
    auto* current = tree.current_focus_node();
    return current ? current->component->semantics().name : std::string{};
  }
  static Rect bounds(Tree& tree, NodeId id) {
    auto* node = tree.root_ ? tree.find_node_mutable(*tree.root_, id) : nullptr;
    return node ? node->bounds : Rect{};
  }
  static bool focused_below(Tree& tree, NodeId id) {
    auto* current = tree.current_focus_node();
    for (; current; current = current->parent) if (current->id == id) return true;
    return false;
  }
};
} // namespace ui

namespace {
// The fallback inherits the actual private Core protocol. It adds only the
// future notification slot to this test wrapper, so explicit override compiles
// against old and new headers. An old Tree cannot call that slot and still
// lacks ancestor admission; no Tree routing or actual widget is substituted.
template <class Base> inline constexpr bool has_did_focus = requires(Base& value) {
  value.focus_group_did_focus();
};
template <class Base> class LegacyHookBase : public Base {
public:
  virtual void focus_group_did_focus() noexcept {}
};
using ParticipantBase = std::conditional_t<has_did_focus<ui::detail::FocusGroupParticipant>,
                                         ui::detail::FocusGroupParticipant,
                                         LegacyHookBase<ui::detail::FocusGroupParticipant>>;

struct GroupMemory {
  std::size_t last{};
  int select_calls{};
  std::array<int, 4> focus_commits{};
  std::array<ui::NodeId, 4> items{};
};
struct Audit {
  std::vector<std::shared_ptr<GroupMemory>> compiled;
  int button_activations{};
};
class Item final : public ui::Component, public ParticipantBase {
public:
  explicit Item(std::size_t index) : index_(index) {}
  void bind(std::shared_ptr<GroupMemory> memory) { memory_ = std::move(memory); }
  bool focusable() const noexcept override { return false; }
  const void* focus_group_identity() const noexcept override { return memory_.get(); }
  bool focus_group_selected() const override { return memory_ && memory_->last == index_; }
  void focus_group_select() override { if (memory_) ++memory_->select_calls; }
  void focus_group_did_focus() noexcept override {
    if (!memory_) return;
    memory_->last = index_;
    ++memory_->focus_commits[index_];
  }
  bool focus_group_accepts_navigation_key(ui::Key key) const noexcept override {
    return key == ui::Key::Left || key == ui::Key::Right;
  }
  void mount(ui::MountContext& context) override {
    if (memory_) memory_->items[index_] = context.node_id();
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  ui::Size minimum_size(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().minimum;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& children) const override {
    for (auto& child : children) child.bounds = bounds;
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::size_t index_{};
  std::shared_ptr<GroupMemory> memory_;
};
ui::Spec item(std::size_t index, ui::Spec child) {
  return {[index] { return std::make_unique<Item>(index); }, {std::move(child)}};
}
class Group final : public ui::Component {
public:
  explicit Group(const std::shared_ptr<Audit>& audit)
      : memory_(std::make_shared<GroupMemory>()) { audit->compiled.push_back(memory_); }
  bool focusable() const noexcept override { return false; }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& children) const override {
    for (auto& child : children) child.bounds = bounds;
  }
  void paint(ui::PaintContext&) const override {}
private:
  void bind_descendant_context(ui::Component& component) const override {
    if (auto* target = dynamic_cast<Item*>(&component)) target->bind(memory_);
  }
  std::shared_ptr<GroupMemory> memory_;
};
ui::Spec group(ui::Binding<bool> toggle, ui::Binding<std::string> text,
               const std::shared_ptr<Audit>& audit, bool with_editor = false) {
  auto first = item(0, ui::make_spec(ui::Button{"First", [audit] { ++audit->button_activations; }}));
  auto middle = item(1, ui::make_spec(ui::ToggleButton{"Middle", std::move(toggle)}));
  auto last = item(2, ui::make_spec(ui::Button{"Last", [audit] { ++audit->button_activations; }}));
  ui::Spec row;
  if (with_editor) {
    auto editor = item(3, ui::make_spec(ui::TextInput{"Editor", std::move(text)}));
    row = ui::make_spec(ui::Row{std::move(first), std::move(editor),
                               std::move(middle), std::move(last)});
  } else {
    row = ui::make_spec(ui::Row{std::move(first), std::move(middle), std::move(last)});
  }
  return {[audit] { return std::make_unique<Group>(audit); }, {std::move(row)}};
}
ui::Spec between_buttons(ui::Spec content) {
  return ui::make_spec(ui::Column{ui::Button{"Before", {}}, std::move(content),
                                  ui::Button{"After", {}}});
}
struct Fixture {
  test::MockPlatform platform;
  ui::Tree tree;
  explicit Fixture(ui::Spec spec) : tree(ui::compile(std::move(spec))) {
    tree.mount(); tree.layout({600, 180}); tree.activate_focus(platform);
    NUI_CHECK(ui::TreeTestAccess::focused_name(tree) == "Before");
  }
  void key(ui::Key key, bool shift = false) { tree.dispatch(test::key(key, shift), platform); }
  void expect(std::string_view name) { NUI_CHECK(ui::TreeTestAccess::focused_name(tree) == name); }
  void pointer(const std::shared_ptr<GroupMemory>& memory, std::size_t index, bool click) {
    const auto bounds = ui::TreeTestAccess::bounds(tree, memory->items[index]);
    NUI_CHECK(!bounds.empty());
    const float x = bounds.x + bounds.w * 0.5f, y = bounds.y + bounds.h * 0.5f;
    tree.dispatch(test::pointer(ui::InputType::PointerDown, x, y), platform);
    tree.dispatch(test::pointer(click ? ui::InputType::PointerUp : ui::InputType::PointerCancel,
                                x, y), platform);
    NUI_CHECK(ui::TreeTestAccess::focused_below(tree, memory->items[index]));
  }
  void finish() {
    tree.deactivate_focus(platform);
    NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
  }
};

void arrows_wrap_without_activating_actual_controls() {
  ui::State<bool> toggle{false}; ui::State<std::string> text{"abc"};
  auto audit = std::make_shared<Audit>();
  Fixture fixture{between_buttons(group(toggle.binding(), text.binding(), audit))};
  const auto memory = audit->compiled.at(0);
  fixture.key(ui::Key::Tab); fixture.expect("First");
  fixture.key(ui::Key::Right); fixture.expect("Middle");
  fixture.key(ui::Key::Right); fixture.expect("Last");
  fixture.key(ui::Key::Right); fixture.expect("First");
  fixture.key(ui::Key::Left); fixture.expect("Last");
  NUI_CHECK(memory->last == 2 && memory->select_calls == 4);
  NUI_CHECK(!toggle.get() && audit->button_activations == 0);
  fixture.finish();
}
void tab_is_one_stop_and_returns_to_last_item() {
  ui::State<bool> toggle{false}; ui::State<std::string> text{"abc"};
  auto audit = std::make_shared<Audit>();
  Fixture fixture{between_buttons(group(toggle.binding(), text.binding(), audit))};
  const auto memory = audit->compiled.at(0);
  fixture.key(ui::Key::Tab); fixture.expect("First");
  fixture.pointer(memory, 1, false); fixture.expect("Middle");
  NUI_CHECK(!toggle.get());
  fixture.key(ui::Key::Tab); fixture.expect("After");
  fixture.key(ui::Key::Tab, true); fixture.expect("Middle");
  fixture.key(ui::Key::Tab, true); fixture.expect("Before");
  fixture.key(ui::Key::Tab); fixture.expect("Middle");
  NUI_CHECK(memory->last == 1 && audit->button_activations == 0);
  fixture.finish();
}
void read_only_click_and_arrows_update_only_focus_cache() {
  ui::State<bool> toggle{false}, read_only{true}; ui::State<std::string> text{"abc"};
  auto audit = std::make_shared<Audit>();
  auto content = ui::make_spec(ui::ReadOnly{read_only, group(toggle.binding(), text.binding(), audit)});
  Fixture fixture{between_buttons(std::move(content))};
  const auto memory = audit->compiled.at(0);
  fixture.key(ui::Key::Tab); fixture.expect("First");
  fixture.key(ui::Key::Right); fixture.expect("Middle");
  NUI_CHECK(memory->last == 1 && memory->select_calls == 0 && !toggle.get());
  fixture.key(ui::Key::Left); fixture.expect("First");
  const int previous = memory->focus_commits[1];
  fixture.pointer(memory, 1, true); fixture.expect("Middle");
  NUI_CHECK(memory->last == 1 && memory->focus_commits[1] == previous + 1);
  NUI_CHECK(memory->select_calls == 0 && !toggle.get());
  fixture.tree.refresh_focus(fixture.platform);
  NUI_CHECK(memory->focus_commits[1] == previous + 1);
  fixture.key(ui::Key::Tab); fixture.expect("After");
  fixture.key(ui::Key::Tab, true); fixture.expect("Middle");
  NUI_CHECK(memory->last == 1 && !toggle.get() && audit->button_activations == 0);
  fixture.finish();
}
void embedded_text_input_keeps_its_editing_keys() {
  ui::State<bool> toggle{false}; ui::State<std::string> text{"abc"};
  auto audit = std::make_shared<Audit>();
  Fixture fixture{between_buttons(group(toggle.binding(), text.binding(), audit, true))};
  const auto memory = audit->compiled.at(0);
  fixture.key(ui::Key::Tab); fixture.expect("First");
  const auto cache = memory->last;
  const int select_calls = memory->select_calls;
  fixture.pointer(memory, 3, false); fixture.expect("Editor");
  fixture.key(ui::Key::Home);
  fixture.key(ui::Key::Right);
  fixture.tree.dispatch(test::text("x"), fixture.platform);
  NUI_CHECK(text.get() == "axbc");
  NUI_CHECK(memory->last == cache && memory->select_calls == select_calls);
  NUI_CHECK(memory->focus_commits[3] == 0 && !toggle.get());
  fixture.finish();
}
void copied_spec_has_independent_group_memory() {
  ui::State<bool> toggle{false}; ui::State<std::string> text{"abc"};
  auto audit = std::make_shared<Audit>();
  auto spec = between_buttons(group(toggle.binding(), text.binding(), audit));
  Fixture first{ui::Spec{spec}}, second{ui::Spec{spec}};
  NUI_CHECK(audit->compiled.size() == 2 && audit->compiled[0] != audit->compiled[1]);
  first.key(ui::Key::Tab); first.key(ui::Key::Right); first.expect("Middle");
  second.key(ui::Key::Tab); second.key(ui::Key::Right); second.key(ui::Key::Right); second.expect("Last");
  NUI_CHECK(audit->compiled[0]->last == 1 && audit->compiled[1]->last == 2);
  first.key(ui::Key::Tab); first.expect("After"); first.key(ui::Key::Tab, true); first.expect("Middle");
  NUI_CHECK(audit->compiled[1]->last == 2 && !toggle.get());
  first.finish(); second.finish();
}
void suite() {
  arrows_wrap_without_activating_actual_controls();
  tab_is_one_stop_and_returns_to_last_item();
  read_only_click_and_arrows_update_only_focus_cache();
  embedded_text_input_keeps_its_editing_keys();
  copied_spec_has_independent_group_memory();
}
} // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "arrows") return test::run("roving_admission_arrows", &arrows_wrap_without_activating_actual_controls);
  if (mode == "tab") return test::run("roving_admission_tab", &tab_is_one_stop_and_returns_to_last_item);
  if (mode == "read_only") return test::run("roving_admission_read_only", &read_only_click_and_arrows_update_only_focus_cache);
  if (mode == "text") return test::run("roving_admission_text", &embedded_text_input_keeps_its_editing_keys);
  if (mode == "instances") return test::run("roving_admission_instances", &copied_spec_has_independent_group_memory);
  if (mode == "all") return test::run("roving_group_admission", &suite);
  return 2;
}
