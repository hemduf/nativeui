#include <nativeui/detail/semantic_native_query.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>

#include <cstdlib>
#include <iostream>
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

ui::SemanticTreeSnapshot button_snapshot(std::string name) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = std::move(name);
    node.info.enabled = true;
    node.info.actions = {ui::SemanticAction::Activate};
    node.bounds = {1.0f, 2.0f, 30.0f, 12.0f};
    tree.nodes.push_back(std::move(node));
    return tree;
}

void native_generation_stays_atomic_when_wrapper_publication_fails() {
    ui::detail::SemanticNativeViewBridge bridge;

    bridge.stage(button_snapshot("Before"));
    const ui::detail::SemanticNativeGeometry first_geometry{
        1.25f, {100.0f, 200.0f}};
    const auto first = bridge.checkpoint_native_publication(first_geometry);
    T068_CHECK(first.has_value());
    T068_CHECK(first->publication != nullptr);

    const auto first_native = bridge.native_current();
    T068_CHECK(first_native != nullptr);
    T068_CHECK(first_native.get() == first->publication.get());
    T068_CHECK(first_native->semantic_snapshot != nullptr);
    T068_CHECK(first_native->semantic_snapshot->nodes.size() == 1);
    T068_CHECK(first_native->semantic_snapshot->nodes.front().info.name == "Before");
    T068_CHECK(first_native->geometry.scale == 1.25f);
    T068_CHECK(first_native->geometry.physical_screen_origin.x == 100.0f);
    T068_CHECK(first_native->geometry.physical_screen_origin.y == 200.0f);

    bridge.fail_next_native_publish_at_for_test(
        ui::detail::SemanticNativePublicationState::FailurePointForTest::
            PublicationAllocation);
    bridge.stage(button_snapshot("After"));
    const ui::detail::SemanticNativeGeometry second_geometry{
        2.0f, {-300.0f, 450.0f}};

    bool allocation_failed = false;
    try {
        (void)bridge.checkpoint_native_publication(second_geometry);
    } catch (const std::bad_alloc&) {
        allocation_failed = true;
    }
    T068_CHECK(allocation_failed);
    T068_CHECK(bridge.has_pending_native_publication());

    // The logical publisher has already committed the newer semantic generation,
    // but native readers must continue seeing the previous semantic+geometry pair
    // atomically until the durable native wrapper retry succeeds.
    const auto logical_after_failure = bridge.current();
    T068_CHECK(logical_after_failure != nullptr);
    T068_CHECK(logical_after_failure->nodes.front().info.name == "After");

    const auto native_after_failure = bridge.native_current();
    T068_CHECK(native_after_failure != nullptr);
    T068_CHECK(native_after_failure.get() == first_native.get());
    T068_CHECK(native_after_failure->semantic_snapshot->nodes.front().info.name ==
               "Before");
    T068_CHECK(native_after_failure->geometry.scale == 1.25f);
    T068_CHECK(native_after_failure->geometry.physical_screen_origin.x == 100.0f);
    T068_CHECK(native_after_failure->geometry.physical_screen_origin.y == 200.0f);

    const auto retried = bridge.retry_pending_native_publication();
    T068_CHECK(retried.has_value());
    T068_CHECK(retried->publication != nullptr);
    T068_CHECK(!bridge.has_pending_native_publication());

    const auto native_after_retry = bridge.native_current();
    T068_CHECK(native_after_retry != nullptr);
    T068_CHECK(native_after_retry.get() == retried->publication.get());
    T068_CHECK(native_after_retry->semantic_snapshot->nodes.front().info.name == "After");
    T068_CHECK(native_after_retry->geometry.scale == 2.0f);
    T068_CHECK(native_after_retry->geometry.physical_screen_origin.x == -300.0f);
    T068_CHECK(native_after_retry->geometry.physical_screen_origin.y == 450.0f);
    T068_CHECK(native_after_retry->generation > first_native->generation);
}

