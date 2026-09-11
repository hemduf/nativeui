#define NATIVEUI_ENABLE_INSPECTOR 1

#include "test_support.hpp"

#include <nativeui/headless.hpp>
#include <nativeui/inspector.hpp>
#include <nativeui/ui.hpp>

#include <memory>

namespace {

class FixedComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 40.0f};
    }
};

ui::Spec fixed_spec() {
    return ui::Spec{[] { return std::make_unique<FixedComponent>(); }, {}};
}

void state_is_per_ui_and_snapshot_is_value_based() {
    ui::UI first{fixed_spec()};
    ui::UI second{fixed_spec()};
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
    ui::UI ui{fixed_spec()};
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
    ui::UI ui{fixed_spec()};
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

void suite() {
    state_is_per_ui_and_snapshot_is_value_based();
    enable_and_selection_changes_request_only_one_repaint_each();
    query_reports_pending_layout_and_stale_ids_are_absent();
}

} // namespace

int main() {
    return test::run("t050_inspector_runtime_tests", suite);
}
