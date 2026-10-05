#include "example_support.hpp"
#include <nativeui/icon_view.hpp>

namespace {
ui::SvgIcon icon() {
  constexpr std::string_view source =
      R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24"><circle cx="12" cy="12" r="10" fill="red"/></svg>)";
  return ui::SvgIcon::parse(
      {reinterpret_cast<const std::byte *>(source.data()), source.size()});
}
int self_test() {
  ui::UI tree{ui::IconView{icon()}.size(16.0).color({0, 1, 0, 1}).alt("Saved")};
  ui::HeadlessRenderer renderer{{16.0f, 16.0f}, 1.0f};
  if (!renderer.render(tree) || renderer.pixel(8, 8).g < 240)
    return example::fail("IconView did not tint the SVG");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  ui::UI tree{ui::IconView{icon()}.size(48.0).alt("Saved")};
  return example::run_window(tree, "NativeUI / IconView", {120, 90});
}
