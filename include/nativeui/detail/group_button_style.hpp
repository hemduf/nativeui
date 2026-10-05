#pragma once
#include <memory>
#include <nativeui/style.hpp>
namespace ui::detail {
// Immutable lexical presentation only. The closest retained scope replaces
// the previous scope, including an empty recipe; explicit leaf patches win.
struct GroupButtonStyle {
  ButtonStyle buttons;
  ButtonStylePatch selected;
};
class GroupButtonStyleTarget {
public:
  virtual ~GroupButtonStyleTarget() = default;
  virtual void
      bind_group_style(std::shared_ptr<const GroupButtonStyle>) noexcept = 0;
};
} // namespace ui::detail
