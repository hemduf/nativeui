#pragma once
#include <functional>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/component_base.hpp>
#include <optional>
#include <string>
#include <vector>
namespace ui {
struct PopupMenuItem final {
  enum class Kind { Action, Separator };
  Kind kind{Kind::Action};
  std::string label;
  bool enabled{true};
  std::function<void()> callback;
  std::string key;
  std::optional<bool> checked;
  std::string shortcut_label;
  std::vector<PopupMenuItem> children;
  [[nodiscard]] static PopupMenuItem action(std::string label,
                                            std::function<void()> callback,
                                            bool enabled = true);
  [[nodiscard]] static PopupMenuItem separator();
  [[nodiscard]] bool actionable() const noexcept;
};
class PopupMenu {
public:
  using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;
  PopupMenu(std::string label, std::vector<PopupMenuItem> items);
  PopupMenu(std::string label, ItemsProvider items_provider);
  PopupMenu &&style(ComboBoxStyle value) &&;
  PopupMenu &&item_style(MenuItemStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  ItemsProvider provider_;
  ComboBoxStyle style_;
  MenuItemStyle item_style_;
};
} // namespace ui
