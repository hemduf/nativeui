#include "example_support.hpp"
#include <nativeui/color_picker.hpp>
int main(int argc, char **argv) {
  ui::State<ui::Color> value{ui::Color{.12f, .34f, .56f, .43f}};
  ui::UI tree{ui::ColorPicker{"Accent", value}.swatches(
      {{"red", "Red", {1, 0, 0, 1}}, {"blue", "Blue", {0, 0, 1, 1}}})};
  if (example::self_test_requested(argc, argv)) {
    const auto initial = value.get();
    ui::HeadlessRenderer renderer{{280, 450}, 1};
    tree.resize({280, 450});
    return renderer.render(tree) && value.get() == initial
               ? 0
               : example::fail("Color render quantized the model");
  }
  return example::run_window(tree, "NativeUI / ColorPicker", {320, 480});
}
