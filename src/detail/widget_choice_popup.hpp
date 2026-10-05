#pragma once
#include <cstdint>
#include <nativeui/combo_box.hpp>
#include <nativeui/detail/overlay_commands.hpp>
#include <optional>
namespace ui::detail {
inline constexpr std::size_t no_choice =
    std::numeric_limits<std::size_t>::max();
struct ChoicePopupSession {
  std::vector<ComboBoxRow> rows;
  MenuItemStyle style;
  std::size_t highlighted{no_choice}, selected{no_choice}, maximum_rows{};
  float minimum_width{}, scroll{};
  std::uint64_t generation{};
  bool live{true}, focusable{true}, wrap{true};
  Key suppressed{Key::None};
  std::function<bool()> allowed;
  std::function<void(std::size_t, Key)> choose;
  std::function<void(Key)> opening_key_released;
  std::function<void()> invalidator, structure_invalidator;
  std::function<std::optional<OverlayComponentCommand>()> take_command;
};
[[nodiscard]] Spec choice_popup_spec(std::shared_ptr<ChoicePopupSession>);
[[nodiscard]] std::size_t choice_step(const ChoicePopupSession &, int);
void choice_highlight(const std::shared_ptr<ChoicePopupSession> &, std::size_t);
} // namespace ui::detail
