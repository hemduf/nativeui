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

class ProgressBar {
public:
  using Formatter = detail::BoundedDisplayComponent::Formatter;

  explicit ProgressBar(State<float> &state, float minimum = 0.0f,
                       float maximum = 1.0f);
  explicit ProgressBar(Binding<float> state, float minimum = 0.0f,
                       float maximum = 1.0f);

  ProgressBar &&reversed(bool value = true) &&;
  ProgressBar &&indeterminate(bool value = true) &&;
  ProgressBar &&reduced_motion(bool value = true) &&;

  ProgressBar &&orientation(ProgressOrientation value) &&;

  ProgressBar &&formatter(Formatter value) &&;

  ProgressBar &&style(ProgressBarStyle value) &&;

  Spec spec() &&;

private:
  Binding<float> state_;
  float minimum_{};
  float maximum_{1.0f};
  ProgressOrientation orientation_{ProgressOrientation::Horizontal};
  Formatter formatter_;
  ProgressBarStyle style_;
  bool reversed_{};
  bool indeterminate_{};
  bool reduced_motion_{};
};

} // namespace ui
