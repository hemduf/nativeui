#pragma once

#include <nativeui/component.hpp>

namespace ui {
namespace detail {

void paint_text_in_rect(Painter &painter, Rect bounds, std::string_view text,
                        const TextStyle &style);

} // namespace detail

} // namespace ui
