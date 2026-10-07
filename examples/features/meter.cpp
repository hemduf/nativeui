#include "example_support.hpp"
#include <cmath>
#include <nativeui/meter.hpp>

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
  return ui::UI{ui::Meter{state.value.binding()}.levels({0.5, 0.75})};
}

int self_test() {
  DemoState state;
  auto tree = make_ui(state);
  example::Platform platform;
  tree.resize({320.0f, 180.0f});
  tree.activate(platform);
  state.value.set(0.8f);
  ui::HeadlessRenderer renderer{{320.0f, 180.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("headless meter render failed");
  if (!example::near(state.value.get(), 0.8f))
    return example::fail("Meter rewrote its model");
  const auto measured = tree.measure();
  if (!std::isfinite(measured.preferred.w) ||
      !std::isfinite(measured.preferred.h))
    return example::fail("meter produced non-finite preferred geometry");
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  DemoState state;
  auto tree = make_ui(state);
  return example::run_window(tree, "NativeUI / meter", {420.0f, 220.0f});
}
