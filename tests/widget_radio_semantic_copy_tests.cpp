#include "test_support.hpp"

#include <nativeui/radio_button.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace {

struct LateCopy {
  bool armed{};
  int copies{};
  std::function<void()> callback;
};

// Deliberately non-default-constructible and unhashed. Moving does not invoke
// application code; the armed copy marks the last candidate preparation.
struct Choice {
  int value;
  std::shared_ptr<LateCopy> hook;

  Choice(int selected, std::shared_ptr<LateCopy> observer)
      : value(selected), hook(std::move(observer)) {}
  Choice(const Choice& other) : value(other.value), hook(other.hook) {
    if (value == 2 && hook && std::exchange(hook->armed, false)) {
      ++hook->copies;
      auto callback = hook->callback;
      if (callback) callback();
    }
  }
  Choice(Choice&&) noexcept = default;
  Choice& operator=(const Choice&) = default;
  Choice& operator=(Choice&&) noexcept = default;
  friend bool operator==(const Choice& a, const Choice& b) {
    return a.value == b.value;
  }
};

struct Request {
  std::function<void()> action;
};

class LabelOwner final : public ui::Component {
public:
  explicit LabelOwner(std::shared_ptr<Request> request)
      : request_(std::move(request)) {}
  void mount(ui::MountContext& context) override {
    request_->action = context.descendant_action_requester("choice");
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& placements) const override {
    for (auto& child : placements) child.bounds = bounds;
  }
  void paint(ui::PaintContext&) const override {}

private:
  std::shared_ptr<Request> request_;
};

void late_candidate_copy_cannot_publish_after_read_only_transition() {
  auto hook = std::make_shared<LateCopy>();
  auto request = std::make_shared<Request>();
  ui::State<Choice> selected{Choice{1, hook}};
  ui::State<bool> read_only{false};
  ui::RadioGroup<Choice> group{selected};
  auto target = ui::make_spec(ui::ReadOnly{
      read_only, ui::RadioButton{group, Choice{2, hook}, "Second"}});
  ui::Spec recipe{
      [request] { return std::make_unique<LabelOwner>(request); },
      {ui::keyed("choice", std::move(target))}};
  ui::UI tree{std::move(recipe)};
  test::MockPlatform platform;
  tree.resize({200.0f, 60.0f});
  tree.activate(platform);
  int notifications{};
  auto subscription = selected.observe([&](const Choice&) { ++notifications; });

  // Resolve and pin the permitted action before arming the copy. Equality and
  // focus preparation are harmless; only candidate T copying changes policy.
  NUI_CHECK(static_cast<bool>(request->action));
  request->action();
  hook->callback = [&] { read_only.set(true); };
  hook->armed = true;
  tree.resize({200.0f, 60.0f});

  NUI_CHECK(hook->copies == 1 && read_only.get());
  NUI_CHECK(selected.get().value == 1 && notifications == 0);
  tree.resize({200.0f, 60.0f});
  NUI_CHECK(selected.get().value == 1 && notifications == 0);

  // The consumed stale action must not revive when availability returns.
  read_only.set(false);
  tree.resize({200.0f, 60.0f});
  NUI_CHECK(selected.get().value == 1 && notifications == 0);
  request->action();
  tree.resize({200.0f, 60.0f});
  NUI_CHECK(selected.get().value == 2 && notifications == 1);
}

} // namespace

int main() {
  return test::run("radio_semantic_late_copy",
                   &late_candidate_copy_cannot_publish_after_read_only_transition);
}
