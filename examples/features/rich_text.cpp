#include "example_support.hpp"
#include <nativeui/rich_text.hpp>
namespace {
std::vector<ui::RichTextSpan> spans(std::function<void()> action) {
  ui::TextStyle accent;
  accent.size = 18;
  accent.color = {.1f, .35f, .75f, 1};
  return {{.text = "Read "},
          {.id = "guide",
           .text = "the guide",
           .style = accent,
           .background = ui::Color{.9f, .95f, 1, 1},
           .underline = true,
           .on_activate = std::move(action)},
          {.text = " before you begin. office / العربية / A\xcc\x81 / "
                   "👨‍👩‍👧‍👦\nThe text wraps as a single "
                   "paragraph."}};
}
int self_test() {
  int calls{};
  ui::UI tree{ui::RichText{spans([&] { ++calls; })}};
  example::Platform platform;
  tree.resize({240, 140});
  tree.activate(platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (calls != 1)
    return example::fail("RichText Enter repeated an action");
  auto release = example::key(ui::Key::Enter);
  release.type = ui::InputType::KeyUp;
  tree.dispatch(release, platform);
  tree.dispatch(example::key(ui::Key::Enter), platform);
  if (calls != 2)
    return example::fail("RichText action did not recover after release");
  const auto wide = tree.measure(ui::Constraints::loose({500, 500})).preferred;
  const auto narrow = tree.measure(ui::Constraints::loose({90, 500})).preferred;
  if (narrow.w > 90 || narrow.h <= wide.h)
    return example::fail("RichText paragraph did not reflow");
  ui::HeadlessRenderer renderer{{240, 140}, 1};
  return renderer.render(tree) ? 0 : example::fail("RichText render failed");
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::UI tree{ui::RichText{spans([] {})}};
  return example::run_window(tree, "NativeUI / RichText", {440, 220});
}
