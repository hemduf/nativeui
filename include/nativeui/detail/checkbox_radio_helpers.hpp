#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nativeui/detail/focus_group.hpp>
#include <nativeui/style.hpp>
#include <stdexcept>

namespace ui::detail {

[[nodiscard]] bool read_only_value_input(const InputEvent &event) noexcept;

[[nodiscard]] bool style_color_equal(Color lhs, Color rhs) noexcept;

[[nodiscard]] bool checkbox_layout_equal(const ResolvedCheckboxStyle &lhs,
                                         const ResolvedCheckboxStyle &rhs);

[[nodiscard]] bool
checkbox_presentation_equal(const ResolvedCheckboxStyle &lhs,
                            const ResolvedCheckboxStyle &rhs);

[[nodiscard]] bool
checkbox_checkmark_visible(const ResolvedCheckboxStyle &style) noexcept;

[[nodiscard]] bool radio_layout_equal(const ResolvedRadioStyle &lhs,
                                      const ResolvedRadioStyle &rhs);

[[nodiscard]] bool radio_presentation_equal(const ResolvedRadioStyle &lhs,
                                            const ResolvedRadioStyle &rhs);

[[nodiscard]] bool radio_mark_visible(const ResolvedRadioStyle &style) noexcept;

template <class Context, class Resolved, class SameLayout,
          class SamePresentation>
void invalidate_resolved_style_transition(
    const Resolved &before, const Resolved &after, Context &context,
    SameLayout &&same_layout, SamePresentation &&same_presentation) {
  if (same_presentation(before, after))
    return;
  if (!same_layout(before, after)) {
    context.invalidate_layout();
    context.invalidate();
    return;
  }
  context.invalidate();
}

struct CheckboxStateInvalidation final {
  const Theme *theme{};
  CheckboxStyle style;
  VisualState visual{};
  std::function<void()> invalidate;
  std::function<void()> invalidate_layout;
  bool active{};

  void sync(VisualState state) noexcept;

  void publish(bool checked);
};

struct RadioStateInvalidation final {
  const Theme *theme{};
  RadioStyle style;
  VisualState visual{};
  std::function<void()> invalidate;
  std::function<void()> invalidate_layout;
  bool active{};

  void sync(VisualState state) noexcept;

  void publish(bool selected);
};

} // namespace ui::detail
