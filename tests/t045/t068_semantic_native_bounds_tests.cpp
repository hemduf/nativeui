#include "../../src/detail/semantic_native_bounds.hpp"

#include <nativeui/detail/semantic_native_view_bridge.hpp>
#include <nativeui/semantics.hpp>

#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

bool same_rect(ui::Rect actual, ui::Rect expected) noexcept {
    return actual.x == expected.x && actual.y == expected.y &&
           actual.w == expected.w && actual.h == expected.h;
}

bool same_geometry(const ui::detail::SemanticNativeGeometry& actual,
                   const ui::detail::SemanticNativeGeometry& expected) noexcept {
    return actual == expected;
}

bool only_change(const std::vector<ui::SemanticChange>& changes,
                 ui::SemanticChange expected) noexcept {
    return changes.size() == 1 && changes.front() == expected;
}

ui::SemanticTreeSnapshot make_semantic_snapshot(ui::Rect bounds) {
    ui::SemanticTreeSnapshot snapshot;
    snapshot.root = 1;

    ui::SemanticNodeSnapshot root;
    root.id = 1;
    root.info.role = ui::SemanticRole::Group;
    root.info.name = "Root";
    root.bounds = bounds;
    snapshot.nodes.push_back(std::move(root));
    return snapshot;
}

void ordinary_bounds_use_t043_scale_and_screen_origin_once() {
    ui::detail::ViewGeometryState geometry{{640.0f, 480.0f}};
    T068_CHECK(geometry.observe_scale(1.5f));

    const ui::detail::SemanticNativeBoundsTransform transform{
        geometry, {100.0f, 200.0f}};
    ui::SemanticNodeSnapshot node;
    node.bounds = {1.25f, 2.5f, 10.25f, 4.5f};

    T068_CHECK(transform.scale() == 1.5f);
    T068_CHECK(same_rect(transform.physical_view_bounds(node.bounds),
                         {1.0f, 3.0f, 17.0f, 8.0f}));
    T068_CHECK(same_rect(transform.physical_screen_bounds(node.bounds),
                         {101.0f, 203.0f, 17.0f, 8.0f}));
}

void retained_t043_geometry_captures_scale_and_origin_together() {
    ui::detail::ViewGeometryState geometry{{640.0f, 480.0f}};
    T068_CHECK(geometry.observe_scale(1.5f));
    T068_CHECK(geometry.observe_physical_screen_origin({-120.0f, 45.0f}));

    const ui::detail::SemanticNativeBoundsTransform captured{geometry};
    T068_CHECK(same_geometry(
        captured.geometry(),
        ui::detail::SemanticNativeGeometry{1.5f, {-120.0f, 45.0f}}));
    T068_CHECK(same_rect(
        captured.physical_screen_bounds({2.0f, 4.0f, 10.0f, 8.0f}),
        {-117.0f, 51.0f, 15.0f, 12.0f}));

    // The native publication value is detached from the UI-thread-owned T043
    // state. Later platform observations must not mutate an existing reader.
    T068_CHECK(geometry.observe_scale(2.0f));
    T068_CHECK(geometry.observe_physical_screen_origin({300.0f, 400.0f}));
    T068_CHECK(same_geometry(
        captured.geometry(),
        ui::detail::SemanticNativeGeometry{1.5f, {-120.0f, 45.0f}}));

    const ui::detail::SemanticNativeBoundsTransform next{geometry};
    T068_CHECK(same_geometry(
        next.geometry(),
        ui::detail::SemanticNativeGeometry{2.0f, {300.0f, 400.0f}}));
}

void captured_transform_does_not_follow_live_geometry() {
    ui::detail::ViewGeometryState geometry{{320.0f, 240.0f}};
    T068_CHECK(geometry.observe_scale(2.0f));
    const ui::detail::SemanticNativeBoundsTransform transform{
        geometry, {-30.0f, 15.0f}};

    T068_CHECK(geometry.observe_scale(3.0f));
    const ui::Rect logical{2.0f, 4.0f, 5.0f, 6.0f};

    T068_CHECK(transform.scale() == 2.0f);
    T068_CHECK(same_rect(transform.physical_screen_bounds(logical),
                         {-26.0f, 23.0f, 10.0f, 12.0f}));
}

void retained_t043_scale_survives_invalid_native_observation() {
    ui::detail::ViewGeometryState geometry{{320.0f, 240.0f}};
    T068_CHECK(geometry.observe_scale(2.0f));
    T068_CHECK(!geometry.observe_scale(0.0f));

    const ui::detail::SemanticNativeBoundsTransform transform{
        geometry, {0.0f, 0.0f}};

    T068_CHECK(transform.scale() == 2.0f);
    T068_CHECK(same_rect(transform.physical_view_bounds({1.0f, 1.0f, 3.0f, 4.0f}),
                         {2.0f, 2.0f, 6.0f, 8.0f}));
}

void virtual_item_bounds_use_the_same_logical_contract() {
    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    ui::VirtualSemanticItemMetadata first;
    first.token = 1;
    first.name = "First";
    metadata->push_back(std::move(first));
    ui::VirtualSemanticItemMetadata second;
    second.token = 2;
    second.name = "Second";
    metadata->push_back(std::move(second));

    const auto children = ui::VirtualSemanticChildren::from_metadata(
        7, metadata, std::nullopt, {4.0f, 10.0f, 100.0f, 40.0f}, 20.0f, 5.0f);
    const auto item = children.item_at(1);
    T068_CHECK(item.has_value());
    T068_CHECK(same_rect(item->logical_bounds, {4.0f, 25.0f, 100.0f, 20.0f}));

    ui::detail::ViewGeometryState geometry{{320.0f, 240.0f}};
    T068_CHECK(geometry.observe_scale(1.25f));
    const ui::detail::SemanticNativeBoundsTransform transform{
        geometry, {-50.0f, 12.0f}};

    T068_CHECK(same_rect(transform.physical_screen_bounds(item->logical_bounds),
                         {-45.0f, 43.0f, 125.0f, 26.0f}));
}

