#include "example_support.hpp"
#include <cmath>
#include <nativeui/range_slider.hpp>

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
  return ui::UI{ui::RangeSlider{state.range}.step(0.1f)};
}

int self_test() {
  DemoState state;
  auto tree = make_ui(state);
  example::Platform platform;
  tree.resize({320.0f, 180.0f});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  tree.dispatch(example::key(ui::Key::Left), platform);
  ui::HeadlessRenderer renderer{{320.0f, 180.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("headless range_slider render failed");
  if (!example::near(state.range.get().low, 0.2f) ||
      !example::near(state.range.get().high, 0.7f))
    return example::fail("RangeSlider active-handle keyboard editing failed");
  const auto measured = tree.measure();
  if (!std::isfinite(measured.preferred.w) ||
      !std::isfinite(measured.preferred.h))
    return example::fail("range_slider produced non-finite preferred geometry");
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  DemoState state;
  auto tree = make_ui(state);
  return example::run_window(tree, "NativeUI / range_slider", {420.0f, 220.0f});
}
