#include "test_support.hpp"

#include <nativeui/inspector.hpp>

namespace {

ui::debug::InspectorNode make_node(ui::NodeId id, ui::NodeId parent, std::size_t depth) {
    ui::debug::InspectorNode node{};
    node.id = id;
    node.parent_id = parent;
    node.debug_name = "Component";
    node.bounds = {10.0f * static_cast<float>(id), 5.0f, 20.0f, 10.0f};
    node.clip_bounds = node.bounds;
    node.depth = depth;
    return node;
}

void snapshot_query_is_value_based_and_stale_ids_are_absent() {
    ui::debug::InspectorSnapshot snapshot{};
    snapshot.nodes.push_back(make_node(1, ui::kInvalidNodeId, 0));
    snapshot.nodes.push_back(make_node(2, 1, 1));
    snapshot.dirty_regions.push_back({0.0f, 0.0f, 20.0f, 20.0f});

    const auto* first = snapshot.find(1);
    NUI_CHECK(first != nullptr);
    NUI_CHECK(first->id == 1);
    NUI_CHECK(first->parent_id == ui::kInvalidNodeId);
    NUI_CHECK(first->depth == 0);

    const auto* child = snapshot.find(2);
    NUI_CHECK(child != nullptr);
    NUI_CHECK(child->parent_id == 1);
    NUI_CHECK(child->depth == 1);

    NUI_CHECK(snapshot.find(9999) == nullptr);

    auto copied = snapshot;
    snapshot.nodes.clear();
    snapshot.dirty_regions.clear();
    NUI_CHECK(copied.find(2) != nullptr);
    NUI_CHECK(copied.dirty_regions.size() == 1);
}

void node_snapshot_contains_no_runtime_owner_pointer_contract() {
    ui::debug::InspectorNode node = make_node(42, 7, 3);
    node.layout_dirty = true;
    node.paint_dirty = true;
    node.focusable = true;
    node.focused = true;
    node.pointer_capture_owner = true;
    node.availability = {ui::VisibilityMode::Visible, true, false};
    node.child_order = 4;

    NUI_CHECK(node.id == 42);
    NUI_CHECK(node.parent_id == 7);
    NUI_CHECK(node.debug_name == "Component");
    NUI_CHECK(node.layout_dirty);
    NUI_CHECK(node.paint_dirty);
    NUI_CHECK(node.focusable);
    NUI_CHECK(node.focused);
    NUI_CHECK(node.pointer_capture_owner);
    NUI_CHECK(node.availability.interactive());
    NUI_CHECK(node.child_order == 4);
    NUI_CHECK(node.depth == 3);
}

void suite() {
    snapshot_query_is_value_based_and_stale_ids_are_absent();
    node_snapshot_contains_no_runtime_owner_pointer_contract();
}

} // namespace

int main() {
    return test::run("t050_inspector_model_tests", suite);
}
