#include "test_support.hpp"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include <nativeui/rich_text.hpp>

#include <stdexcept>
#include <string_view>

namespace ui {
struct TreeTestAccess {
  static bool has_contact(Tree& tree) { return tree.pointer_capture_owner(17) != nullptr; }
};
} // namespace ui

namespace {
ui::InputEvent touch(ui::InputType type) {
  auto result = test::pointer(type, 10, 10);
  result.pointer.id = 17;
  result.pointer.type = ui::PointerType::Touch;
  return result;
}
void newer_contact_survives_older_invalidation(bool fail) {
  int calls{};
  ui::Tree tree{ui::compile(ui::make_spec(ui::RichText{
      std::vector<ui::RichTextSpan>{{.id = "action", .text = "Action",
                                    .on_activate = [&] { ++calls; }}}}))};
  test::MockPlatform platform;
  tree.mount();
  tree.layout({120, 70});
  tree.activate_focus(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 70));
  NUI_CHECK(surface);
  // Consume all pre-existing geometry/focus damage before arming exposure.
  tree.paint(*surface->getCanvas(), platform);
  tree.paint(*surface->getCanvas(), platform);
  bool armed{};
  bool nested{};
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!std::exchange(armed, false)) return;
    tree.dispatch(touch(ui::InputType::PointerDown), platform);
    nested = true;
    NUI_CHECK(ui::TreeTestAccess::has_contact(tree));
    if (fail) throw std::runtime_error("older RichText invalidation");
  });
  armed = true;
  bool caught{};
  try {
    tree.dispatch(touch(ui::InputType::PointerDown), platform);
  } catch (const std::runtime_error& error) {
    NUI_CHECK(std::string_view{error.what()} == "older RichText invalidation");
    caught = true;
  }
  tree.set_invalidation_callback(std::function<void(ui::Rect)>{});
  NUI_CHECK(nested && caught == fail && calls == 0);
  // The new dispatch owns Touch17. Old unwind/completion cannot retire it.
  NUI_CHECK(ui::TreeTestAccess::has_contact(tree));
  tree.dispatch(touch(ui::InputType::PointerUp), platform);
  NUI_CHECK(calls == 1 && !ui::TreeTestAccess::has_contact(tree));
  tree.dispatch(touch(ui::InputType::PointerDown), platform);
  NUI_CHECK(ui::TreeTestAccess::has_contact(tree));
  tree.dispatch(touch(ui::InputType::PointerUp), platform);
  NUI_CHECK(calls == 2 && !ui::TreeTestAccess::has_contact(tree));
  tree.deactivate_focus(platform);
  tree.unmount();
  NUI_CHECK(platform.pointer_capture_begin_count == platform.pointer_capture_end_count);
}
void success() { newer_contact_survives_older_invalidation(false); }
void failure() { newer_contact_survives_older_invalidation(true); }
void suite() { success(); failure(); }
} // namespace
int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "nested_success") return test::run("rich_text_nested_success", &success);
  if (mode == "nested_failure") return test::run("rich_text_nested_failure", &failure);
  if (mode == "all") return test::run("rich_text_contact_generation", &suite);
  return 2;
}
