#include "example_support.hpp"
#include <nativeui/color_well.hpp>
int main(int argc, char **argv) {
  ui::State<ui::Color> value{ui::Color{.2f, .4f, .8f, .5f}};
  ui::UI tree{ui::ColorWell{"Couleur de piste", value}};
  if (example::self_test_requested(argc, argv)) {
    example::Platform platform;
    tree.resize({500, 480});
    tree.activate(platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    tree.dispatch(example::key(ui::Key::Escape), platform);
    return tree.overlay_entries().empty() &&
                   value.get() == ui::Color({.2f, .4f, .8f, .5f})
               ? 0
               : example::fail("ColorWell cancellation failed");
  }
  return example::run_window(tree, "NativeUI / ColorWell", {500, 480});
}
