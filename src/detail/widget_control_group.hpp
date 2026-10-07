#pragma once
#include <memory>
#include <nativeui/detail/availability_wrapper.hpp>
#include <nativeui/detail/focus_group.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/toggle_group.hpp>
#include <optional>
namespace ui::detail {
struct GroupButtonStyle;
struct ControlGroupMemory {
  std::optional<std::size_t> last;
  bool mounted{}, active{};
  std::uint64_t generation{};
};
std::shared_ptr<const GroupButtonStyle>
    make_control_group_style(ButtonStyle, ButtonStylePatch = {});
void bind_control_group_style(
    Component &, const std::shared_ptr<const GroupButtonStyle> &) noexcept;
class ControlGroupItem : public AvailabilityWrapperComponent,
                         public FocusGroupParticipant {
public:
  ControlGroupItem(std::shared_ptr<ControlGroupMemory>, std::size_t,
                   bool vertical);
  bool focusable() const noexcept override;
  FlexFactors flex_factors() const noexcept override;
  ChildMetrics
  measure_constrained(const Constraints &,
                      const std::vector<ChildMetrics> &) const override;
  const void *focus_group_identity() const noexcept override;
  bool focus_group_selected() const override;
  void focus_group_select() override;
  void focus_group_did_focus() noexcept override;
  bool focus_group_accepts_navigation_key(Key) const noexcept override;
  bool focus_group_accepts_boundary_key(Key) const noexcept override;

protected:
  std::shared_ptr<ControlGroupMemory> memory_;
  std::size_t index_{};

private:
  bool vertical_{};
  mutable FlexFactors measured_flex_{};
};
Spec control_group_item(const std::shared_ptr<ControlGroupMemory> &,
                        std::size_t, bool, Spec);
class ControlGroupTrack : public Component, public ThemeBinding {
public:
  ControlGroupTrack(std::string label, ToggleGroupStyle style);
  bool focusable() const noexcept override;
  bool clips_children() const noexcept override;
  Size measure(const std::vector<ChildMetrics> &) const override;
  Size minimum_size(const std::vector<ChildMetrics> &) const override;
  Constraints child_constraints(const Constraints &, std::size_t,
                                std::size_t) const override;
  void layout_children(Rect, const std::vector<ChildMetrics> &,
                       std::vector<ChildPlacement> &) const override;
  SemanticInfo semantics() const override;
  void paint(PaintContext &) const override;

protected:
  std::string label_;
  ToggleGroupStyle style_;
  std::shared_ptr<const GroupButtonStyle> presentation_;

private:
  void bind_descendant_context(Component &) const override;
};
float group_extent(float) noexcept;
} // namespace ui::detail
