#pragma once
#include <functional>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/text_input.hpp>
#include <string_view>
namespace ui {
struct AutocompleteStyle {
  TextInputStyle text_input;
  MenuItemStyle item;
  std::size_t maximum_visible_rows{8};
};
class Autocomplete {
public:
  using SuggestionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view, std::string_view)>;
  Autocomplete(std::string, Binding<std::string>, std::vector<std::string>);
  Autocomplete(std::string, State<std::string> &, std::vector<std::string>);
  Autocomplete(std::string, Binding<std::string>, SuggestionsProvider);
  Autocomplete(std::string, State<std::string> &, SuggestionsProvider);
  Autocomplete &&filter(Filter) &&;
  Autocomplete &&placeholder(std::string) &&;
  Autocomplete &&on_submit(std::function<void(const std::string &)>) &&;
  Autocomplete &&style(AutocompleteStyle) &&;
  Spec spec() &&;

private:
  std::string label_, placeholder_;
  Binding<std::string> value_;
  SuggestionsProvider suggestions_;
  Filter filter_;
  std::function<void(const std::string &)> submit_;
  AutocompleteStyle style_;
};
} // namespace ui
