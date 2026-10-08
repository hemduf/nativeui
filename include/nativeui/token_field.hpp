#pragma once
#include <nativeui/button.hpp>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/text_input.hpp>
namespace ui {
struct TokenFieldStyle {
  TextInputStyle text_input;
  MenuItemStyle suggestions;
  ButtonStyle remove;
  std::optional<Color> background, border, chip_background, chip_text,
      chip_border, active;
  double minimum_width{240}, draft_width{90}, chip_height{28}, chip_padding{8},
      remove_width{24}, padding{5}, gap{4}, text_size{14}, corner_radius{5};
};
class TokenField {
public:
  TokenField(std::string label, Binding<std::vector<std::string>> tokens,
             std::vector<std::string> suggestions = {});
  TokenField(std::string label, State<std::vector<std::string>> &tokens,
             std::vector<std::string> suggestions = {});
  TokenField &&allow_custom(bool value = true) &&;
  TokenField &&maximum_tokens(std::size_t value) &&;
  TokenField &&placeholder(std::string value) &&;
  TokenField &&style(TokenFieldStyle value) &&;
  Spec spec() &&;

private:
  std::string label_, placeholder_;
  Binding<std::vector<std::string>> tokens_;
  std::vector<std::string> suggestions_;
  bool custom_{true};
  std::size_t maximum_{};
  TokenFieldStyle style_;
};
} // namespace ui
