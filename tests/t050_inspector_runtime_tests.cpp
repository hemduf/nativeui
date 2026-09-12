#include "test_support.hpp"

#include <nativeui/detail/inspector_paint.hpp>
#include <nativeui/dynamic.hpp>
#include <nativeui/headless.hpp>
#include <nativeui/inspector.hpp>
#include <nativeui/layout.hpp>
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

bool same_rect(ui::Rect a, ui::Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

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
    for (const auto& node : snapshot.nodes) NUI_CHECK(!node.debug_name.empty());
    const auto original_count = snapshot.nodes.size();

    first.invalidate_layout();
    const auto later = ui::debug::inspector_snapshot(first);
    NUI_CHECK(later.nodes.size() == original_count);
    NUI_CHECK(snapshot.nodes.size() == original_count);
}

void snapshot_reports_runtime_hierarchy_bounds_and_child_order() {
    ui::UI ui{ui::Padding{8.0f, ui::Label{"Hierarchy"}}};
    ui.resize({160.0f, 100.0f});

    const auto snapshot = ui::debug::inspector_snapshot(ui);
    NUI_CHECK(snapshot.nodes.size() >= 3);

    const auto& root = snapshot.nodes.front();
    NUI_CHECK(root.id != ui::kInvalidNodeId);
    NUI_CHECK(root.parent_id == ui::kInvalidNodeId);
    NUI_CHECK(root.depth == 0);
    NUI_CHECK(root.child_order == 0);
    NUI_CHECK(same_rect(root.bounds, {0.0f, 0.0f, 160.0f, 100.0f}));

    bool saw_nested_node = false;
    for (std::size_t index = 0; index < snapshot.nodes.size(); ++index) {
        const auto& node = snapshot.nodes[index];
        NUI_CHECK(node.id != ui::kInvalidNodeId);
        NUI_CHECK(node.bounds.w >= 0.0f);
        NUI_CHECK(node.bounds.h >= 0.0f);

        if (node.parent_id == ui::kInvalidNodeId) continue;
        const auto* parent = snapshot.find(node.parent_id);
        NUI_CHECK(parent != nullptr);
        NUI_CHECK(parent->depth + 1 == node.depth);

        std::size_t earlier_siblings = 0;
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (snapshot.nodes[earlier].parent_id == node.parent_id) ++earlier_siblings;
        }
        NUI_CHECK(node.child_order == earlier_siblings);
        saw_nested_node = saw_nested_node || node.depth >= 2;
    }
    NUI_CHECK(saw_nested_node);
}

