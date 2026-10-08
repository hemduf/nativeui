#include "example_support.hpp"
#include <nativeui/image_view.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace {
ui::SvgIcon prepared_image() {
  constexpr std::string_view svg =
      R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20" viewBox="0 0 40 20"><rect width="40" height="20" fill="#e88030"/><circle cx="20" cy="10" r="7" fill="#ffffff"/></svg>)svg";
  return ui::SvgIcon::parse(
      {reinterpret_cast<const std::byte *>(svg.data()), svg.size()});
}
ui::UI make_ui(ui::SvgIcon image) {
  return ui::UI{ui::ImageView{std::move(image)}
                    .fit(ui::ImageFit::Cover)
                    .size({96.0f, 64.0f})
                    .alt("Prepared document illustration")
                    .decorative(false)};
}
int self_test() {
  const auto image = prepared_image();
  if (!image.valid())
    return example::fail("SVG preparation failed");
  auto tree = make_ui(image);
  ui::HeadlessRenderer renderer{{96.0f, 64.0f}, 1.0f};
  if (!renderer.render(tree))
    return example::fail("ImageView headless render failed");
  if (!example::near(tree.measure().preferred.w, 96.0f))
    return example::fail("ImageView explicit size failed");
  const auto center = renderer.pixel(48, 32);
  if (center.r < 240 || center.g < 240 || center.b < 240)
    return example::fail("ImageView did not preserve the prepared SVG colors");
  return 0;
}
} // namespace
int main(int argc, char **argv) {
  if (example::self_test_requested(argc, argv))
    return self_test();
  auto tree = make_ui(prepared_image());
  return example::run_window(tree, "NativeUI / ImageView", {240.0f, 180.0f});
}
