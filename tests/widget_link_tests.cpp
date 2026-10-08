#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/link.hpp>
#include <nativeui/read_only.hpp>

#include <stdexcept>

namespace {
void navigation_release_repeats_and_throw_recovery() {
  int calls{};
  bool fail = true;
  std::string destination;
  ui::UI tree{ui::Link{"Help", "/help", [&](const std::string &value) {
                         ++calls;
                         destination = value;
                         if (fail)
                           throw std::runtime_error("injected navigation");
                       }}};
  test::MockPlatform platform;
  tree.resize({120.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f),
                platform);
  NUI_CHECK(calls == 0);
  bool caught = false;
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerUp, 10.0f, 10.0f),
                  platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1 && destination == "/help");
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 2);
  auto up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 3);
}
void readonly_navigation_wrap_and_semantics() {
  ui::State<bool> readonly{true};
  int calls{};
  ui::UI tree{
      ui::ReadOnly{readonly, ui::Link{"Help", "/help",
                                      [&](const std::string &) { ++calls; }}}};
  test::MockPlatform platform;
  tree.resize({120.0f, 40.0f});
  tree.activate(platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1);
  ui::UI wrapped{ui::Link{
      "A long help label with e\xcc\x81 and 👨‍👩‍👧‍👦",
      "/help",
      {}}.wrap()};
  const auto wide = wrapped.measure().preferred;
  const auto narrow =
      wrapped.measure(ui::Constraints::loose({60.0f, 500.0f})).preferred;
  NUI_CHECK(narrow.w <= 60.0f && narrow.h > wide.h);
  ui::HeadlessRenderer renderer{{60.0f, 160.0f}, 1.0f};
  NUI_CHECK(renderer.render(wrapped));
  ui::detail::LinkComponent projection{
      "Help", "/help", [](const std::string &) {}, {}, false};
  const auto info = projection.semantics();
  NUI_CHECK(info.role == ui::SemanticRole::Custom && info.name == "Help");
  NUI_CHECK(info.description == "/help");
  ui::detail::LinkComponent inert{"Text", "", {}, {}, false};
  NUI_CHECK(!inert.focusable() && inert.semantics().actions.empty());
}
void invalidation_throw_cancels_capture_and_recovers() {
  int calls{};
  ui::UI tree{ui::Link{"Help", "/help", [&](const std::string &) { ++calls; }}};
  test::MockPlatform platform;
  tree.resize({120, 40});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 40));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  tree.set_invalidation_callback(
      [](ui::Rect) { throw std::runtime_error("injected Link invalidation"); });
  bool caught{};
  try {
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  tree.clear_invalidation_callback();
  NUI_CHECK(caught && calls == 0);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 10, 10), platform);
  NUI_CHECK(calls == 1);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
