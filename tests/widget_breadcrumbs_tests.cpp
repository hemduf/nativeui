#include "test_support.hpp"
#include <nativeui/breadcrumbs.hpp>
#include <nativeui/visibility.hpp>
#include "include/core/SkSurface.h"

namespace {
using Path = std::vector<ui::BreadcrumbItem>;
void release(ui::UI &tree, test::MockPlatform &platform, ui::Key key) {
  auto event = test::key(key);
  event.type = ui::InputType::KeyUp;
  tree.dispatch(event, platform);
}
void exact_keys_and_terminal_destination() {
  ui::State<Path> path{
      Path{{"root", "Root"}, {"home", "Home"}, {"leaf", "Destination"}}};
  std::vector<std::string> calls;
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [&](const auto &key) { calls.push_back(key); })};
  test::MockPlatform platform;
  tree.resize({600, 40});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  NUI_CHECK(calls.empty());
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls == std::vector<std::string>{"root"});
  tree.dispatch(test::key(ui::Key::Tab), platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls.back() == "home");
  bool terminal{};
  for (ui::NodeId id = 1; id < 32; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->name == "Destination") {
      terminal = true;
      NUI_CHECK(info->role == ui::SemanticRole::Text && !info->focusable &&
                info->actions.empty());
    }
  }
  NUI_CHECK(terminal);
}
void removed_or_terminal_key_cancels_a_press() {
  ui::State<Path> path{Path{{"root", "Root"}, {"leaf", "Destination"}}};
  int calls{};
  ui::UI tree{
      ui::Breadcrumbs{path}.on_navigate([&](const auto &) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({400, 40});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 15, 20), platform);
  path.set({{"root", "Root"}});
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 15, 20), platform);
  NUI_CHECK(calls == 0 && platform.pointer_capture_end_count == 1);
}
void overflow_is_keyed_and_invalid_updates_keep_the_last_path() {
  Path original{{"root", "Root"},
                {"one", "First"},
                {"two", "Second"},
                {"three", "Third"},
                {"leaf", "Very long Unicode destination"}};
  ui::State<Path> path{original};
  std::vector<std::string> calls;
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [&](const auto &key) { calls.push_back(key); })};
  test::MockPlatform platform;
  tree.resize({100, 40});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == std::vector<std::string>{"root"});
  NUI_CHECK(tree.overlay_entries().empty());
  path.set({{"dup", "One"}, {"dup", "Two"}});
  ui::HeadlessRenderer renderer{{100, 40}, 1};
  NUI_CHECK(renderer.render(tree));
  bool old_leaf{}, diagnostic{};
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (!info)
      continue;
    old_leaf |= info->name == original.back().label;
    diagnostic |=
        info->role == ui::SemanticRole::Group && !info->description.empty();
  }
  NUI_CHECK(old_leaf && diagnostic);
  bool rejected{};
  try {
    (void)ui::Breadcrumbs{path}.spec();
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  NUI_CHECK(rejected);
}
void callback_failure_disarms_and_allows_another_activation() {
  ui::State<Path> path{Path{{"root", "Root"}, {"leaf", "Destination"}}};
  bool fail = true;
  int calls{};
  ui::UI tree{ui::Breadcrumbs{path}.on_navigate([&](const auto &) {
    ++calls;
    if (fail)
      throw std::runtime_error("navigate");
  })};
  test::MockPlatform platform;
  tree.resize({400, 40});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 15, 20), platform);
  bool caught{};
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 15, 20), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1 && platform.pointer_capture_end_count == 1);
  ui::HeadlessRenderer renderer{{400, 40}, 1};
  NUI_CHECK(renderer.render(tree) && calls == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Space), platform);
  release(tree, platform, ui::Key::Space);
  NUI_CHECK(calls == 2);
}
struct BreadcrumbLayoutFault { bool armed{}; };
class BreadcrumbFaultComponent final : public ui::Component {
public:
  explicit BreadcrumbFaultComponent(std::shared_ptr<BreadcrumbLayoutFault> fault)
      : fault_(std::move(fault)) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {1,1}; }
  bool pointer_targetable() const noexcept override { return false; }
  void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                       std::vector<ui::ChildPlacement>& children) const override {
    if (fault_->armed) throw std::runtime_error("breadcrumb neighbour layout");
    for (auto& child : children) child.bounds = bounds;
  }
  void paint(ui::PaintContext&) const override {}