void shutdown_defuncts_future_native_reads_but_retained_generation_survives() {
    ui::detail::SemanticNativeViewBridge bridge;

    bridge.stage(button_snapshot("Retained"));
    const auto published = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{1.5f, {-40.0f, 75.0f}});
    T068_CHECK(published.has_value());

    const auto retained = bridge.native_current();
    T068_CHECK(retained != nullptr);
    T068_CHECK(retained->semantic_snapshot != nullptr);

    bridge.shutdown();

    T068_CHECK(!bridge.native_current());
    T068_CHECK(!bridge.current());
    T068_CHECK(!bridge.has_pending_native_publication());

    // An OS callback that retained the immutable generation before teardown may
    // finish reading it, while every future view lookup is already defunct.
    T068_CHECK(retained->semantic_snapshot->nodes.front().info.name == "Retained");
    T068_CHECK(retained->geometry.scale == 1.5f);
    T068_CHECK(retained->geometry.physical_screen_origin.x == -40.0f);
    T068_CHECK(retained->geometry.physical_screen_origin.y == 75.0f);
}

void native_query_retains_exact_semantic_and_geometry_generation() {
    ui::detail::SemanticNativeViewBridge bridge;

    bridge.stage(button_snapshot("First"));
    const auto first_batch = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{1.25f, {120.0f, -30.0f}});
    T068_CHECK(first_batch.has_value());

    auto first = ui::detail::SemanticNativeSnapshotQuery::ordinary(
        bridge.native_current(), 42);
    T068_CHECK(first.has_value());
    T068_CHECK(first->generation() == first_batch->generation());
    T068_CHECK(first->semantic_generation() == first_batch->semantic_generation());
    T068_CHECK(first->node_id() == 42);
    T068_CHECK(!first->virtual_token().has_value());
    T068_CHECK(first->info().name == "First");
    T068_CHECK(first->logical_bounds().x == 1.0f);
    T068_CHECK(first->geometry().scale == 1.25f);
    T068_CHECK(first->geometry().physical_screen_origin.x == 120.0f);
    T068_CHECK(first->geometry().physical_screen_origin.y == -30.0f);

    auto second_snapshot = button_snapshot("Second");
    second_snapshot.nodes.front().bounds.x = 9.0f;
    bridge.stage(std::move(second_snapshot));
    const auto second_batch = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{2.0f, {-400.0f, 250.0f}});
    T068_CHECK(second_batch.has_value());

    const auto second = ui::detail::SemanticNativeSnapshotQuery::ordinary(
        bridge.native_current(), 42);
    T068_CHECK(second.has_value());
    T068_CHECK(second->generation() == second_batch->generation());
    T068_CHECK(second->semantic_generation() == second_batch->semantic_generation());
    T068_CHECK(second->info().name == "Second");
    T068_CHECK(second->logical_bounds().x == 9.0f);
    T068_CHECK(second->geometry().scale == 2.0f);
    T068_CHECK(second->geometry().physical_screen_origin.x == -400.0f);
    T068_CHECK(second->geometry().physical_screen_origin.y == 250.0f);

    // A native callback that began on the first generation keeps the entire
    // semantic+geometry pair, never a mixed old semantic/new geometry read.
    T068_CHECK(first->info().name == "First");
    T068_CHECK(first->logical_bounds().x == 1.0f);
    T068_CHECK(first->geometry().scale == 1.25f);
    T068_CHECK(first->geometry().physical_screen_origin.x == 120.0f);
    T068_CHECK(first->geometry().physical_screen_origin.y == -30.0f);

    bridge.shutdown();
    T068_CHECK(!ui::detail::SemanticNativeSnapshotQuery::ordinary(
        bridge.native_current(), 42).has_value());
    T068_CHECK(first->info().name == "First");
}

