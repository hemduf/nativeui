#include "example_support.hpp"
#include <nativeui/segmented_control.hpp>
namespace {
struct View {
  int mode{};
  bool operator==(const View &) const = default;
};
std::vector<ui::SegmentOption<View>> options() {
  return {{{0}, "Liste", true}, {{1}, "Grille", true}, {{2}, "Colonnes", true}};
}
int self_test() {
  ui::State<View> view{View{0}};
  int notifications{};
  auto subscription = view.observe([&](const View &) { ++notifications; });
  ui::UI tree{ui::SegmentedControl{"Vue", view, options()}};
  example::Platform platform;
  tree.resize({400, 70});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Right), platform);
  if (view.get().mode != 1 || notifications != 1)
    return example::fail(
        "SegmentedControl navigation did not select generic value");
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (notifications != 1)
    return example::fail("SegmentedControl republished current value");
  ui::HeadlessRenderer renderer{{400, 70}, 1};
  return renderer.render(tree)
             ? 0
             : example::fail("SegmentedControl render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<View> view{View{0}};
  ui::UI tree{ui::SegmentedControl{"Vue", view, options()}};
  return example::run_window(tree, "NativeUI / SegmentedControl", {440, 90});
}
