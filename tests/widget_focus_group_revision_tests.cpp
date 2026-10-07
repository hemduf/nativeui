#include "test_support.hpp"
#include <nativeui/detail/focus_group.hpp>
#include <type_traits>
namespace {
template <class Base>
inline constexpr bool has_revision =
    requires(const Base &value) { value.focus_group_selection_revision(); };
template <class Base> class LegacyRevisionBase : public Base {
public:
  virtual std::uint64_t focus_group_selection_revision() const noexcept {
    return 0;
  }
};
using Participant =
    std::conditional_t<has_revision<ui::detail::FocusGroupParticipant>,
                       ui::detail::FocusGroupParticipant,
                       LegacyRevisionBase<ui::detail::FocusGroupParticipant>>;
struct Memory {
  ui::Binding<int> source;
  int calls{};
};
class Item final : public ui::Component, public Participant {
public:
  Item(std::shared_ptr<Memory> memory, int value)
      : memory_(std::move(memory)), value_(value) {}
  bool focusable() const noexcept override { return true; }
  const void *focus_group_identity() const noexcept override {
    return memory_.get();
  }
  bool focus_group_selected() const override {
    return memory_->source.get() == value_;
  }
  std::uint64_t focus_group_selection_revision() const noexcept override {
    return memory_->source.revision();
  }
  void focus_group_select() override { focus_group_select_guarded({}); }
  void
  focus_group_select_guarded(const std::function<bool()> &allowed) override {
    auto memory = memory_;
    const auto source = memory->source;
    const auto revision = source.revision();
    if (allowed && !allowed())
      return;
    ++memory->calls;
    auto writer = source;
    writer.set_if(value_, [source, revision, allowed] {
      return source.valid() && source.revision() == revision &&
             (!allowed || allowed());
    });
  }
  ui::Size measure(const std::vector<ui::ChildMetrics> &) const override {
    return {80, 32};
  }
  ui::SemanticInfo semantics() const override {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::RadioButton;
    info.name = std::to_string(value_);
    return info;
  }
  void focus_changed(bool focused, ui::FocusContext& context) override {
    if (focused) context.invalidate();
  }
  void paint(ui::PaintContext &) const override {}

private:
  std::shared_ptr<Memory> memory_;
  int value_{};
};
ui::Spec item(const std::shared_ptr<Memory> &memory, int value) {
  return {[memory, value] { return std::make_unique<Item>(memory, value); },
          {}};
}
void external_write_during_focus_exposure_wins() {
  ui::State<int> value{1};
  auto memory = std::make_shared<Memory>(Memory{value.binding()});
  ui::UI tree{ui::Row{item(memory, 1), item(memory, 2), item(memory, 3)}};
  test::MockPlatform platform;
  tree.resize({300, 50});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{300, 50}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed{true};
  tree.set_invalidation_callback([&] {
    if (armed) {
      armed = false;
      value.set(73);
    }
  });
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(!armed && value.get() == 73 && memory->calls == 0);
  tree.clear_invalidation_callback();
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(value.get() == 3 && memory->calls == 1);
}
void equal_write_does_not_cancel_navigation() {
  ui::State<int> value{1};
  auto memory = std::make_shared<Memory>(Memory{value.binding()});
  ui::UI tree{ui::Row{item(memory, 1), item(memory, 2)}};
  test::MockPlatform platform;
  tree.resize({300, 50});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{300, 50}, 1};
  NUI_CHECK(renderer.render(tree));
  bool armed{true};
  tree.set_invalidation_callback([&] {
    if (armed) {
      armed = false;
      value.set(1);
    }
  });
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(!armed && value.get() == 2 && memory->calls == 1);
}
void suite() {
  external_write_during_focus_exposure_wins();
  equal_write_does_not_cancel_navigation();
}
} // namespace
int main() { return test::run("focus_group_source_revision", &suite); }
