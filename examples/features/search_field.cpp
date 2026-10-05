#include "example_support.hpp"
#include <nativeui/search_field.hpp>
namespace {
int self_test() {
  ui::State<std::string> query{"presets"};
  int submissions{};
  ui::UI tree{ui::SearchField{"Chercher", query}.on_submit(
      [&](const auto &) { ++submissions; })};
  example::Platform platform;
  tree.resize({480, 80});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (submissions != 1)
    return example::fail("SearchField did not submit");
  tree.dispatch(example::key(ui::Key::Escape), platform);
  if (!query.get().empty())
    return example::fail("SearchField did not clear");
  ui::HeadlessRenderer renderer{{480, 80}, 1};
  return renderer.render(tree) ? 0
                               : example::fail("SearchField rendering failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::State<std::string> query{};
  ui::UI tree{ui::SearchField{"Chercher dans les presets", query}.placeholder(
      "Rechercher")};
  return example::run_window(tree, "NativeUI / SearchField", {480, 110});
}
