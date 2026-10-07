#pragma once

#include <nativeui/geometry.hpp>

namespace ui {
class Painter;
class SvgIcon;
namespace detail {
void draw_svg_monochrome(Painter& painter, const SvgIcon& icon, Rect destination, Color color);
}
} // namespace ui
