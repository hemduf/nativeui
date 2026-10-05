#pragma once
#include <nativeui/button.hpp>
#include <nativeui/text_input.hpp>
namespace ui {
struct SearchFieldStyle {
  TextInputStyle text_input;
  ButtonStyle clear;
  float magnifier_size{16.0f};
  float gap{8.0f};
  float clear_width{28.0f};
  std::optional<Color> magnifier_color;
};
class SearchField {
public:
  SearchField(std::string label, Binding<std::string> query);
  SearchField(std::string label, State<std::string> &query);
  SearchField &&placeholder(std::string value) &&;
  SearchField &&max_length(std::size_t value) &&;
  SearchField &&on_submit(std::function<void(const std::string &)> callback) &&;
  SearchField &&style(SearchFieldStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<std::string> query_;
  std::string placeholder_{"Rechercher"};
  std::size_t max_length_{};
  std::function<void(const std::string &)> on_submit_;
  SearchFieldStyle style_;
};
} // namespace ui
