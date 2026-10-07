#pragma once
#include "test_support.hpp"
#include <nativeui/button.hpp>
#include <nativeui/column.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/if.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/visibility.hpp>
#include <optional>
namespace group_test {
inline std::optional<ui::NodeId> node(const ui::UI &tree, std::string_view name,
                                      bool visible_only = false) {
  for (ui::NodeId id = 1; id < 8192; ++id) {
    const auto info = tree.component_semantics(id);
    const auto availability = tree.component_availability(id);
    if (info && info->name == name &&
        (!visible_only || (availability && availability->visibility ==
                                               ui::VisibilityMode::Visible)))
      return id;
  }
  return {};
}
inline std::optional<ui::SemanticInfo> info(const ui::UI &tree,
                                            std::string_view name) {
  const auto id = node(tree, name);
  return id ? tree.component_semantics(*id) : std::nullopt;
}
inline std::string focused(const ui::UI &tree) {
  for (ui::NodeId id = 1; id < 8192; ++id) {
    const auto current = tree.component_semantics(id);
    if (current && current->focused)
      return current->name;
  }
  return {};
}
inline void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto up = test::key(key);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
}
inline void render(ui::UI &tree, ui::Size size = {640, 180}) {
  ui::HeadlessRenderer renderer{size, 1};
  NUI_CHECK(renderer.render(tree));
}
struct Audit {
  ui::Rect bounds{};
  int mounts{}, unmounts{};
  bool throw_measure{}, throw_mount{};
};
class Probe final : public ui::Component {
public:
  explicit Probe(std::shared_ptr<Audit> audit) : audit_(std::move(audit)) {}
  ui::Size
  measure(const std::vector<ui::ChildMetrics> &children) const override {
    if (audit_->throw_measure)
      throw std::runtime_error("injected group child measure");
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  ui::Size
  minimum_size(const std::vector<ui::ChildMetrics> &children) const override {
    return children.empty() ? ui::Size{} : children.front().minimum;
  }
  ui::FlexFactors flex_factors() const noexcept override { return {}; }
  void
  layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics> &,
                  std::vector<ui::ChildPlacement> &placements) const override {
    if (!placements.empty())
      placements.front().bounds = bounds;
  }
  void mount(ui::MountContext &) override {
    ++audit_->mounts;
    if (audit_->throw_mount)
      throw std::runtime_error("injected group child mount");
  }
  void unmount(ui::LifecycleContext &) override { ++audit_->unmounts; }
  void paint(ui::PaintContext &) const override {}

private:
  void layout_committed(ui::Rect, ui::Rect bounds) noexcept override {
    audit_->bounds = bounds;
  }
  std::shared_ptr<Audit> audit_;
};
inline ui::Spec probe(std::shared_ptr<Audit> audit, ui::Spec child) {
  return {[audit = std::move(audit)] { return std::make_unique<Probe>(audit); },
          {std::move(child)}};
}
inline void click(ui::UI &tree, test::MockPlatform &platform, ui::Rect bounds,
                  bool accept = true) {
  NUI_CHECK(!bounds.empty());
  const auto x = bounds.x + bounds.w * .5f, y = bounds.y + bounds.h * .5f;
  tree.dispatch(test::pointer(ui::InputType::PointerDown, x, y), platform);
  tree.dispatch(test::pointer(accept ? ui::InputType::PointerUp
                                     : ui::InputType::PointerCancel,
                              x, y),
                platform);
}
inline bool same_rect(ui::Rect a, ui::Rect b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}
} // namespace group_test