void destroyed_node_ids_disappear_without_stale_access() {
    ui::State<bool> present{true};
    ui::UI ui{ui::If{present, ui::Label{"Transient"}}};
    test::MockPlatform platform;
    ui.resize({120.0f, 80.0f});
    ui.activate(platform);

    const auto before = ui::debug::inspector_snapshot(ui);
    ui::NodeId transient_id = ui::kInvalidNodeId;
    std::size_t transient_depth = 0;
    for (const auto& node : before.nodes) {
        if (node.depth > transient_depth) {
            transient_depth = node.depth;
            transient_id = node.id;
        }
    }
    NUI_CHECK(transient_depth > 0);
    NUI_CHECK(transient_id != ui::kInvalidNodeId);
    NUI_CHECK(before.find(transient_id) != nullptr);

    present.set(false);
    // T058 installs dynamic structure observers at mount/activation and
    // coalesces the resulting mutation until the next retained-tree boundary.
    // Drive the documented activate -> state change -> resize sequence before
    // querying the post-destruction snapshot. The deepest active descendant is
    // the branch payload, while the shallower If wrapper remains retained.
    ui.resize({120.0f, 80.0f});
    const auto after = ui::debug::inspector_snapshot(ui);
    NUI_CHECK(after.nodes.size() < before.nodes.size());
    NUI_CHECK(after.find(transient_id) == nullptr);

    // The old value snapshot remains self-contained and safe to inspect after
    // the retained node has been destroyed.
    NUI_CHECK(before.find(transient_id) != nullptr);
    ui.deactivate(platform);
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

void query_reports_layout_dirty_and_exact_dirty_regions() {
    ui::UI ui{ui::Label{"Dirty"}};
    ui.resize({120.0f, 80.0f});

    // resize() consumes layout immediately through the UI overlay-layout path.
    // Create a real pending-layout state and verify that a diagnostic query
    // observes it without consuming it.
    ui.invalidate_layout();
    NUI_CHECK(ui.layout_dirty());

    const auto pending = ui::debug::inspector_snapshot(ui);
    NUI_CHECK(!pending.nodes.empty());
    bool saw_layout_dirty = false;
    for (const auto& node : pending.nodes) {
        saw_layout_dirty = saw_layout_dirty || node.layout_dirty;
    }
    NUI_CHECK(saw_layout_dirty);
    NUI_CHECK(ui.layout_dirty());
    NUI_CHECK(pending.find(999999) == nullptr);

    ui::HeadlessRenderer renderer{{120.0f, 80.0f}};
    NUI_CHECK(renderer.render(ui));
    ui.invalidate({7.0f, 9.0f, 13.0f, 11.0f});

    const auto normal_dirty = ui.dirty_regions();
    const auto snapshot = ui::debug::inspector_snapshot(ui);
    NUI_CHECK(snapshot.dirty_regions.size() == normal_dirty.size());
    for (std::size_t index = 0; index < normal_dirty.size(); ++index) {
        NUI_CHECK(same_rect(snapshot.dirty_regions[index], normal_dirty[index]));
    }

    NUI_CHECK(renderer.render(ui));
    const auto clean = ui::debug::inspector_snapshot(ui);
    for (const auto& node : clean.nodes) NUI_CHECK(!node.layout_dirty);
}

void snapshot_reports_focus_capture_and_effective_clip() {
    ui::State<bool> toggled{false};
    ui::UI focused{ui::Padding{8.0f, ui::Toggle{"Focus", toggled}}};
    test::MockPlatform focus_platform;
    focused.resize({140.0f, 70.0f});
    focused.activate(focus_platform);

    const auto focus_snapshot = ui::debug::inspector_snapshot(focused);
    int focused_nodes = 0;
    bool saw_inherited_clip = false;
    for (const auto& node : focus_snapshot.nodes) {
        if (node.focused) {
            ++focused_nodes;
            NUI_CHECK(node.focusable);
        }
        NUI_CHECK(node.clip_bounds.x >= 0.0f);
        NUI_CHECK(node.clip_bounds.y >= 0.0f);
        NUI_CHECK(node.clip_bounds.x + node.clip_bounds.w <= 140.0f);
        NUI_CHECK(node.clip_bounds.y + node.clip_bounds.h <= 70.0f);
        saw_inherited_clip = saw_inherited_clip || !same_rect(node.clip_bounds, node.bounds);
    }
    NUI_CHECK(focused_nodes == 1);
    NUI_CHECK(saw_inherited_clip);

    int capture_requests = 0;
    ui::UI captured{
        ui::Canvas{40.0f, 30.0f, [](ui::CanvasContext2D&) {}}
            .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& context) {
                if (event.type != ui::InputType::PointerDown) {
                    return ui::EventResult::Ignored;
                }
                ++capture_requests;
                context.capture_pointer();
                return ui::EventResult::Handled;
            })};
    test::MockPlatform capture_platform;
    captured.resize({80.0f, 60.0f});
    captured.activate(capture_platform);
    NUI_CHECK(captured.dispatch(test::pointer(ui::InputType::PointerDown, 5.0f, 5.0f),
                                capture_platform) == ui::EventResult::Handled);
    NUI_CHECK(capture_requests == 1);

    const auto capture_snapshot = ui::debug::inspector_snapshot(captured);
    int capture_owners = 0;
    for (const auto& node : capture_snapshot.nodes) {
        if (node.pointer_capture_owner) ++capture_owners;
    }
    NUI_CHECK(capture_owners == 1);
    (void)captured.cancel_pointer(capture_platform);
}

