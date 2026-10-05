#include "test_support.hpp"
#include <nativeui/overlay.hpp>
namespace {
class Box final : public ui::Component {
public:
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80, 20}; }
  void paint(ui::PaintContext&) const override {}
};
template<class Placement = ui::OverlayPlacement>
void bottom_center_and_resize_preserve_historical_center() {
  if constexpr (requires { Placement::ViewportBottomCenter; }) {
    ui::UI tree{ui::Label{"Owner"}};
    test::MockPlatform platform;
    tree.resize({200, 100}); tree.activate(platform);
    ui::OverlaySpec overlay;
    overlay.content = {[] { return std::make_unique<Box>(); }, {}};
    overlay.placement = Placement::ViewportBottomCenter;
    overlay.pointer_policy = ui::OverlayPointerPolicy::Ignore;
    auto handle = tree.show_overlay(overlay);
    tree.resize({200, 100});
    const auto entries = tree.overlay_entries();
    NUI_CHECK(entries.size() == 1 && entries[0].resolved);
    NUI_CHECK_NEAR(entries[0].bounds.x, 60.0f, .01f);
    NUI_CHECK_NEAR(entries[0].bounds.y, 80.0f, .01f);
    NUI_CHECK_NEAR(entries[0].bounds.w, 80.0f, .01f);
    NUI_CHECK_NEAR(entries[0].bounds.h, 20.0f, .01f);
    tree.resize({100, 50});
    const auto resized = tree.overlay_entries();
    NUI_CHECK_NEAR(resized[0].bounds.x, 10.0f, .01f);
    NUI_CHECK_NEAR(resized[0].bounds.y, 30.0f, .01f);
    NUI_CHECK(tree.close_overlay(handle));
    overlay.placement = Placement::AnchorBelow; // Unanchored legacy stays Center.
    const auto centered_handle = tree.show_overlay(overlay);
    NUI_CHECK(centered_handle.valid());
    tree.resize({100, 50});
    const auto centered = tree.overlay_entries();
    NUI_CHECK(centered.size() == 1);
    NUI_CHECK_NEAR(centered[0].bounds.x, 10.0f, .01f);
    NUI_CHECK_NEAR(centered[0].bounds.y, 15.0f, .01f);
  } else {
    NUI_CHECK(false && "ViewportBottomCenter additive placement is absent");
  }
}
void suite() { bottom_center_and_resize_preserve_historical_center(); }
}
int main() { return test::run("widget_overlay_bottom_center", &suite); }
