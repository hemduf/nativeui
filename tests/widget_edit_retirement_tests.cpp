#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include "test_support.hpp"
#include <nativeui/if.hpp>

#include <string_view>

namespace ui {
struct TreeTestAccess {
  static bool captured(Tree &tree, PointerId id) {
    return tree.pointer_capture_owner(id) != nullptr;
  }
  static Point mouse_position(Tree &tree) {
    const auto locate = [](auto &&self, Node &node) -> Point {
      if (!node.component->focusable() && node.children.empty())
        return {node.bounds.x + node.bounds.w / 2,
                node.bounds.y + node.bounds.h / 2};
      for (auto &child : node.children) {
        auto point = self(self, *child);
        if (point.x >= 0)
          return point;
      }
      return {-1, -1};
    };
    return locate(locate, *tree.root_);
  }
};
} // namespace ui

namespace {
struct CallbackFailure final : std::runtime_error {
  using std::runtime_error::runtime_error;
};
template <class T>
ui::EditCallbacks<T> callbacks(std::function<void(int)> notify) {
  return {[notify](ui::EditSource) { notify(0); },
          [notify](const T &, ui::EditSource) { notify(1); },
          [notify](ui::EditSource) { notify(2); },
          [notify](ui::EditSource) { notify(3); }};
}
void retired_callback_suppresses_stale_work_and_remount_recovers() {
  for (const int kind : {0, 1, 2}) {
    for (const int phase : {0, 1, 2}) {
      for (const bool fail : {false, true}) {
        ui::State<float> value{0.5f};
        ui::State<bool> toggled{false}, present{true};
        test::MockPlatform platform;
        ui::UI *owner{};
        std::string trace;
        bool armed = true;
        auto notify = [&](int current) {
          trace += "BCEX"[current];
          if (current != phase || !std::exchange(armed, false))
            return;
          present.set(false);
          owner->resize({201, 60});
          if (fail)
            throw CallbackFailure{"retired edit callback"};
        };
        auto control =
            kind == 0   ? ui::make_spec(ui::Knob{"Value", value}.on_edit(
                              callbacks<float>(notify)))
            : kind == 1 ? ui::make_spec(ui::Slider{value}.on_edit(
                              callbacks<float>(notify)))
                        : ui::make_spec(ui::Toggle{"Switch", toggled}.on_edit(
                              callbacks<bool>(notify)));
        ui::UI tree{ui::If{present, std::move(control)}};
        owner = &tree;
        tree.resize({200, 60});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{200, 60}, 1};
        NUI_CHECK(renderer.render(tree));
        bool caught{};
        try {
          tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30),
                        platform);
          if (present.get())
            tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10),
                          platform);
          if (present.get())
            tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10),
                          platform);
        } catch (const CallbackFailure &) {
          caught = true;
        }
        NUI_CHECK(caught == fail && !armed && !present.get());
        NUI_CHECK(trace == (phase == 0 ? "BX" : phase == 1 ? "BCX" : "BCE"));
        NUI_CHECK(platform.pointer_capture_begin_count ==
                  platform.pointer_capture_end_count);
        const auto committed = value.get();
        const auto previous_trace = trace;
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 140, 0),
                      platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 140, 0),
                      platform);
        NUI_CHECK(value.get() == committed && trace == previous_trace);
        value.set(0.5f);
        toggled.set(false);
        trace.clear();
        present.set(true);
        tree.resize({200, 60});
        NUI_CHECK(renderer.render(tree));
        tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30),
                      platform);
        tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10),
                      platform);
        tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10),
                      platform);
        NUI_CHECK(trace == "BCE");
        NUI_CHECK(kind == 2 ? toggled.get() : value.get() > 0.5f);
        tree.deactivate(platform);
        NUI_CHECK(platform.pointer_capture_begin_count ==
                  platform.pointer_capture_end_count);
      }
    }
  }
}
void external_knob_write_cancels_the_active_edit() {
  ui::State<float> value{0.5f};
  test::MockPlatform platform;
  std::string trace;
  ui::UI tree{ui::Knob{"Value", value}.on_edit(
      callbacks<float>([&](int phase) { trace += "BCEX"[phase]; }))};
  tree.resize({200, 60});
  tree.activate(platform);
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
  NUI_CHECK(trace == "B");
  value.set(0.7f);
  NUI_CHECK(trace == "BX");
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 30), platform);
  NUI_CHECK(trace == "BX" && value.get() == 0.7f);
  trace.clear();
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerMove, 120, 10), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 120, 10), platform);
  NUI_CHECK(trace == "BCE" && value.get() > 0.7f);
  tree.deactivate(platform);
  NUI_CHECK(platform.pointer_capture_begin_count ==
            platform.pointer_capture_end_count);
}
class MouseCapture final : public ui::Component {
public:
  explicit MouseCapture(int &cancels) : cancels_(cancels) {}
  bool pointer_targetable() const noexcept override { return true; }
  ui::Size measure(const std::vector<ui::ChildMetrics> &) const override {
    return {200, 60};
  }
  ui::EventResult input(const ui::InputEvent &event,
                        ui::InputContext &context) override {
    if (event.type == ui::InputType::PointerDown)
      context.capture_pointer();
    if (event.type == ui::InputType::PointerUp ||
        event.type == ui::InputType::PointerCancel) {
      if (event.type == ui::InputType::PointerCancel)
        ++cancels_;
      context.release_pointer();
    }
    return ui::EventResult::Handled;
  }
  void paint(ui::PaintContext &) const override {}

private:
  int &cancels_;
};
void keyboard_cleanup_releases_originating_touch_only(bool toggle, bool knob = false) {
  for (int mode = 0; mode < (toggle ? 1 : 3); ++mode) {
    ui::State<float> value{0.5f};
    ui::State<bool> checked{false}, read_only{false};
    test::MockPlatform platform;
    bool fail{};
    auto notify = [&](int phase) {
      if (fail && phase == 0)
        throw CallbackFailure{"keyboard begin"};
    };
    ui::SliderStyle slider_style;
    slider_style.pressed.thumb = ui::Color{1, 0, 0, 1};
    auto control = toggle
                       ? ui::make_spec(ui::Toggle{"Switch", checked}.on_edit(
                             callbacks<bool>(notify)))
                       : knob ? ui::make_spec(ui::Knob{"Value", value}.on_edit(
                                    callbacks<float>(notify)))
                              : ui::make_spec(ui::Slider{value}
                                                  .style(slider_style)
                                                  .on_edit(callbacks<float>(notify)));
    int mouse_cancels{};
    ui::Spec mouse{
        [&] { return std::make_unique<MouseCapture>(mouse_cancels); }, {}};
    ui::Tree tree{ui::compile(ui::make_spec(ui::Row{
        ui::ReadOnly{read_only, std::move(control)}, std::move(mouse)}))};
    tree.mount();
    tree.layout({400, 60});
    tree.activate_focus(platform);
    auto touch = test::pointer(ui::InputType::PointerDown, 100, 30);
    touch.pointer.id = 7;
    touch.pointer.type = ui::PointerType::Touch;
    tree.dispatch(touch, platform);
    NUI_CHECK(ui::TreeTestAccess::captured(tree, 7));
    const auto mouse_position = ui::TreeTestAccess::mouse_position(tree);
    NUI_CHECK(mouse_position.x >= 0);
    tree.dispatch(test::pointer(ui::InputType::PointerDown, mouse_position.x,
                                mouse_position.y),
                  platform);
    NUI_CHECK(ui::TreeTestAccess::captured(tree, 0));
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 60));
    NUI_CHECK(surface);
    tree.paint(*surface->getCanvas(), platform);
    bool caught{}, inject{};
    if (toggle)
      fail = true;
    else if (mode == 1)
      read_only.set(true);
    else if (mode == 2) {
      tree.set_invalidation_callback([&](ui::Rect) {
        if (std::exchange(inject, false))
          throw CallbackFailure{"keyboard invalidation"};
      });
      // Installing a handler immediately reports already dirty damage; arm the
      // one-shot fault only afterward, for the keyboard cancellation itself.
      inject = true;
    }
    try {
      tree.dispatch(test::key(toggle      ? ui::Key::Space
                              : mode == 1 ? ui::Key::Right
                                          : ui::Key::Escape),
                    platform);
    } catch (const CallbackFailure &) {
      caught = true;
    }
    tree.set_invalidation_callback(std::function<void(ui::Rect)>{});
    NUI_CHECK(caught == (toggle || mode == 2));
    NUI_CHECK(!ui::TreeTestAccess::captured(tree, 7));
    NUI_CHECK(ui::TreeTestAccess::captured(tree, 0) && mouse_cancels == 0);
    fail = false;
    read_only.set(false);
    tree.dispatch(test::pointer(ui::InputType::PointerUp, mouse_position.x,
                                mouse_position.y),
                  platform);
    NUI_CHECK(!ui::TreeTestAccess::captured(tree, 0));
    const auto before = toggle ? float(checked.get()) : value.get();
    tree.dispatch(touch, platform);
    auto move = touch;
    move.type = ui::InputType::PointerMove;
    move.position = {120, 10};
    tree.dispatch(move, platform);
    move.type = ui::InputType::PointerUp;
    tree.dispatch(move, platform);
    NUI_CHECK((toggle ? float(checked.get()) : value.get()) != before);
    NUI_CHECK(!ui::TreeTestAccess::captured(tree, 7));
    tree.deactivate_focus(platform);
  }
}
void slider_contacts() {
  keyboard_cleanup_releases_originating_touch_only(false);
}
void toggle_contacts() {
  keyboard_cleanup_releases_originating_touch_only(true);
}
void knob_contacts() {
  keyboard_cleanup_releases_originating_touch_only(false, true);
}
void toggle_layout_invalidation_can_retire_before_paint_invalidation() {
  ui::State<bool> present{true}, checked{false};
  test::MockPlatform platform;
  ui::Component *component{};
  ui::ToggleStyle style{};
  style.pressed.control_width = 220;
  auto spec = ui::Toggle{"Switch", checked}.style(style).spec();
  auto factory = spec.factory;
  spec.factory = [&] {
    auto result = factory();
    component = result.get();
    return result;
  };
  ui::UI tree{ui::If{present, std::move(spec)}};
  tree.resize({200, 60});
  tree.activate(platform);
  ui::InputContext context{{0, 0, 200, 60},
                           platform,
                           [&] { NUI_CHECK(present.get()); },
                           [&] {
                             present.set(false);
                             tree.resize({201, 60});
                           },
                           [] {},
                           [] {}};
  NUI_CHECK(component->input(test::pointer(ui::InputType::PointerDown, 100, 30),
                             context) == ui::EventResult::Handled);
  NUI_CHECK(!present.get() && !checked.get());
  present.set(true);
  tree.resize({200, 60});
  tree.dispatch(test::pointer(ui::InputType::PointerDown, 100, 30), platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, 100, 30), platform);
  NUI_CHECK(checked.get());
  tree.deactivate(platform);
}
void suite() {
  retired_callback_suppresses_stale_work_and_remount_recovers();
  external_knob_write_cancels_the_active_edit();
  slider_contacts();
  toggle_contacts();
  knob_contacts();
  toggle_layout_invalidation_can_retire_before_paint_invalidation();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view{argv[1]} == "slider_contacts")
    return test::run("slider contacts", slider_contacts);
  if (argc == 2 && std::string_view{argv[1]} == "toggle_contacts")
    return test::run("toggle contacts", toggle_contacts);
  if (argc == 2 && std::string_view{argv[1]} == "knob_contacts")
    return test::run("knob contacts", knob_contacts);
  if (argc == 2 && std::string_view{argv[1]} == "toggle_invalidation")
    return test::run(
        "toggle invalidation",
        toggle_layout_invalidation_can_retire_before_paint_invalidation);
  return test::run("widget_edit_retirement", suite);
}
