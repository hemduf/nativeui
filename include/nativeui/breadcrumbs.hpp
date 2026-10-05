#pragma once
#include <nativeui/button.hpp>
#include <nativeui/combo_popup_style.hpp>
namespace ui {
struct BreadcrumbItem {
  std::string key;
  std::string label;
  bool enabled{true};
  bool operator==(const BreadcrumbItem &) const = default;
};
struct BreadcrumbsStyle {
  ButtonStyle item;
  MenuItemStyle overflow;
  std::optional<Color> text, destination, separator;
  std::string font_family;
  double text_size{14}, minimum_item_width{40}, overflow_width{28}, height{32},
      padding{4}, gap{4}, chevron_width{10};
};
class Breadcrumbs {
public:
  Breadcrumbs(Binding<std::vector<BreadcrumbItem>> path);
  Breadcrumbs(State<std::vector<BreadcrumbItem>> &path);
  Breadcrumbs &&on_navigate(std::function<void(const std::string &)>) &&;
  Breadcrumbs &&label(std::string accessible_name) &&;
  Breadcrumbs &&style(BreadcrumbsStyle) &&;
  Spec spec() &&;

private:
  Binding<std::vector<BreadcrumbItem>> path_;
  std::function<void(const std::string &)> navigate_;
  std::string label_;
  BreadcrumbsStyle style_;
};
} // namespace ui
