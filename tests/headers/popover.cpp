#include <nativeui/popover.hpp>

namespace {
class Child {
  public:
    ui::Spec spec() && { return {}; }
};
[[maybe_unused]] void header_probe(ui::Binding<bool> value) {
    auto specification = ui::Popover{value, Child{}, Child{}}
                             .match_anchor_width(false)
                             .focus_on_open()
                             .placement(ui::OverlayPlacement::AnchorAbove)
                             .on_close([] {})
                             .style(ui::PopoverStyle{.padding = 8.0f})
                             .spec();
    (void)specification;
}
} // namespace
