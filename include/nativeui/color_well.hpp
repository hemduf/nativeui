#pragma once
#include <nativeui/color_picker.hpp>
namespace ui {
struct ColorWellStyle {
  std::optional<Color> background, border, focus, checker_light, checker_dark;
  double preview_width{36}, preview_height{20}, padding{5}, corner_radius{4},
      border_width{1}, checker_size{6};
};
class ColorWell {
public:
  ColorWell(std::string label, Binding<Color> value);
  ColorWell(std::string label, State<Color> &value);
  ColorWell &&alpha_enabled(bool value = true) &&;
  ColorWell &&swatches(std::vector<ColorSwatch> value) &&;
  ColorWell &&on_change(std::function<void(Color)> callback) &&;
  ColorWell &&style(ColorWellStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<Color> value_;
  bool alpha_{true};
  std::vector<ColorSwatch> swatches_;
  std::function<void(Color)> callback_;
  ColorWellStyle style_;
};
} // namespace ui
