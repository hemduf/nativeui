#include "example_support.hpp"
#include <cmath>
#include <nativeui/checkbox.hpp>

namespace {
struct DemoState {
  ui::State<float> value{0.5f};
  ui::State<bool> flag{false};
  ui::State<std::string> text{"NativeUI"};
  ui::State<ui::RangeValue> range{{0.2f, 0.8f}};
  int calls{};
};

ui::UI make_ui(DemoState &state) {
  (void)state;
  return ui::UI{ui::Checkbox{state.flag, "Option"}};
}

int self_test() {
  DemoState state;
  auto tree = make_ui(state);
  example::Platform platform;
  tree.resize({320.0f, 180.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Space), platform);
  if (state.flag.get())
    return example::fail("Checkbox changed before release");
  auto release = example::key(ui::Key::Space);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  ui::HeadlessRenderer renderer{{320.0f, 180.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("headless checkbox render failed");
  if (!state.flag.get())
    return example::fail("Checkbox must change on release");
  const auto measured = tree.measure();
  if (!std::isfinite(measured.preferred.w) ||
      !std::isfinite(measured.preferred.h))
    return example::fail("checkbox produced non-finite preferred geometry");
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  DemoState state;
  auto tree = make_ui(state);
  return example::run_window(tree, "NativeUI / checkbox", {420.0f, 220.0f});
}
