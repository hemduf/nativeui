#pragma once

#include <nativeui/avatar.hpp>
#include <nativeui/headless.hpp>
#include <nativeui/rich_text.hpp>
#include <nativeui/state.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

inline void run_unicode_probe() {
  ui::State<std::string> name{"A\xcc\x81" "da Unicode"};
  ui::UI avatar{ui::Avatar{name}.size(32.0)};
  if (avatar.measure().preferred.w != 32.0f)
    throw std::runtime_error("Unicode Avatar size is incorrect");
  ui::HeadlessRenderer avatar_renderer{{64.0f, 40.0f}, 1.0f};
  if (!avatar_renderer.render(avatar))
    throw std::runtime_error("Unicode Avatar rendering failed");
  name.set("Zo\xc3\xab Unicode");
  if (!avatar_renderer.render(avatar))
    throw std::runtime_error("Unicode Avatar update failed");

  std::vector<ui::RichTextSpan> spans{
      {.text = "Unicode combining A\xcc\x81, emoji "
               "\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d"
               "\xf0\x9f\x91\xa7\xe2\x80\x8d\xf0\x9f\x91\xa6, "
               "and bidirectional \xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9 "
               "text wrap across several lines while preserving complete clusters."}};
  ui::UI rich_text{ui::RichText{std::move(spans)}};
  const auto wide = rich_text.measure(ui::Constraints::loose({500.0f, 500.0f})).preferred;
  const auto narrow = rich_text.measure(ui::Constraints::loose({90.0f, 500.0f})).preferred;
  if (!std::isfinite(wide.w) || !std::isfinite(wide.h) ||
      !std::isfinite(narrow.w) || !std::isfinite(narrow.h) ||
      narrow.w > 90.0f || narrow.h <= wide.h)
    throw std::runtime_error("Unicode RichText did not reflow");
  ui::HeadlessRenderer text_renderer{{240.0f, 160.0f}, 1.0f};
  if (!text_renderer.render(rich_text))
    throw std::runtime_error("Unicode RichText rendering failed");
  const auto &pixels = text_renderer.rgba_pixels();
  if (pixels.size() != 240u * 160u * 4u)
    throw std::runtime_error("Unicode RichText produced an incomplete raster frame");
  bool has_painted_rgb{};
  for (std::size_t index = 0; index < pixels.size(); index += 4u) {
    if (pixels[index] != 0 || pixels[index + 1u] != 0 || pixels[index + 2u] != 0) {
      has_painted_rgb = true;
      break;
    }
  }
  if (!has_painted_rgb)
    throw std::runtime_error("Unicode RichText produced no painted raster pixels");
}
