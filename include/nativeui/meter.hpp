#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nativeui/detail/bounded_display.hpp>

namespace ui {

class Meter {
public:
  using Formatter = detail::BoundedDisplayComponent::Formatter;

  explicit Meter(State<float> &state, float minimum = 0.0f,
                 float maximum = 1.0f);
  explicit Meter(Binding<float> state, float minimum = 0.0f,
                 float maximum = 1.0f);

  Meter &&levels(MeterLevels value) &&;
  Meter &&threshold_colors(Color warning, Color critical) &&;

  Meter &&orientation(ProgressOrientation value) &&;

  Meter &&formatter(Formatter value) &&;

  Meter &&style(MeterStyle value) &&;

  Spec spec() &&;

private:
  Binding<float> state_;
  float minimum_{};
  float maximum_{1.0f};
  ProgressOrientation orientation_{ProgressOrientation::Horizontal};
  Formatter formatter_;
  MeterStyle style_;
  std::optional<MeterLevels> levels_;
  Color warning_{1.0f, 0.65f, 0.0f, 1.0f};
  Color critical_{0.85f, 0.15f, 0.15f, 1.0f};
};

} // namespace ui
