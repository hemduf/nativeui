#include "example_support.hpp"
#include <cmath>
#include <nativeui/progress_bar.hpp>

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
  return ui::UI{ui::ProgressBar{state.value.binding()}.reversed()};
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
    return example::fail("headless progress_bar render failed");
  if (!example::near(state.value.get(), 0.8f))
    return example::fail("ProgressBar rewrote its model");
  int formatted{};
  ui::UI activity{
      ui::ProgressBar{state.value}.indeterminate().reduced_motion().formatter(
          [&](float) {
            ++formatted;
            return "unused";
          })};
  if (!renderer.render(activity))
    return example::fail("ProgressBar activity render failed");
  if (formatted != 0)
    return example::fail(
        "Indeterminate activity called its numerical formatter");
  const auto measured = tree.measure();
  if (!std::isfinite(measured.preferred.w) ||
      !std::isfinite(measured.preferred.h))
    return example::fail("progress_bar produced non-finite preferred geometry");
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  DemoState state;
  auto tree = make_ui(state);
  return example::run_window(tree, "NativeUI / progress_bar", {420.0f, 220.0f});
}
