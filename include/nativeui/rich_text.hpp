#pragma once
#include <functional>
#include <nativeui/component.hpp>
#include <nativeui/text.hpp>
#include <optional>
#include <string>
#include <vector>
namespace ui {
struct RichTextSpan {
  std::string id;
  std::string text;
  std::optional<TextStyle> style;
  std::optional<Color> background;
  bool underline{};
  bool strikethrough{};
  std::function<void()> on_activate;
};
class RichText {
public:
  explicit RichText(std::vector<RichTextSpan> spans);
  RichText &&style(TextStyle value) &&;
  RichText &&wrap(bool value = true) &&;
  Spec spec() &&;

private:
  std::vector<RichTextSpan> spans_;
  TextStyle style_;
  bool wrap_{true};
};
} // namespace ui
