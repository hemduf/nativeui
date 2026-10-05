#include "example_support.hpp"
#include <nativeui/breadcrumbs.hpp>
namespace {
int self_test() {
  ui::State<std::vector<ui::BreadcrumbItem>> path{
      std::vector<ui::BreadcrumbItem>{
          {"root", "Disk"}, {"home", "Ada"}, {"docs", "Documents"}}};
  std::string chosen;
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [&](const auto &key) { chosen = key; })};
  example::Platform platform;
  tree.resize({500, 50});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Space), platform);
  auto up = example::key(ui::Key::Space);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  if (chosen != "root")
    return example::fail("Breadcrumbs key navigation failed");
  ui::HeadlessRenderer renderer{{500, 50}, 1};
  return renderer.render(tree) ? 0
                               : example::fail("Breadcrumbs rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::vector<ui::BreadcrumbItem>> path{
      std::vector<ui::BreadcrumbItem>{{"root", "Disk"},
                                      {"home", "Ada"},
                                      {"projects", "Audio projects"},
                                      {"nativeui", "NativeUI"},
                                      {"docs", "Documentation"}}};
  auto model = path.binding();
  ui::UI tree{ui::Breadcrumbs{path}.label("Path").on_navigate(
      [model](const std::string &key) mutable {
        auto next = model.snapshot();
        const auto found =
            std::find_if(next.begin(), next.end(),
                         [&](const auto &item) { return item.key == key; });
        if (found != next.end()) {
          next.erase(found + 1, next.end());
          model.set(std::move(next));
        }
      })};
  return example::run_window(tree, "NativeUI / Breadcrumbs", {560, 90});
}