void native_publication_pairs_exact_semantics_and_geometry() {
    ui::detail::SemanticNativeViewBridge bridge;
    const ui::detail::SemanticNativeGeometry first_geometry{
        1.5f, {100.0f, 200.0f}};
    const ui::Rect logical_bounds{2.0f, 4.0f, 10.0f, 8.0f};

    bridge.stage(make_semantic_snapshot(logical_bounds));
    const auto first = bridge.checkpoint_native_publication(first_geometry);
    T068_CHECK(first.has_value());
    T068_CHECK(first->publication);
    T068_CHECK(first->generation() == 1);
    T068_CHECK(first->semantic_generation() == 1);
    T068_CHECK(only_change(first->changes, ui::SemanticChange::StructureChanged));
    T068_CHECK(same_geometry(first->publication->geometry, first_geometry));

    const auto retained_first = first->publication;
    const ui::detail::SemanticNativeBoundsTransform first_transform{
        retained_first->geometry};
    T068_CHECK(same_rect(first_transform.physical_screen_bounds(logical_bounds),
                         {103.0f, 206.0f, 15.0f, 12.0f}));

    // A native transform change is exposed as BoundsChanged without changing
    // the logical SemanticTreeSnapshot generation.
    const ui::detail::SemanticNativeGeometry second_geometry{
        2.0f, {-20.0f, 30.0f}};
    bridge.stage(make_semantic_snapshot(logical_bounds));
    const auto second = bridge.checkpoint_native_publication(second_geometry);
    T068_CHECK(second.has_value());
    T068_CHECK(second->generation() == 2);
    T068_CHECK(second->semantic_generation() == 1);
    T068_CHECK(only_change(second->changes, ui::SemanticChange::BoundsChanged));
    T068_CHECK(same_geometry(second->publication->geometry, second_geometry));
    T068_CHECK(same_geometry(retained_first->geometry, first_geometry));
    T068_CHECK(bridge.native_current().get() == second->publication.get());

    // Repeating the exact semantic+geometry state consumes the UI checkpoint
    // but does not manufacture another native generation or notification.
    bridge.stage(make_semantic_snapshot(logical_bounds));
    const auto unchanged = bridge.checkpoint_native_publication(second_geometry);
    T068_CHECK(unchanged.has_value());
    T068_CHECK(unchanged->changes.empty());
    T068_CHECK(unchanged->generation() == 2);
    T068_CHECK(unchanged->semantic_generation() == 1);
    T068_CHECK(unchanged->publication.get() == second->publication.get());

    bridge.shutdown();
    T068_CHECK(!bridge.native_current());
    T068_CHECK(!bridge.current());
    T068_CHECK(retained_first->semantic_snapshot);
    T068_CHECK(retained_first->semantic_generation() == 1);
    T068_CHECK(same_geometry(retained_first->geometry, first_geometry));
}

void native_publication_failure_preserves_exact_pending_batch() {
    ui::detail::SemanticNativeViewBridge bridge;
    const ui::detail::SemanticNativeGeometry geometry{
        1.25f, {10.0f, 20.0f}};
    const ui::Rect first_bounds{1.0f, 2.0f, 40.0f, 20.0f};
    const ui::Rect second_bounds{3.0f, 5.0f, 40.0f, 20.0f};

    bridge.stage(make_semantic_snapshot(first_bounds));
    bridge.fail_next_native_publish_at_for_test(
        ui::detail::SemanticNativePublicationState::FailurePointForTest::
            PublicationAllocation);

    bool threw = false;
    try {
        (void)bridge.checkpoint_native_publication(geometry);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    T068_CHECK(threw);
    T068_CHECK(bridge.has_pending_native_publication());
    T068_CHECK(!bridge.native_current());
    T068_CHECK(bridge.current());
    T068_CHECK(bridge.current()->generation == 1);

    // Newer retained state can be staged while the already-committed semantic
    // batch waits for native allocation recovery. The retry must publish the
    // older exact batch first and leave the newer semantic candidate pending.
    bridge.stage(make_semantic_snapshot(second_bounds));
    T068_CHECK(bridge.has_pending_publication());
    const auto retry = bridge.checkpoint_native_publication(geometry);
    T068_CHECK(retry.has_value());
    T068_CHECK(retry->generation() == 1);
    T068_CHECK(retry->semantic_generation() == 1);
    T068_CHECK(only_change(retry->changes, ui::SemanticChange::StructureChanged));
    T068_CHECK(!bridge.has_pending_native_publication());
    T068_CHECK(bridge.has_pending_publication());

    const auto newer = bridge.checkpoint_native_publication(geometry);
    T068_CHECK(newer.has_value());
    T068_CHECK(newer->generation() == 2);
    T068_CHECK(newer->semantic_generation() == 2);
    T068_CHECK(only_change(newer->changes, ui::SemanticChange::BoundsChanged));
    T068_CHECK(!bridge.has_pending_native_publication());
    T068_CHECK(!bridge.has_pending_publication());
}

void suite() {
    ordinary_bounds_use_t043_scale_and_screen_origin_once();
    retained_t043_geometry_captures_scale_and_origin_together();
    captured_transform_does_not_follow_live_geometry();
    retained_t043_scale_survives_invalid_native_observation();
    virtual_item_bounds_use_the_same_logical_contract();
    native_publication_pairs_exact_semantics_and_geometry();
    native_publication_failure_preserves_exact_pending_batch();
}

} // namespace

int main() {
    try {
        suite();
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
