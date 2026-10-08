#include <nativeui/segmented_control.hpp>
#include <type_traits>
struct UserMode {
  int code{};
  bool operator==(const UserMode &) const = default;
};
static_assert(std::is_constructible_v<
              ui::SegmentedControl<UserMode>, std::string,
              ui::Binding<UserMode>, std::vector<ui::SegmentOption<UserMode>>>);
ui::Spec segmented_header_probe(ui::State<UserMode> &value) {
  ui::SegmentedControlStyle style;
  style.segment.base.control_height = 36.0f;
  return ui::SegmentedControl{"View", value,
                              std::vector<ui::SegmentOption<UserMode>>{
                                  {{1}, "List", true}, {{2}, "Grid", true}}}
      .style(std::move(style))
      .spec();
}
