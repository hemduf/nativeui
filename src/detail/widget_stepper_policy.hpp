#pragma once
#include <nativeui/stepper.hpp>
namespace ui::detail {
struct StepperAccess {
  static double stepped_value(double value, double minimum, double maximum,
                              double step, int direction) noexcept;
  static void
  configure(StepperComponent &editor, bool focusable,
            std::function<void()> request_editor_focus,
            std::function<void(double, std::function<bool()>)> publish = {});
};
} // namespace ui::detail
