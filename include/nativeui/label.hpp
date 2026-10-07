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

class LabelComponent final : public Component {
public:
  LabelComponent(std::string text, TextStyle style);

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;
  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &context) const override;

private:
  std::string text_;
  TextStyle style_{};
};

class Label {
public:
  explicit Label(std::string text);

  Label &&size(float value) &&;

  Label &&color(Color value) &&;

  Label &&align(TextAlign value) &&;

  Label &&weight(FontWeight value) &&;

  Label &&bold(bool value = true) &&;

  Label &&slant(FontSlant value) &&;

  Label &&italic(bool value = true) &&;

  Label &&family(std::string value) &&;

  Label &&fallback_families(std::vector<std::string> value) &&;

  Label &&style(TextStyle value) &&;

  Spec spec() &&;

private:
  std::string text_;
  TextStyle style_{};
};

using TextLabel = Label;

} // namespace ui
