#include "test_support.hpp"

#include <array>
#include <functional>
#include <stdexcept>
#include <string_view>

namespace {

using InputCallback =
    std::function<ui::EventResult(const ui::InputEvent &, ui::InputContext &)>;

class InputProbe final : public ui::Component {
public:
  explicit InputProbe(InputCallback callback)
      : callback_(std::move(callback)) {}
  [[nodiscard]] bool pointer_targetable() const noexcept override {
    return true;
  }
  [[nodiscard]] ui::Size
  measure(const std::vector<ui::ChildMetrics> &) const override {
    return {100.0f, 100.0f};
  }
  void
  layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics> &,
                  std::vector<ui::ChildPlacement> &placements) const override {
    for (auto &placement : placements)
      placement.bounds = bounds;
  }
  ui::EventResult input(const ui::InputEvent &event,
                        ui::InputContext &context) override {
    return callback_(event, context);
  }
  void paint(ui::PaintContext &) const override {}

private:
  InputCallback callback_;
};

ui::Spec probe(InputCallback callback, std::vector<ui::Spec> children = {}) {
  return ui::Spec{[callback = std::move(callback)] {
                    return std::make_unique<InputProbe>(callback);
                  },
                  std::move(children)};
}

ui::InputEvent magnify(float x, float y, float factor = 0.1f) {
  auto event = test::pointer(ui::InputType::Magnify, x, y);
  event.pointer.type = ui::PointerType::Mouse;
  event.magnification = factor;
  event.ctrl = true;
  return event;
}

void canvas_coordinates() {
  test::MockPlatform platform;
  ui::Point wheel_position{};
  ui::Point zoom_position{};
  float factor{};
  bool ctrl{};
  ui::UI tree{ui::Padding{
      40.0f,
      ui::Canvas{ui::Size{120.0f, 120.0f}, [](ui::CanvasContext2D &) {}}
          .on_input([&](const ui::InputEvent &event, ui::CanvasInputContext &) {
            if (event.type == ui::InputType::PointerWheel)
              wheel_position = event.position;
            if (event.type == ui::InputType::Magnify) {
              zoom_position = event.position;
              factor = event.magnification;
              ctrl = event.ctrl;
            }
            return ui::EventResult::Handled;
          })}};
  tree.resize({200.0f, 200.0f});
  tree.activate(platform);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerWheel, 60.0f, 70.0f),
                      platform);
  (void)tree.dispatch(magnify(60.0f, 70.0f), platform);
  NUI_CHECK_NEAR(wheel_position.x, 20.0f, 0.001f);
  NUI_CHECK_NEAR(wheel_position.y, 30.0f, 0.001f);
  NUI_CHECK_NEAR(zoom_position.x, wheel_position.x, 0.001f);
  NUI_CHECK_NEAR(zoom_position.y, wheel_position.y, 0.001f);
  NUI_CHECK_NEAR(factor, 0.1f, 0.0001f);
  NUI_CHECK(ctrl);
  (void)tree.dispatch(magnify(120.0f, 130.0f, -0.05f), platform);
  NUI_CHECK_NEAR(zoom_position.x, 80.0f, 0.001f);
  NUI_CHECK_NEAR(zoom_position.y, 90.0f, 0.001f);
  NUI_CHECK_NEAR(factor, -0.05f, 0.0001f);
}

void capture_permission() {
  test::MockPlatform platform;
  int moves{};
  ui::UI tree{ui::Canvas{ui::Size{100.0f, 100.0f}, [](ui::CanvasContext2D &) {}}
                  .on_input([&](const ui::InputEvent &event,
                                ui::CanvasInputContext &context) {
                    if (event.type == ui::InputType::Magnify ||
                        event.type == ui::InputType::PointerDown)
                      context.capture_pointer();
                    if (event.type == ui::InputType::PointerMove)
                      ++moves;
                    return ui::EventResult::Handled;
                  })};
  tree.resize({100.0f, 100.0f});
  tree.activate(platform);
  (void)tree.dispatch(magnify(20.0f, 30.0f), platform);
  NUI_CHECK(platform.pointer_capture_begin_count == 0);
  NUI_CHECK(
      tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f),
                    platform) == ui::EventResult::Ignored);
  NUI_CHECK(moves == 0);
  // The permission change is event-local: a normal contact still works.
  (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f),
                      platform);
  NUI_CHECK(platform.pointer_capture_begin_count == 1);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(moves == 1);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(platform.pointer_capture_end_count == 1);
}