private:
  std::shared_ptr<BreadcrumbLayoutFault> fault_;
};
void failed_layout_preserves_published_crumb_visibility() {
  ui::State<Path> path{Path{{"root", "Root"}, {"one", "First"},
                           {"two", "Second"}, {"leaf", "Destination"}}};
  auto fault = std::make_shared<BreadcrumbLayoutFault>();
  int calls = 0;
  auto neighbour = ui::Spec{[fault] { return std::make_unique<BreadcrumbFaultComponent>(fault); },
                            {ui::make_spec(ui::Spacer{1,1})}};
  ui::UI tree{ui::Stack{ui::Breadcrumbs{path}.on_navigate([&](const auto&) { ++calls; }),
                       std::move(neighbour)}};
  test::MockPlatform platform;
  tree.resize({600,40}); tree.activate(platform);
  ui::NodeId root = ui::kInvalidNodeId;
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->name == "Root") root = id;
  }
  NUI_CHECK(root != ui::kInvalidNodeId);
  fault->armed = true;
  bool caught = false;
  try { tree.resize({100,40}); }
  catch (const std::runtime_error& error) {
    caught = std::string_view{error.what()} == "breadcrumb neighbour layout";
  }
  NUI_CHECK(caught);
  const auto rolled_back = tree.component_semantics(root);
  NUI_CHECK(rolled_back && rolled_back->name == "Root" && rolled_back->enabled);
  fault->armed = false;
  tree.resize({100,40});
  ui::HeadlessRenderer renderer{{100,40},1};
  NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::key(ui::Key::Space),platform);
  release(tree,platform,ui::Key::Space);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.dispatch(test::key(ui::Key::Enter),platform);
  NUI_CHECK(calls == 1 && tree.overlay_entries().empty());
}
void path_replacement_recovers_after_close_invalidation_failure() {
  ui::State<Path> path{Path{{"root", "Root"}, {"one", "First"},
                           {"two", "Second"}, {"leaf", "Destination"}}};
  ui::UI tree{ui::Breadcrumbs{path}.on_navigate([](const auto&) {})};
  test::MockPlatform platform;
  tree.resize({100,40}); tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Space),platform);
  release(tree,platform,ui::Key::Space);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  // Consume opening damage so close() must expose a fresh invalidation before
  // the new path's structural/layout publication suffix can start.
  ui::HeadlessRenderer renderer{{100,40},1};
  NUI_CHECK(renderer.render(tree));
  NUI_CHECK(tree.overlay_entries().size() == 1);
  bool armed = false, before_suffix = false;
  int boundaries = 0;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed,false)) return;
    ++boundaries;
    bool replacement_present = false;
    for (ui::NodeId id = 1; id < 64; ++id) {
      const auto info = tree.component_semantics(id);
      replacement_present |= info &&
          (info->name == "New root" || info->name == "New destination");
    }
    before_suffix = !replacement_present && tree.overlay_entries().size() == 1;
    throw std::runtime_error("breadcrumb close exposure");
  });
  armed = true;
  bool caught = false;
  try { path.set({{"new-root", "New root"}, {"new-leaf", "New destination"}}); }
  catch (const std::runtime_error& error) {
    caught = std::string_view{error.what()} == "breadcrumb close exposure";
  }
  NUI_CHECK(caught && boundaries == 1 && before_suffix);
  tree.clear_invalidation_callback();
  tree.resize({400,40});
  renderer.resize({400,40});
  NUI_CHECK(renderer.render(tree));
  bool root = false, destination = false;
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (!info) continue;
    root |= info->name == "New root";
    destination |= info->name == "New destination";
  }
  NUI_CHECK(root && destination && tree.overlay_entries().empty());
}
void lazy_fit_publishes_overflow_before_first_paint() {
  ui::State<Path> path{Path{{"root", "Root"}}};
  ui::UI tree{ui::Breadcrumbs{path}.on_navigate([](const auto&) {})};
  test::MockPlatform platform;
  tree.resize({100,40}); tree.activate(platform);
  ui::HeadlessRenderer renderer{{100,40},1};
  NUI_CHECK(renderer.render(tree));
  path.set({{"root", "Root"}, {"one", "First"},
            {"two", "Second"}, {"leaf", "Destination"}});
  // HeadlessRenderer performs an explicit resize before paint. Call the public
  // paint API directly to qualify ensure_layout's lazy commit checkpoint.
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100,40));
  NUI_CHECK(surface);
  auto* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  tree.paint(*canvas,platform);
  SkPixmap first;
  NUI_CHECK(surface->peekPixels(&first));
  const auto* first_bytes = static_cast<const std::uint8_t*>(first.addr());
  const std::vector<std::uint8_t> first_pixels{first_bytes,first_bytes+first.computeByteSize()};
  bool overflow = false;
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    overflow |= info && info->name == "More ancestors";
  }
  NUI_CHECK(overflow);
  canvas->clear(SK_ColorBLACK);
  tree.paint(*canvas,platform);
  SkPixmap second;
  NUI_CHECK(surface->peekPixels(&second));
  const auto* second_bytes = static_cast<const std::uint8_t*>(second.addr());
  const std::vector<std::uint8_t> second_pixels{second_bytes,second_bytes+second.computeByteSize()};
  NUI_CHECK(first_pixels == second_pixels);
}
void lazy_fit_routes_the_first_pointer_to_new_overflow() {
  ui::State<Path> path{Path{{"root", "Root"}}};
  int calls{};
  ui::UI tree{ui::Breadcrumbs{path}.on_navigate([&](const auto&) { ++calls; })};
  test::MockPlatform platform;
  tree.resize({100,40});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{100,40},1};
  NUI_CHECK(renderer.render(tree));
  path.set({{"root", "Root"}, {"one", "First"},
            {"two", "Second"}, {"leaf", "Destination"}});
  // No paint, semantic query or explicit resize may repair availability before
  // this first pointer hit test against the newly committed overflow fit.
  const auto down=tree.dispatch(test::pointer(ui::InputType::PointerDown,18,20),platform);
  NUI_CHECK(ui::handled(down));
  tree.dispatch(test::pointer(ui::InputType::PointerUp,18,20),platform);
  NUI_CHECK(tree.overlay_entries().size()==1);
  tree.dispatch(test::key(ui::Key::Enter),platform);
  NUI_CHECK(calls==1 && tree.overlay_entries().empty());
  NUI_CHECK(renderer.render(tree));
  tree.deactivate(platform);
}
void failed_fit_is_discarded_when_collapsed_frame_skips_layout() {
  ui::State<Path> path{Path{{"root", "Root"}, {"one", "First"},
                           {"two", "Second"}, {"leaf", "Destination"}}};
  ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
  auto fault = std::make_shared<BreadcrumbLayoutFault>();
  auto neighbour = ui::Spec{[fault] { return std::make_unique<BreadcrumbFaultComponent>(fault); },
                            {ui::make_spec(ui::Spacer{1,1})}};
  ui::UI tree{ui::Stack{ui::Visibility{visibility,
      ui::Breadcrumbs{path}.on_navigate([](const auto&) {})},std::move(neighbour)}};
  test::MockPlatform platform;
  tree.resize({600,40}); tree.activate(platform);
  ui::NodeId root = ui::kInvalidNodeId;
  for (ui::NodeId id = 1; id < 64; ++id) {
    const auto info = tree.component_semantics(id);
    if (info && info->name == "Root") root = id;
  }
  NUI_CHECK(root != ui::kInvalidNodeId);
  fault->armed = true;
  bool caught = false;
  try { tree.resize({100,40}); }
  catch (const std::runtime_error& error) {
    caught = std::string_view{error.what()} == "breadcrumb neighbour layout";
  }
  NUI_CHECK(caught);
  fault->armed = false;
  visibility.set(ui::VisibilityMode::Collapsed);
  tree.resize({100,40});
  const auto skipped = tree.component_semantics(root);
  NUI_CHECK(skipped && skipped->name == "Root");
  visibility.set(ui::VisibilityMode::Visible);
  tree.resize({100,40});
  ui::HeadlessRenderer renderer{{100,40},1};
  NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::key(ui::Key::Space),platform);
  release(tree,platform,ui::Key::Space);
  NUI_CHECK(tree.overlay_entries().size() == 1);
  tree.deactivate(platform);
}
void suite() {
  failed_layout_preserves_published_crumb_visibility();
  path_replacement_recovers_after_close_invalidation_failure();
  lazy_fit_publishes_overflow_before_first_paint();
  lazy_fit_routes_the_first_pointer_to_new_overflow();
  failed_fit_is_discarded_when_collapsed_frame_skips_layout();
  exact_keys_and_terminal_destination();
  removed_or_terminal_key_cancels_a_press();
  overflow_is_keyed_and_invalid_updates_keep_the_last_path();
  callback_failure_disarms_and_allows_another_activation();
}
} // namespace
int main(int argc, char** argv) {
  if (argc == 2) {
    const std::string_view selected{argv[1]};
    if (selected == "close_exposure")
      return test::run("breadcrumb_close_exposure", &path_replacement_recovers_after_close_invalidation_failure);
    if (selected == "collapsed_fit")
      return test::run("breadcrumb_collapsed_fit", &failed_fit_is_discarded_when_collapsed_frame_skips_layout);
    if (selected == "lazy_pointer")
      return test::run("breadcrumb_lazy_pointer", &lazy_fit_routes_the_first_pointer_to_new_overflow);
    if (selected == "lazy_fit")
      return test::run("breadcrumb_lazy_fit", &lazy_fit_publishes_overflow_before_first_paint);
  }
  return test::run("widget_breadcrumbs", &suite);
}
