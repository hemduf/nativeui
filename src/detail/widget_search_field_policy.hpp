#pragma once
#include "widget_text_input_policy.hpp"
#include <nativeui/search_field.hpp>
namespace ui::detail {
struct SearchFieldBridge {
  std::weak_ptr<TextInputSession> session;
  std::function<std::optional<EventResult>(const InputEvent &, InputContext &,
                                           const TextInputSnapshot &)>
      before_input;
  std::function<void(bool, TextInputSnapshot)> focus_changed;
};
Spec search_field_spec(std::string label, Binding<std::string> query,
                       std::string placeholder, std::size_t max_length,
                       std::function<void(const std::string &)> submit,
                       SearchFieldStyle style,
                       std::shared_ptr<SearchFieldBridge> bridge = {});
} // namespace ui::detail
