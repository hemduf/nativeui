#pragma once
#include <nativeui/search_field.hpp>
namespace ui {
struct FindBarStyle {
  SearchFieldStyle search_field;
  ButtonStyle navigation;
  ButtonStyle close;
  TextStyle status;
  float gap{8.0f};
  float padding{8.0f};
  float maximum_field_width{480.0f};
  float status_width{120.0f};
  std::optional<Color> background;
  std::optional<Color> border;
};
class FindBar {
public:
  FindBar(Binding<bool> open, Binding<std::string> query,
          Binding<std::size_t> matches,
          Binding<std::optional<std::size_t>> current);
  FindBar(State<bool> &open, State<std::string> &query,
          State<std::size_t> &matches,
          State<std::optional<std::size_t>> &current);
  FindBar &&label(std::string value) &&;
  FindBar &&on_navigate(std::function<void(std::size_t)> callback) &&;
  FindBar &&style(FindBarStyle value) &&;
  Spec spec() &&;

private:
  Binding<bool> open_;
  Binding<std::string> query_;
  Binding<std::size_t> matches_;
  Binding<std::optional<std::size_t>> current_;
  std::string label_{"Rechercher"};
  std::function<void(std::size_t)> on_navigate_;
  FindBarStyle style_;
};
} // namespace ui
