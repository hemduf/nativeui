#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/knob.hpp>

#include <stdexcept>
#include <string_view>

namespace ui {
struct TreeTestAccess {
  static bool has_contact(Tree &tree) {
    return tree.pointer_capture_owner(17) != nullptr;
  }
};
} // namespace ui

namespace {
void pointer_observer_throw_ends_gesture() {
  ui::State<float> value{0.5f};
  bool throw_next = true;
  int notifications{};
  auto subscription = value.observe([&](float) {
    ++notifications;
    if (throw_next)
      throw std::runtime_error("injected knob observer failure");
  });
  ui::UI tree{ui::Knob{"Value", value}};
  test::MockPlatform platform;
  tree.resize({176.0f, 182.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                platform);
  bool caught = false;
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 82.0f),
                  platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && notifications == 1);
  NUI_CHECK_NEAR(value.get(), 0.6f, 0.0001f);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);

  throw_next = false;
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 82.0f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 80.0f, 82.0f),
                platform);
  NUI_CHECK_NEAR(value.get(), 0.7f, 0.0001f);
  NUI_CHECK(notifications == 2);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void newer_contact_survives_older_failure() {
  ui::State<float> value{0.5f};
  ui::UI *owner{};
  test::MockPlatform platform;
  bool nested = false;
  int notifications{};
  auto subscription = value.observe([&](float) {
    ++notifications;
    if (!nested) {
      nested = true;
      owner->dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                      platform);
      throw std::runtime_error("outer contact observer failure");
    }
  });
  ui::UI tree{ui::Knob{"Value", value}};
  owner = &tree;
  tree.resize({176.0f, 182.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                platform);
  bool caught = false;
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 82.0f),
                  platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && notifications == 1);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count + 1);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 80.0f, 82.0f),
                platform);
  NUI_CHECK_NEAR(value.get(), 0.7f, 0.0001f);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 80.0f, 82.0f),
                platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void focus_throw_releases_before_invalidation() {
  ui::State<float> value{0.5f};
  ui::UI tree{ui::Row{ui::Knob{"Value", value}, ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({400.0f, 182.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 80.0f, 100.0f),
                platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 182));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count + 1);
  tree.set_invalidation_callback([](ui::Rect) {
    throw std::runtime_error("injected focus invalidation");
  });
  bool caught = false;
  try {
    tree.dispatch(test::key(ui::Key::Tab), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  tree.clear_invalidation_callback();
  NUI_CHECK(caught);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.paint(*surface->getCanvas(), platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void dead_binding_key_releases_existing_contact() {
  auto value = std::make_unique<ui::State<float>>(0.5f);
  test::MockPlatform platform;
  ui::Tree tree{
      ui::compile(ui::make_spec(ui::Knob{"Value", value->binding()}))};
  tree.mount();
  tree.layout({176, 182});
  tree.activate_focus(platform);
  auto down = test::pointer(ui::InputType::PointerDown, 20, 20);
  down.pointer.id = 17;
  down.pointer.type = ui::PointerType::Touch;
  tree.dispatch(down, platform);
  auto has_capture = [&] { return ui::TreeTestAccess::has_contact(tree); };
  NUI_CHECK(has_capture());
  value.reset();
  tree.dispatch(test::key(ui::Key::Right), platform);
  NUI_CHECK(!has_capture());
  tree.deactivate_focus(platform);
}
void suite() {
  focus_throw_releases_before_invalidation();
  dead_binding_key_releases_existing_contact();
  pointer_observer_throw_ends_gesture();
  newer_contact_survives_older_failure();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "newer_contact")
    return test::run("knob newer contact", newer_contact_survives_older_failure);
  return test::run("widget_knob_lifetime", &suite);
}
