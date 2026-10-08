#pragma once
#include <nativeui/component_base.hpp>
#include <nativeui/style.hpp>
#include <optional>
#include <string>
#include <vector>
namespace ui {
struct ToggleGroupStyle {
  float padding{2}, gap{};
  std::optional<float> radius;
  std::optional<Color> fill, border;
  float border_width{1};
  ButtonStyle segment;
  ButtonStylePatch selected;
};
class ToggleGroup {
public:
  ToggleGroup(std::string label, std::vector<Spec> controls);
  ToggleGroup &&style(ToggleGroupStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  std::vector<Spec> controls_;
  ToggleGroupStyle style_;
};
} // namespace ui