void snapshot_reports_effective_availability() {
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Hidden};
    ui::State<bool> enabled{false};
    ui::UI ui{ui::Visibility{visibility, ui::Enabled{enabled, ui::Label{"Availability"}}}};
    ui.resize({120.0f, 60.0f});

    auto snapshot = ui::debug::inspector_snapshot(ui);
    bool saw_hidden_disabled = false;
    for (const auto& node : snapshot.nodes) {
        saw_hidden_disabled = saw_hidden_disabled ||
            (node.availability.visibility == ui::VisibilityMode::Hidden &&
             !node.availability.enabled);
    }
    NUI_CHECK(saw_hidden_disabled);

    visibility.set(ui::VisibilityMode::Collapsed);
    snapshot = ui::debug::inspector_snapshot(ui);
    bool saw_collapsed_disabled = false;
    for (const auto& node : snapshot.nodes) {
        saw_collapsed_disabled = saw_collapsed_disabled ||
            (node.availability.visibility == ui::VisibilityMode::Collapsed &&
             !node.availability.enabled);
    }
    NUI_CHECK(saw_collapsed_disabled);
}

void overlay_draws_focus_and_capture_markers() {
    const auto info = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    auto surface = SkSurfaces::Raster(info);
    NUI_CHECK(static_cast<bool>(surface));
    auto* canvas = surface->getCanvas();
    NUI_CHECK(canvas != nullptr);
    canvas->clear(SK_ColorBLACK);

    ui::debug::InspectorSnapshot snapshot;
    snapshot.nodes.push_back(ui::debug::InspectorNode{
        .id = 7,
        .parent_id = ui::kInvalidNodeId,
        .debug_name = {},
        .bounds = {10.0f, 10.0f, 40.0f, 30.0f},
        .clip_bounds = {10.0f, 10.0f, 40.0f, 30.0f},
        .focusable = true,
        .focused = true,
        .pointer_capture_owner = true});

    ui::detail::paint_inspector_overlay(*canvas, snapshot, ui::kInvalidNodeId);

    SkPixmap pixmap;
    NUI_CHECK(surface->peekPixels(&pixmap));
    const auto focus = pixmap.getColor(11, 30);
    const auto capture = pixmap.getColor(45, 15);
    NUI_CHECK(SkColorGetR(focus) > 180);
    NUI_CHECK(SkColorGetB(focus) > 180);
    NUI_CHECK(SkColorGetG(focus) < 150);
    NUI_CHECK(SkColorGetR(capture) > 220);
    NUI_CHECK(SkColorGetG(capture) > 80);
    NUI_CHECK(SkColorGetG(capture) < 180);
    NUI_CHECK(SkColorGetB(capture) < 80);
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

void inspector_does_not_intercept_pointer_input() {
    int pointer_downs = 0;
    ui::UI ui{
        ui::Canvas{80.0f, 40.0f, [](ui::CanvasContext2D&) {}}
            .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext&) {
                if (event.type != ui::InputType::PointerDown) {
                    return ui::EventResult::Ignored;
                }
                ++pointer_downs;
                return ui::EventResult::Handled;
            })};
    test::MockPlatform platform;
    ui.resize({80.0f, 40.0f});
    ui.activate(platform);
    ui::debug::set_inspector_enabled(ui, true);

    const auto result = ui.dispatch(
        test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), platform);
    NUI_CHECK(result == ui::EventResult::Handled);
    NUI_CHECK(pointer_downs == 1);
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
    snapshot_reports_runtime_hierarchy_bounds_and_child_order();
    destroyed_node_ids_disappear_without_stale_access();
    enable_and_selection_changes_request_only_one_repaint_each();
    query_reports_layout_dirty_and_exact_dirty_regions();
    snapshot_reports_focus_capture_and_effective_clip();
    snapshot_reports_effective_availability();
    overlay_draws_focus_and_capture_markers();
    inspector_changes_headless_pixels_without_persistent_redraw();
    inspector_does_not_intercept_focus_or_keyboard_input();
    inspector_does_not_intercept_pointer_input();
    inspector_post_paint_preserves_incoming_canvas_state();
}

} // namespace

int main() {
    return test::run("t050_inspector_runtime_tests", suite);
}
