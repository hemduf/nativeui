#pragma once
#include <nativeui/color_picker.hpp>
namespace ui::detail {
struct ColorPreview {
  Color color;
  std::string hex;
  bool invalid{};
};
ColorPreview color_preview(Color);
void validate_swatches(const std::vector<ColorSwatch> &);
void paint_color_preview(Painter &, Rect, Color, double radius,
                         double checker_size, Color light, Color dark);
Spec color_picker_spec(std::string, Binding<Color>, bool,
                       std::vector<ColorSwatch>, std::function<void(Color)>,
                       ColorPickerStyle,
                       std::function<bool()> owner_guard = {});
} // namespace ui::detail