void reentrant_capture_permission() {
  test::MockPlatform platform;
  ui::UI *owner{};
  ui::InputContext *outer_context{};
  int captures_inside_zoom{-1};
  ui::UI tree{
      probe([&](const ui::InputEvent &event, ui::InputContext &context) {
        if (event.type == ui::InputType::PointerDown) {
          outer_context = &context;
          (void)owner->dispatch(magnify(20.0f, 30.0f), platform);
          outer_context = nullptr;
          context.capture_pointer();
        }
        if (event.type == ui::InputType::Magnify) {
          // A permitted outer Down context must not bypass the current
          // Magnify frame's prohibition. The borrow never escapes the stack.
          outer_context->capture_pointer();
          captures_inside_zoom = platform.pointer_capture_begin_count;
        }
        return ui::EventResult::Handled;
      })};
  owner = &tree;
  tree.resize({100.0f, 100.0f});
  tree.activate(platform);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f),
                      platform);
  NUI_CHECK(captures_inside_zoom == 0);
  NUI_CHECK(platform.pointer_capture_begin_count == 1);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(platform.pointer_capture_end_count == 1);
}

void captured_drag_owner() {
  test::MockPlatform platform;
  int child_moves{}, ancestor_moves{};
  auto child =
      probe([&](const ui::InputEvent &event, ui::InputContext &context) {
        if (event.type == ui::InputType::PointerDown)
          context.capture_pointer();
        if (event.type == ui::InputType::PointerMove)
          ++child_moves;
        return event.type == ui::InputType::Magnify ? ui::EventResult::Ignored
                                                    : ui::EventResult::Handled;
      });
  ui::UI tree{probe(
      [&](const ui::InputEvent &event, ui::InputContext &context) {
        if (event.type == ui::InputType::Magnify)
          context.capture_pointer();
        if (event.type == ui::InputType::PointerMove)
          ++ancestor_moves;
        return ui::EventResult::Handled;
      },
      {std::move(child)})};
  tree.resize({100.0f, 100.0f});
  tree.activate(platform);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f),
                      platform);
  (void)tree.dispatch(magnify(20.0f, 30.0f), platform);
  NUI_CHECK(platform.pointer_capture_begin_count == 1);
  NUI_CHECK(platform.pointer_capture_end_count == 0);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(child_moves == 1 && ancestor_moves == 0);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(platform.pointer_capture_end_count == 1);
}

void nested_new_contact() {
  test::MockPlatform platform;
  ui::UI *owner{};
  bool nest{true};
  ui::UI tree{probe([&](const ui::InputEvent &event,
                        ui::InputContext &context) {
    if (event.type == ui::InputType::Magnify && std::exchange(nest, false)) {
      (void)owner->dispatch(
          test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f), platform);
      context.capture_pointer();
    }
    if (event.type == ui::InputType::PointerDown)
      context.capture_pointer();
    return ui::EventResult::Handled;
  })};
  owner = &tree;
  tree.resize({100.0f, 100.0f});
  tree.activate(platform);
  (void)tree.dispatch(magnify(20.0f, 30.0f), platform);
  // A real nested Down is a new frame with its own permission and token.
  NUI_CHECK(platform.pointer_capture_begin_count == 1);
  NUI_CHECK(platform.pointer_capture_end_count == 0);
  (void)tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                      platform);
  NUI_CHECK(platform.pointer_capture_end_count == 1);
}