void hover_leaves_to_sibling_restores_color() {
  ui::LinkStyle style;
  style.color = ui::Color{0, 0, 1, 1};
  style.hovered_color = ui::Color{1, 0, 0, 1};
  style.underline = true;
  style.focus_ring_width = 0.0;
  ui::UI tree{ui::Row{
      ui::Link{"Help", "/help", [](const std::string &) {}}.style(style),
      ui::Button{"Next", {}}}};
  test::MockPlatform platform;
  tree.resize({200, 40});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  NUI_CHECK(surface);
  auto colored_pixels = [&] {
    surface->getCanvas()->clear(SK_ColorBLACK);
    tree.paint(*surface->getCanvas(), platform);
    SkPixmap pixels;
    NUI_CHECK(surface->peekPixels(&pixels));
    std::pair<int, int> counts{};
    for (int y = 0; y < 40; ++y)
      for (int x = 0; x < 55; ++x) {
        const auto pixel = pixels.getColor(x, y);
        if (SkColorGetR(pixel) > 200 && SkColorGetB(pixel) < 30)
          ++counts.first;
        if (SkColorGetB(pixel) > 200 && SkColorGetR(pixel) < 30)
          ++counts.second;
      }
    return counts;
  };
  const auto normal = colored_pixels();
  NUI_CHECK(normal.first == 0 && normal.second > 1);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 10, 10), platform);
  const auto hovered = colored_pixels();
  NUI_CHECK(hovered.first > 1 && hovered.second == 0);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 70, 10), platform);
  const auto left = colored_pixels();
  NUI_CHECK(left.first == 0 && left.second == normal.second);
}
void enter_callback_throw_keeps_repeat_latched() {
  bool fail = true;
  int calls{};
  ui::UI tree{ui::Link{"Help", "/help", [&](const std::string &) {
                         ++calls;
                         if (fail)
                           throw std::runtime_error(
                               "injected Link Enter action");
                       }}};
  test::MockPlatform platform;
  tree.resize({120, 40});
  tree.activate(platform);
  bool caught{};
  try {
    tree.dispatch(test::key(ui::Key::Enter), platform);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  NUI_CHECK(caught && calls == 1);
  fail = false;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 1);
  auto up = test::key(ui::Key::Enter);
  up.type = ui::InputType::KeyUp;
  tree.dispatch(up, platform);
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(calls == 2);
}
void prepared_action_removed_by_invalidation_is_not_navigated() {
  ui::State<bool> present{true};
  int calls{};
  ui::UI tree{ui::If{present, ui::Link{"Help", "/help",
                                       [&](const std::string &) { ++calls; }}}};
  test::MockPlatform platform;
  tree.resize({120, 40});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 40));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  bool remove = true;
  tree.set_invalidation_callback([&](ui::Rect) {
    if (!remove)
      return;
    remove = false;
    present.set(false);
    tree.resize({121, 40});
  });
  tree.dispatch(test::key(ui::Key::Enter), platform);
  tree.clear_invalidation_callback();
  NUI_CHECK(!present.get());
  NUI_CHECK(calls == 0);
}
struct CopyActionState {
  bool armed{};
  int calls{};
  int destroyed{};
  int retire_calls{};
  std::function<void()> retire;
};
struct CopyNavigation {
  std::shared_ptr<CopyActionState> state;
  explicit CopyNavigation(std::shared_ptr<CopyActionState> value)
      : state(std::move(value)) {}
  CopyNavigation(const CopyNavigation &other) : state(other.state) {
    if (state->armed) {
      state->armed = false;
      ++state->retire_calls;
      auto retire = state->retire;
      if (retire)
        retire();
    }
  }
  CopyNavigation(CopyNavigation &&) = default;
  void operator()(const std::string &) const { ++state->calls; }
};
class DestructionSentinel final : public ui::Component {
public:
  explicit DestructionSentinel(std::shared_ptr<CopyActionState> value)
      : state_(std::move(value)) {}
  ~DestructionSentinel() override { ++state_->destroyed; }
  ui::Size
  measure(const std::vector<ui::ChildMetrics> &children) const override {
    return children.empty() ? ui::Size{} : children.front().preferred;
  }
  void paint(ui::PaintContext &) const override {}

private:
  std::shared_ptr<CopyActionState> state_;
};
void callback_copy_removal_has_no_member_access_after_copy() {
  ui::State<bool> present{true};
  auto state = std::make_shared<CopyActionState>();
  ui::Spec content{
      [state] { return std::make_unique<DestructionSentinel>(state); },
      {ui::make_spec(
          ui::Link{"Help", std::string(200, 'x'), CopyNavigation{state}})}};
  ui::UI tree{ui::If{present, std::move(content)}};
  test::MockPlatform platform;
  tree.resize({120, 40});
  tree.activate(platform);
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 40));
  NUI_CHECK(surface);
  tree.paint(*surface->getCanvas(), platform);
  state->retire = [&] {
    present.set(false);
    tree.resize({121, 40});
  };
  state->armed = true;
  tree.dispatch(test::key(ui::Key::Enter), platform);
  NUI_CHECK(state->retire_calls == 1 && state->destroyed == 1);
  NUI_CHECK(state->calls == 0 && !present.get());
  state->retire = {};
}
void suite() {
  callback_copy_removal_has_no_member_access_after_copy();
  enter_callback_throw_keeps_repeat_latched();
  prepared_action_removed_by_invalidation_is_not_navigated();
  hover_leaves_to_sibling_restores_color();
  invalidation_throw_cancels_capture_and_recovers();
  navigation_release_repeats_and_throw_recovery();
  readonly_navigation_wrap_and_semantics();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "copy")
    return test::run("widget_link_copy",
                     &callback_copy_removal_has_no_member_access_after_copy);
  if (argc == 2 && std::string_view{argv[1]} == "enter")
    return test::run("widget_link_enter",
                     &enter_callback_throw_keeps_repeat_latched);
  if (argc == 2 && std::string_view{argv[1]} == "remove")
    return test::run("widget_link_remove",
                     &prepared_action_removed_by_invalidation_is_not_navigated);
  if (argc == 2 && std::string_view{argv[1]} == "hover")
    return test::run("widget_link_hover",
                     &hover_leaves_to_sibling_restores_color);
  return test::run("widget_link", &suite);
}
