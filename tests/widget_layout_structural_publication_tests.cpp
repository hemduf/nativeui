#include "test_support.hpp"

#include <nativeui/for_each.hpp>

#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
struct Observation {
  int old_commits{};
  int old_destroyed{};
  int replacement_mounts{};
  int replacement_layouts{};
  int replacement_inputs{};
  int replacement_commits{};
  bool hook_failed{};
  bool fail_replacement{true};
};

class PublicationLeaf final : public ui::Component {
public:
  PublicationLeaf(int id, ui::Binding<std::vector<int>> items,
                  std::shared_ptr<Observation> observation)
      : id_(id), items_(std::move(items)), observation_(std::move(observation)) {}
  ~PublicationLeaf() override {
    if (id_ == 1) ++observation_->old_destroyed;
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
    return {80.0f, 40.0f};
  }
  bool pointer_targetable() const noexcept override { return true; }
  void mount(ui::MountContext&) override {
    if (id_ == 2) ++observation_->replacement_mounts;
  }
  void layout_children(ui::Rect, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>&) const override {
    if (id_ != 2) return;
    ++observation_->replacement_layouts;
    if (observation_->fail_replacement)
      throw std::runtime_error("replacement has no published layout");
  }
  ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
    if (event.type != ui::InputType::PointerDown || id_ != 2)
      return ui::EventResult::Ignored;
    ++observation_->replacement_inputs;
    return ui::EventResult::Handled;
  }
  void paint(ui::PaintContext&) const override {}

private:
  void layout_committed(ui::Rect, ui::Rect) noexcept override {
    if (id_ == 2) {
      ++observation_->replacement_commits;
      return;
    }
    if (observation_->old_commits++ != 0) return;
    // This hook runs after the old geometry succeeds, inside the publication
    // barrier. ForEach only enqueues here; its old child remains retained until
    // the next ordinary checkpoint, although its desired-source epoch changes.
    try { items_.set({2}); }
    catch (...) { observation_->hook_failed = true; }
  }
  int id_;
  ui::Binding<std::vector<int>> items_;
  std::shared_ptr<Observation> observation_;
};

void replacement_after_commit_withdraws_old_geometry_authority() {
  ui::State<std::vector<int>> items{{1}};
  auto observation = std::make_shared<Observation>();
  const auto binding = items.binding();
  auto component = ui::ForEach<int>{
      items, [](int id) { return id; },
      [binding, observation](int id) {
        return ui::Spec{
            [id, binding, observation] {
              return std::make_unique<PublicationLeaf>(id, binding, observation);
            }, {}};
      }};
  test::MockPlatform platform;
  ui::Tree tree{ui::compile(ui::make_spec(std::move(component)))};
  tree.mount();
  tree.activate_focus(platform);

  tree.layout({80.0f, 40.0f});
  NUI_CHECK(!observation->hook_failed && observation->old_commits == 1);
  NUI_CHECK(items.get() == std::vector<int>{2});
  NUI_CHECK(observation->old_destroyed == 0);
  NUI_CHECK(observation->replacement_mounts == 0);

  bool resize_failed{};
  try { tree.layout({80.0f, 40.0f}); }
  catch (const std::runtime_error& error) {
    resize_failed = std::string_view{error.what()} == "replacement has no published layout";
  }
  NUI_CHECK(resize_failed && observation->old_destroyed == 1);
  NUI_CHECK(observation->replacement_mounts == 1);
  NUI_CHECK(observation->replacement_layouts == 1);
  NUI_CHECK(observation->replacement_commits == 0 && tree.layout_dirty());

  // The old bounds belonged to a different NodeId. Input must retry layout and
  // propagate the replacement's fault, not silently route a fabricated rollback
  // geometry for this newly inserted, never-published component.
  bool input_retried{};
  try { (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 5.0f, 5.0f), platform); }
  catch (const std::runtime_error& error) {
    input_retried = std::string_view{error.what()} == "replacement has no published layout";
  }
  NUI_CHECK(input_retried);
  NUI_CHECK(observation->replacement_layouts == 2);
  NUI_CHECK(observation->replacement_inputs == 0 && tree.layout_dirty());

  observation->fail_replacement = false;
  NUI_CHECK(tree.dispatch(test::pointer(ui::InputType::PointerDown, 5.0f, 5.0f), platform)
            == ui::EventResult::Handled);
  NUI_CHECK(observation->replacement_inputs == 1);
  NUI_CHECK(observation->replacement_commits == 1 && !tree.layout_dirty());
  NUI_CHECK(observation->replacement_mounts == 1);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 5.0f, 5.0f), platform);
  tree.deactivate_focus(platform);
  tree.unmount();
}
} // namespace

int main() {
  return test::run("layout_structural_publication",
                   &replacement_after_commit_withdraws_old_geometry_authority);
}