void modal_absorption() {
  test::MockPlatform platform;
  int underlying_events{};
  ui::UI tree{probe([&](const ui::InputEvent &, ui::InputContext &) {
    ++underlying_events;
    return ui::EventResult::Handled;
  })};
  tree.resize({200.0f, 200.0f});
  tree.activate(platform);
  ui::OverlaySpec modal;
  modal.mode = ui::OverlayMode::Modal;
  modal.placement = ui::OverlayPlacement::Center;
  modal.content = ui::make_spec(ui::Spacer{20.0f, 20.0f});
  const auto handle = tree.show_overlay(std::move(modal));
  tree.resize({200.0f, 200.0f});
  NUI_CHECK(
      tree.dispatch(test::pointer(ui::InputType::PointerWheel, 10.0f, 10.0f),
                    platform) == ui::EventResult::Handled);
  NUI_CHECK(tree.dispatch(magnify(10.0f, 10.0f), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(underlying_events == 0 && handle.valid());
  NUI_CHECK(tree.close_overlay(handle));
  tree.resize({200.0f, 200.0f});
  NUI_CHECK(tree.dispatch(magnify(10.0f, 10.0f), platform) ==
            ui::EventResult::Handled);
  NUI_CHECK(underlying_events == 1);
}

void overlay_pointer_policy() {
  for (const auto policy :
       {ui::OverlayPointerPolicy::Normal, ui::OverlayPointerPolicy::Ignore}) {
    test::MockPlatform platform;
    int underlying_events{};
    ui::UI tree{probe([&](const ui::InputEvent &, ui::InputContext &) {
      ++underlying_events;
      return ui::EventResult::Handled;
    })};
    tree.resize({200.0f, 200.0f});
    tree.activate(platform);
    ui::OverlaySpec overlay;
    overlay.pointer_policy = policy;
    overlay.placement = ui::OverlayPlacement::Center;
    overlay.content = ui::make_spec(ui::Spacer{20.0f, 20.0f});
    const auto handle = tree.show_overlay(std::move(overlay));
    tree.resize({200.0f, 200.0f});
    NUI_CHECK(tree.dispatch(magnify(100.0f, 100.0f), platform) ==
              ui::EventResult::Handled);
    NUI_CHECK(underlying_events ==
              (policy == ui::OverlayPointerPolicy::Ignore ? 1 : 0));
    NUI_CHECK(tree.close_overlay(handle));
  }
}

void throwing_callback_recovery_and_instance_isolation() {
  test::MockPlatform first_platform, second_platform;
  bool fail{true};
  int second_moves{};
  auto first = std::make_unique<ui::UI>(
      probe([&](const ui::InputEvent &event, ui::InputContext &context) {
        if (event.type == ui::InputType::Magnify) {
          context.capture_pointer();
          if (fail)
            throw std::runtime_error("magnify callback fault");
        }
        if (event.type == ui::InputType::PointerDown)
          context.capture_pointer();
        return ui::EventResult::Handled;
      }));
  ui::UI second{
      probe([&](const ui::InputEvent &event, ui::InputContext &context) {
        if (event.type == ui::InputType::PointerDown)
          context.capture_pointer();
        if (event.type == ui::InputType::PointerMove)
          ++second_moves;
        return ui::EventResult::Handled;
      })};
  first->resize({100.0f, 100.0f});
  second.resize({100.0f, 100.0f});
  first->activate(first_platform);
  second.activate(second_platform);
  (void)second.dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f),
                        second_platform);
  bool caught{};
  try {
    (void)first->dispatch(magnify(20.0f, 30.0f), first_platform);
  } catch (const std::runtime_error &error) {
    NUI_CHECK(std::string_view{error.what()} == "magnify callback fault");
    caught = true;
  }
  NUI_CHECK(caught);
  NUI_CHECK(first_platform.pointer_capture_begin_count == 0);
  fail = false;
  NUI_CHECK(first->dispatch(magnify(20.0f, 30.0f), first_platform) ==
            ui::EventResult::Handled);
  (void)first->dispatch(test::pointer(ui::InputType::PointerDown, 20.0f, 30.0f),
                        first_platform);
  NUI_CHECK(first_platform.pointer_capture_begin_count == 1);
  (void)first->dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                        first_platform);
  NUI_CHECK(first_platform.pointer_capture_end_count == 1);
  first.reset();
  NUI_CHECK(second_platform.pointer_capture_begin_count == 1);
  NUI_CHECK(second_platform.pointer_capture_end_count == 0);
  (void)second.dispatch(
      test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f),
      second_platform);
  NUI_CHECK(second_moves == 1);
  (void)second.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f),
                        second_platform);
  NUI_CHECK(second_platform.pointer_capture_end_count == 1);
}

void synthetic_leave_payload() {
  test::MockPlatform platform;
  ui::UI *owner{};
  std::vector<float> left_factors, right_factors;
  bool nest{true};
  ui::UI tree{ui::Row{
      probe([&](const ui::InputEvent &event, ui::InputContext &) {
        if (event.type == ui::InputType::PointerLeave) {
          left_factors.push_back(event.magnification);
          // This request is queued while the first hover transition runs.
          if (std::exchange(nest, false))
            (void)owner->dispatch(magnify(20.0f, 30.0f, -0.2f), platform);
        }
        return ui::EventResult::Handled;
      }),
      probe([&](const ui::InputEvent &event, ui::InputContext &) {
        if (event.type == ui::InputType::PointerLeave)
          right_factors.push_back(event.magnification);
        return ui::EventResult::Handled;
      })}};
  owner = &tree;
  tree.resize({200.0f, 100.0f});
  tree.activate(platform);
  (void)tree.dispatch(magnify(20.0f, 30.0f), platform);
  (void)tree.dispatch(magnify(120.0f, 30.0f, 0.3f), platform);
  NUI_CHECK(left_factors.size() == 1 && right_factors.size() == 1);
  NUI_CHECK_NEAR(left_factors.front(), 0.0f, 0.0001f);
  NUI_CHECK_NEAR(right_factors.front(), 0.0f, 0.0001f);
}

struct Case {
  std::string_view name;
  void (*run)();
};
constexpr std::array cases{
    Case{"canvas", canvas_coordinates},
    Case{"capture", capture_permission},
    Case{"reentrant_capture", reentrant_capture_permission},
    Case{"captured_drag", captured_drag_owner},
    Case{"nested_contact", nested_new_contact},
    Case{"modal", modal_absorption},
    Case{"overlay", overlay_pointer_policy},
    Case{"throw_recovery", throwing_callback_recovery_and_instance_isolation},
    Case{"leave_payload", synthetic_leave_payload}};

} // namespace

int main(int argc, char **argv) {
  if (argc > 2)
    return 2;
  const std::string_view selected = argc == 2 ? argv[1] : "";
  int failures{};
  bool found{};
  for (const auto &entry : cases) {
    if (!selected.empty() && selected != entry.name)
      continue;
    found = true;
    failures += test::run(entry.name.data(), entry.run);
  }
  return found ? failures == 0 ? 0 : 1 : 2;
}
