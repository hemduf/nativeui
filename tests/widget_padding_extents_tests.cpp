#include "test_support.hpp"
#include <nativeui/column.hpp>
#include <nativeui/padding.hpp>

#include <cmath>
#include <limits>

namespace {

void large_finite_padding_retains_representable_intrinsics_and_placements() {
    const auto maximum = std::numeric_limits<float>::max();
    const std::vector<ui::ChildMetrics> children{ui::ChildMetrics{{12.0f,8.0f},{40.0f,20.0f},{0.0f,1.0f}}};
    ui::PaddingComponent padding{maximum};
    ui::ColumnComponent column{0.0f,maximum};
    for (const auto* component : {static_cast<const ui::Component*>(&padding),
                                  static_cast<const ui::Component*>(&column)}) {
        const auto preferred = component->measure(children);
        const auto minimum = component->minimum_size(children);
        NUI_CHECK(std::isfinite(preferred.w) && std::isfinite(preferred.h));
        NUI_CHECK(std::isfinite(minimum.w) && std::isfinite(minimum.h));
        NUI_CHECK(preferred.w >= 40.0f && preferred.h >= 20.0f);
        NUI_CHECK(minimum.w >= 12.0f && minimum.h >= 8.0f);
        std::vector<ui::ChildPlacement> placements(1);
        component->layout_children({maximum,maximum,40.0f,30.0f},children,placements);
        const auto bounds = placements.front().bounds;
        NUI_CHECK(std::isfinite(bounds.x) && std::isfinite(bounds.y));
        NUI_CHECK(std::isfinite(bounds.w) && std::isfinite(bounds.h));
        NUI_CHECK(bounds.w >= 0.0f && bounds.h >= 0.0f);
        if (component == &padding) {
            NUI_CHECK(bounds.w == 0.0f && bounds.h == 0.0f);
        } else {
            // Column still honors child minima when an inset leaves no space.
            NUI_CHECK_NEAR(bounds.h,8.0f,0.001f);
        }
    }
}

void suite() { large_finite_padding_retains_representable_intrinsics_and_placements(); }
}

int main() { return test::run("widget_padding_extents",&suite); }
