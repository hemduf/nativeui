#pragma once
#include "widget_choice_popup.hpp"
#include "widget_text_input_policy.hpp"
namespace ui::detail {
using StringSuggestions = std::function<std::vector<std::string>()>;
using SuggestionFilter =
    std::function<bool(std::string_view, std::string_view)>;
class SuggestionsEngine
    : public std::enable_shared_from_this<SuggestionsEngine> {
public:
  StringSuggestions provider;
  SuggestionFilter filter;
  MenuItemStyle style;
  std::size_t maximum_rows{8};
  float minimum_width{};
  std::string selection;
  NodeId owner{kInvalidNodeId};
  std::shared_ptr<ChoicePopupSession> session;
  std::optional<OverlayComponentCommand> pending;
  OverlayHandle handle;
  std::uint64_t generation{};
  bool mounted{}, freeform{};
  std::function<bool()> allowed;
  std::function<std::function<void()>(std::string)> prepare_choice;
  std::function<void()> invalidate;
  void rebuild(std::string query, bool full = false);
  void move(int direction, std::string query);
  bool choose_highlight();
  void choose(std::size_t index);
  void close();
  void detach() noexcept;
  bool expanded() const noexcept;
  std::optional<OverlayComponentCommand> take_command();
};
[[nodiscard]] bool suggestions_blank(std::string_view);
} // namespace ui::detail
