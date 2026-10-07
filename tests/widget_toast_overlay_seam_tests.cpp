#include "test_support.hpp"
namespace {
void bottom_center_uses_natural_bounds_and_preserves_center() {
    ui::UI tree{ui::Button{"Root", [] {}}};
    test::MockPlatform platform;
    tree.resize({200.0f, 180.0f});
    tree.activate(platform);
    ui::OverlaySpec spec;
    spec.placement = ui::OverlayPlacement::ViewportBottomCenter;
    spec.content = ui::Spacer{100.0f, 50.0f}.spec();
    auto handle = tree.show_overlay(std::move(spec));
    tree.resize({200.0f, 180.0f});
    auto entries = tree.overlay_entries();
    NUI_CHECK(entries.size() == 1);
    NUI_CHECK_NEAR(entries.front().bounds.x, 50.0f, 0.01f);
    NUI_CHECK_NEAR(entries.front().bounds.y, 130.0f, 0.01f);
    NUI_CHECK_NEAR(entries.front().bounds.w, 100.0f, 0.01f);
    NUI_CHECK_NEAR(entries.front().bounds.h, 50.0f, 0.01f);
    tree.resize({300.0f, 240.0f});
    entries = tree.overlay_entries();
    NUI_CHECK_NEAR(entries.front().bounds.x, 100.0f, 0.01f);
    NUI_CHECK_NEAR(entries.front().bounds.y, 190.0f, 0.01f);
    NUI_CHECK(tree.close_overlay(handle));
    spec = {};
    spec.placement = ui::OverlayPlacement::AnchorBelow; // historical no-anchor => Center
    spec.content = ui::Spacer{100.0f, 50.0f}.spec();
    handle = tree.show_overlay(std::move(spec));
    tree.resize({200.0f, 180.0f});
    entries = tree.overlay_entries();
    NUI_CHECK_NEAR(entries.front().bounds.x, 50.0f, 0.01f);
    NUI_CHECK_NEAR(entries.front().bounds.y, 65.0f, 0.01f);
}
} // namespace
int main() {
    return test::run("toast_overlay_bottom_center",
                     &bottom_center_uses_natural_bounds_and_preserves_center);
}
