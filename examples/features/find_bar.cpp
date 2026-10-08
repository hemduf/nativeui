#include "example_support.hpp"
#include <nativeui/find_bar.hpp>
namespace {
int self_test() {
  ui::State<bool> open{true};
  ui::State<std::string> query{"gain"};
  ui::State<std::size_t> matches{3};
  ui::State<std::optional<std::size_t>> current{};
  int navigations{};
  ui::UI tree{ui::FindBar{open, query, matches, current}.on_navigate(
      [&](auto) { ++navigations; })};
  example::Platform platform;
  tree.resize({800, 80});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (current.get() != 0 || navigations != 1)
    return example::fail("FindBar did not choose its first match");
  tree.dispatch(example::key(ui::Key::Enter, true), platform);
  if (current.get() != 2)
    return example::fail("FindBar did not wrap backwards");
  tree.dispatch(example::key(ui::Key::Escape), platform);
  if (open.get() || query.get() != "gain")
    return example::fail("FindBar close changed the query");
  return tree.measure().preferred.w == 0
             ? 0
             : example::fail("Closed FindBar remained in layout");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<bool> open{true};
  ui::State<std::string> query{"gain"};
  ui::State<std::size_t> matches{3};
  ui::State<std::optional<std::size_t>> current{0};
  ui::UI tree{ui::FindBar{open, query, matches, current}};
  return example::run_window(tree, "NativeUI / FindBar", {800, 110});
}
