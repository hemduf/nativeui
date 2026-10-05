#pragma once
#include <nativeui/button.hpp>
#include <nativeui/combo_popup_style.hpp>
namespace ui {
enum class HistoryDirection { Backward, Forward };
struct HistoryEntry {
  std::string key;
  std::string title;
  bool operator==(const HistoryEntry &) const = default;
};
struct HistoryButtonStyle {
  ButtonStyle button;
  MenuItemStyle item;
  bool show_label{};
};
class HistoryButton {
public:
  HistoryButton(HistoryDirection direction, Binding<bool> can_navigate,
                std::function<void(int)> navigate);
  HistoryButton(HistoryDirection direction, State<bool> &can_navigate,
                std::function<void(int)> navigate);
  HistoryButton &&entries(Binding<std::vector<HistoryEntry>>) &&;
  HistoryButton &&entries(State<std::vector<HistoryEntry>> &) &&;
  HistoryButton &&maximum_menu_entries(std::size_t count) &&;
  HistoryButton &&label(std::string text) &&;
  HistoryButton &&style(HistoryButtonStyle) &&;
  Spec spec() &&;

private:
  HistoryDirection direction_;
  Binding<bool> can_;
  std::function<void(int)> navigate_;
  std::optional<Binding<std::vector<HistoryEntry>>> entries_;
  std::size_t maximum_{15};
  std::string label_;
  HistoryButtonStyle style_;
};
} // namespace ui
