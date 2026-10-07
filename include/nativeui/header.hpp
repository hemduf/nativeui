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

#include <nativeui/detail/widget_text_paint.hpp>

namespace ui {

struct HeaderStyle {
  HeaderStyle();
  TextStyle title;
  TextStyle subtitle;
  double padding{10.0};
  double gap{7.0};
  Color border_color{colors::border};
  double border_width{1.0};
};

class HeaderComponent final : public Component, public detail::ThemeBinding {
public:
  explicit HeaderComponent(std::string title);
  HeaderComponent(std::string title, std::optional<std::string> subtitle,
                  std::optional<HeaderStyle> style);

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &p) const override;

private:
  [[nodiscard]] HeaderStyle resolved_style() const;
  std::string title_;
  std::string subtitle_{"SATURATION / CHARACTER"};
  std::optional<HeaderStyle> style_;
  bool legacy_{true};
};

class Header {
public:
  explicit Header(std::string title);

  Header &&subtitle(std::string value) &&;
  Header &&style(HeaderStyle value) &&;

  Spec spec() &&;

private:
  std::string title_;
  std::optional<std::string> subtitle_;
  std::optional<HeaderStyle> style_;
};

} // namespace ui
