#include "test_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/inspector.hpp>
#include <nativeui/ui.hpp>
#include <nativeui/widgets.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <cstdint>

namespace {

void state_is_per_ui_and_snapshot_is_value_based() {
    ui::UI first{ui::Label{"First"}};
    ui::UI second{ui::Label{"Second"}};
    first.resize({160.0f, 100.0f});
    second.resize({160.0f, 100.0f});

    NUI_CHECK(!ui::debug::inspector_enabled(first));
    NUI_CHECK(!ui::debug::inspector_enabled(second));

    ui::debug::set_inspector_enabled(first, true);
    ui::debug::set_inspector_selected_node(first, 1);

    NUI_CHECK(ui::debug::inspector_enabled(first));
    NUI_CHECK(!ui::debug::inspector_enabled(second));
    NUI_CHECK(ui::debug::inspector_selected_node(first) == 1);
    NUI_CHECK(ui::debug::inspector_selected_node(second) == ui::kInvalidNodeId);

    auto snapshot = ui::debug::inspector_snapshot(first);
    NUI_CHECK(!snapshot.nodes.empty());
    const auto original_count = snapshot.nodes.size();

    // A retained-tree mutation or later snapshot cannot invalidate copied
    // diagnostic values because the snapshot owns all of its data.
    first.invalidate_layout();
    const auto later = ui::debug::inspector_snapshot(first);
    NUI_CHECK(later.nodes.size() == original_count);
    NUI_CHECK(snapshot.nodes.size() == original_count);
}

void enable_and_selection_changes_request_only_one_repaint_each() {
    ui::UI ui{ui::Label{"Inspector"}};
    ui.resize({160.0f, 100.0f});
    ui::HeadlessRenderer renderer{{160.0f, 100.0f}};
    NUI_CHECK(renderer.render(ui));
    NUI_CHECK(!ui.paint_dirty());

    int invalidations = 0;
    ui.set_invalidation_callback([&](ui::Rect) { ++invalidations; });

    ui::debug::set_inspector_enabled(ui, true);
    NUI_CHECK(ui.paint_dirty());
    NUI_CHECK(invalidations == 1);

    ui::debug::set_inspector_enabled(ui, true);
    NUI_CHECK(invalidations == 1);

    NUI_CHECK(renderer.render(ui));
    NUI_CHECK(!ui.paint_dirty());

    ui::debug::set_inspector_selected_node(ui, 1);
    NUI_CHECK(ui.paint_dirty());
    NUI_CHECK(invalidations == 2);

    ui::debug::set_inspector_selected_node(ui, 1);
    NUI_CHECK(invalidations == 2);
}

void query_reports_pending_layout_and_stale_ids_are_absent() {
    ui::UI ui{ui::Label{"Dirty"}};
    ui.resize({120.0f, 80.0f});

    const auto dirty = ui::debug::inspector_snapshot(ui);
    NUI_CHECK(!dirty.nodes.empty());
    bool saw_layout_dirty = false;
    for (const auto& node : dirty.nodes) {
        saw_layout_dirty = saw_layout_dirty || node.layout_dirty;
    }
    NUI_CHECK(saw_layout_dirty);
    NUI_CHECK(dirty.find(999999) == nullptr);

    ui::HeadlessRenderer renderer{{120.0f, 80.0f}};
    NUI_CHECK(renderer.render(ui));
    const auto clean = ui::debug::inspector_snapshot(ui);
    for (const auto& node : clean.nodes) NUI_CHECK(!node.layout_dirty);
}

void inspector_changes_headless_pixels_without_persistent_redraw() {
    ui::UI ui{ui::Label{"Pixels"}};
    ui::HeadlessRenderer renderer{{160.0f, 90.0f}};
    NUI_CHECK(renderer.render(ui));
    const auto baseline = renderer.rgba_pixels();

    ui::debug::set_inspector_enabled(ui, true);
    NUI_CHECK(renderer.render(ui));
    const auto inspected = renderer.rgba_pixels();

    NUI_CHECK(baseline.size() == inspected.size());
    NUI_CHECK(!std::equal(baseline.begin(), baseline.end(), inspected.begin()));
    NUI_CHECK(!ui.paint_dirty());
}

void inspector_does_not_intercept_focus_or_keyboard_input() {
    ui::State<bool> toggled{false};
    ui::UI ui{ui::Toggle{"Target", toggled}};
    test::MockPlatform platform;
    ui.resize({160.0f, 80.0f});
    ui.activate(platform);
    ui::debug::set_inspector_enabled(ui, true);

    const auto result = ui.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(result == ui::EventResult::Handled);
    NUI_CHECK(toggled.get());
}

void inspector_post_paint_preserves_incoming_canvas_state() {
    const auto info = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(static_cast<bool>(surface));
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);

    ui::UI ui{ui::Label{"State"}};
    test::MockPlatform platform;
    ui.resize({20.0f, 20.0f});
    ui::debug::set_inspector_enabled(ui, true);

    canvas->clear(SK_ColorBLACK);
    canvas->translate(10.0f, 12.0f);
    canvas->clipRect(SkRect::MakeXYWH(0.0f, 0.0f, 20.0f, 20.0f));
    const int save_count = canvas->getSaveCount();

    ui.paint(*canvas, platform);
    NUI_CHECK(canvas->getSaveCount() == save_count);

    // Draw after the inspector pass. If it leaked transform/clip state, this
    // marker would move back to the origin or disappear behind a narrowed clip.
    SkPaint marker;
    marker.setColor(SK_ColorRED);
    canvas->drawRect(SkRect::MakeXYWH(0.0f, 0.0f, 3.0f, 3.0f), marker);

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap));
    const auto translated = pixmap.getColor(11, 13);
    const auto origin = pixmap.getColor(1, 1);
    NUI_CHECK(SkColorGetR(translated) > 220);
    NUI_CHECK(SkColorGetR(origin) < 20);
}

void suite() {
    state_is_per_ui_and_snapshot_is_value_based();
    enable_and_selection_changes_request_only_one_repaint_each();
    query_reports_pending_layout_and_stale_ids_are_absent();
    inspector_changes_headless_pixels_without_persistent_redraw();
    inspector_does_not_intercept_focus_or_keyboard_input();
    inspector_post_paint_preserves_incoming_canvas_state();
}

} // namespace

int main() {
    return test::run("t050_inspector_runtime_tests", suite);
}