void native_query_resolves_root_from_one_retained_generation() {
    ui::detail::SemanticNativeViewBridge bridge;

    bridge.stage(button_snapshot("Root"));
    const auto first_batch = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{1.75f, {40.0f, -60.0f}});
    T068_CHECK(first_batch.has_value());

    const auto retained = bridge.native_current();
    T068_CHECK(retained != nullptr);
    auto root = ui::detail::SemanticNativeSnapshotQuery::root(retained);
    T068_CHECK(root.has_value());
    T068_CHECK(root->node_id() == 42);
    T068_CHECK(!root->virtual_token().has_value());
    T068_CHECK(root->generation() == first_batch->generation());
    T068_CHECK(root->semantic_generation() == first_batch->semantic_generation());
    T068_CHECK(root->info().name == "Root");
    T068_CHECK(root->geometry().scale == 1.75f);
    T068_CHECK(root->geometry().physical_screen_origin.x == 40.0f);
    T068_CHECK(root->geometry().physical_screen_origin.y == -60.0f);

    auto successor = button_snapshot("Successor");
    successor.nodes.front().bounds.x = 8.0f;
    bridge.stage(std::move(successor));
    const auto second_batch = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{2.0f, {-100.0f, 120.0f}});
    T068_CHECK(second_batch.has_value());

    const auto current_root =
        ui::detail::SemanticNativeSnapshotQuery::root(bridge.native_current());
    T068_CHECK(current_root.has_value());
    T068_CHECK(current_root->info().name == "Successor");

    T068_CHECK(root->info().name == "Root");
    T068_CHECK(root->geometry().scale == 1.75f);
    T068_CHECK(root->geometry().physical_screen_origin.x == 40.0f);
    T068_CHECK(root->geometry().physical_screen_origin.y == -60.0f);

    bridge.shutdown();
    T068_CHECK(!ui::detail::SemanticNativeSnapshotQuery::root(
        bridge.native_current()).has_value());
    T068_CHECK(root->info().name == "Root");
}

ui::VirtualSemanticChildren::TokenIndexSnapshot token_index_for(
    const ui::VirtualSemanticChildren::MetadataSnapshot& metadata) {
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    if (!metadata) {
        return token_index;
    }

    token_index->reserve(metadata->size());
    for (std::size_t index = 0; index < metadata->size(); ++index) {
        token_index->emplace((*metadata)[index].token, index);
    }
    return token_index;
}

ui::SemanticTreeSnapshot virtual_snapshot(
    ui::VirtualSemanticChildren::MetadataSnapshot metadata,
    bool indexed) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 7;

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
    if (indexed) {
        list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
            1, metadata, token_index_for(metadata), 20,
            list.bounds, 20.0f, 5.0f);
    } else {
        list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
            1, std::move(metadata), 20, list.bounds, 20.0f, 5.0f);
    }
    tree.nodes.push_back(std::move(list));
    return tree;
}

void native_query_resolves_virtual_items_only_through_t067_index() {
    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    metadata->push_back({10, "Ten", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select}});
    metadata->push_back({20, "Twenty", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select, ui::SemanticAction::Focus}});

    ui::detail::SemanticNativeViewBridge bridge;
    bridge.stage(virtual_snapshot(metadata, true));
    const auto published = bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{1.5f, {-80.0f, 60.0f}});
    T068_CHECK(published.has_value());

    const auto item = ui::detail::SemanticNativeSnapshotQuery::virtual_item(
        bridge.native_current(), 7, 20);
    T068_CHECK(item.has_value());
    T068_CHECK(item->node_id() == 7);
    T068_CHECK(item->virtual_token() ==
               std::optional<ui::VirtualSemanticItemToken>{20});
    T068_CHECK(item->info().name == "Twenty");
    T068_CHECK(item->info().selected);
    T068_CHECK(item->logical_bounds().y == 15.0f);
    T068_CHECK(item->geometry().scale == 1.5f);
    T068_CHECK(item->geometry().physical_screen_origin.x == -80.0f);

    ui::detail::SemanticNativeViewBridge unindexed_bridge;
    unindexed_bridge.stage(virtual_snapshot(metadata, false));
    const auto unindexed_published = unindexed_bridge.checkpoint_native_publication(
        ui::detail::SemanticNativeGeometry{1.0f, {}});
    T068_CHECK(unindexed_published.has_value());

    // Generic virtual metadata supports a documented linear fallback, but the
    // native callback seam is deliberately O(1) and treats a missing index as
    // defunct rather than scanning a potentially 100k-item dataset.
    T068_CHECK(!ui::detail::SemanticNativeSnapshotQuery::virtual_item(
        unindexed_bridge.native_current(), 7, 20).has_value());
}

} // namespace

int main() {
    try {
        native_generation_stays_atomic_when_wrapper_publication_fails();
        shutdown_defuncts_future_native_reads_but_retained_generation_survives();
        native_query_retains_exact_semantic_and_geometry_generation();
        native_query_resolves_root_from_one_retained_generation();
        native_query_resolves_virtual_items_only_through_t067_index();
        std::cout << "PASS t068 semantic native generation\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic native generation: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
