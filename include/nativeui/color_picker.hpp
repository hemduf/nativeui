#pragma once
#include <nativeui/button.hpp>
#include <nativeui/text_input.hpp>
namespace ui {
struct ColorSwatch {
  std::string id, name;
  Color value;
};
struct ColorPickerStyle {
  TextInputStyle hex;
  ButtonStyle swatch;
  std::optional<Color> background, border, focus, thumb, checker_light,
      checker_dark, text;
  double preferred_width{280}, padding{8}, gap{8}, channel_height{24},
      hex_height{32}, preview_width{36}, swatch_size{28}, checker_size{6},
      thumb_radius{5}, corner_radius{4}, text_size{14};
};
class ColorPicker {
public:
  ColorPicker(std::string label, Binding<Color> value);
  ColorPicker(std::string label, State<Color> &value);
  ColorPicker &&alpha_enabled(bool value = true) &&;
  ColorPicker &&swatches(std::vector<ColorSwatch> value) &&;
  ColorPicker &&on_change(std::function<void(Color)> callback) &&;
  ColorPicker &&style(ColorPickerStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<Color> value_;
  bool alpha_{true};
  std::vector<ColorSwatch> swatches_;
  std::function<void(Color)> callback_;
  ColorPickerStyle style_;
};
} // namespace ui
