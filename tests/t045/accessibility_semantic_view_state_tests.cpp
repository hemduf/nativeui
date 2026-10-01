#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_view_state.hpp>
#include <nativeui/detail/virtual_list_model.hpp>
#include <nativeui/semantics.hpp>

#include "../../src/detail/view_geometry.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define ACCESSIBILITY_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::SemanticTreeSnapshot snapshot(ui::SemanticId id, std::string name) {
    ui::SemanticTreeSnapshot tree;
    tree.root = id;

    ui::SemanticNodeSnapshot node;
    node.id = id;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = std::move(name);
    node.info.enabled = true;
    node.info.focusable = true;
    node.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    tree.nodes.push_back(std::move(node));
    return tree;
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
    std::uint64_t dataset_generation,
    ui::VirtualSemanticChildren::MetadataSnapshot metadata,
    std::optional<ui::VirtualSemanticItemToken> selected = ui::VirtualSemanticItemToken{20},
    float scroll_y = 0.0f,
    bool focused = false,
    ui::VirtualSemanticChildren::TokenIndexSnapshot token_index = {}) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 7;

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.info.focused = focused;
    list.bounds = {0.0f, 0.0f, 120.0f, 40.0f};
    if (!token_index) {
        token_index = token_index_for(metadata);
    }
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        dataset_generation,
        std::move(metadata),
        std::move(token_index),
        selected,
        list.bounds,
        20.0f,
        scroll_y);
    tree.nodes.push_back(std::move(list));
    return tree;
}

ui::VirtualSemanticChildren::MetadataSnapshot virtual_metadata() {
    auto values = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    values->push_back({10, "Ten", "", true, false,
                       ui::SemanticCheckedState::NotApplicable,
                       {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    values->push_back({20, "Twenty", "", true, false,
                       ui::SemanticCheckedState::NotApplicable,
                       {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    return values;
}

void independent_view_state_destroy_a_keeps_b_alive() {
    auto first = std::make_unique<ui::detail::SemanticViewState>();
    auto second = std::make_unique<ui::detail::SemanticViewState>();

    ACCESSIBILITY_CHECK(first->publish(snapshot(11, "First")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    ACCESSIBILITY_CHECK(second->publish(snapshot(22, "Second")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto first_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        first->publisher(), 11);
    const auto second_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        second->publisher(), 22);

    ACCESSIBILITY_CHECK(first_proxy.read().has_value());
    ACCESSIBILITY_CHECK(first_proxy.read()->info().name == "First");
    ACCESSIBILITY_CHECK(second_proxy.read().has_value());
    ACCESSIBILITY_CHECK(second_proxy.read()->info().name == "Second");

    first.reset();
    ACCESSIBILITY_CHECK(!first_proxy.read().has_value());
    ACCESSIBILITY_CHECK(second_proxy.read().has_value());
    ACCESSIBILITY_CHECK(second_proxy.read()->info().name == "Second");

    ACCESSIBILITY_CHECK(second->publish(snapshot(22, "Second updated")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    ACCESSIBILITY_CHECK(second_proxy.read().has_value());
    ACCESSIBILITY_CHECK(second_proxy.read()->generation() == 2);
    ACCESSIBILITY_CHECK(second_proxy.read()->info().name == "Second updated");
}

void no_change_publish_preserves_generation_and_snapshot_identity() {
    ui::detail::SemanticViewState view;
    ACCESSIBILITY_CHECK(!view.publish(snapshot(7, "Stable")).empty());

    const auto first = view.current();
    ACCESSIBILITY_CHECK(first);
    ACCESSIBILITY_CHECK(first->generation == 1);

    ACCESSIBILITY_CHECK(view.publish(snapshot(7, "Stable")).empty());
    const auto second = view.current();
    ACCESSIBILITY_CHECK(second.get() == first.get());
    ACCESSIBILITY_CHECK(second->generation == 1);
}

void identical_dataset_replacement_refreshes_backing_storage_without_semantic_generation() {
    ui::detail::SemanticViewState view;
    const auto first_metadata = virtual_metadata();
    ACCESSIBILITY_CHECK(view.publish(virtual_snapshot(1, first_metadata)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto first = view.current();
    ACCESSIBILITY_CHECK(first);
    ACCESSIBILITY_CHECK(first->generation == 1);
    ACCESSIBILITY_CHECK(first->nodes.size() == 1);
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children.has_value());
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children->dataset_generation() == 1);
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children->metadata_snapshot().get() ==
               first_metadata.get());

    // Model an explicit dataset replacement that happens to expose identical
    // accessibility values. accessibility must publish the new virtual-list immutable metadata
    // object so subsequent native readers use the current dataset generation,
    // while preserving the semantic generation because no exposed data changed.
    const auto replacement_metadata = virtual_metadata();
    ACCESSIBILITY_CHECK(replacement_metadata.get() != first_metadata.get());
    ACCESSIBILITY_CHECK(view.publish(virtual_snapshot(2, replacement_metadata)).empty());

    const auto second = view.current();
    ACCESSIBILITY_CHECK(second);
    ACCESSIBILITY_CHECK(second.get() != first.get());
    ACCESSIBILITY_CHECK(second->generation == first->generation);
    ACCESSIBILITY_CHECK(second->nodes[0].virtual_children.has_value());
    ACCESSIBILITY_CHECK(second->nodes[0].virtual_children->dataset_generation() == 2);
    ACCESSIBILITY_CHECK(second->nodes[0].virtual_children->metadata_snapshot().get() ==
               replacement_metadata.get());

    // A reader that retained the old immutable generation remains valid and
    // continues to own the original metadata object after publication swaps.
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children->dataset_generation() == 1);
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children->metadata_snapshot().get() ==
               first_metadata.get());
    ACCESSIBILITY_CHECK(first->nodes[0].virtual_children->item_at(1)->info.name == "Twenty");
    ACCESSIBILITY_CHECK(second->nodes[0].virtual_children->item_at(1)->info.name == "Twenty");
}

void large_virtual_dataset_reuses_exact_metadata_across_1000_semantic_publications() {
    using Model = ui::detail::VirtualListDatasetModel<std::size_t>;
    constexpr std::size_t kItemCount = 100000;

    std::vector<Model::Item> items;
    items.reserve(kItemCount);
    for (std::size_t index = 0; index < kItemCount; ++index) {
        items.emplace_back(
            index,
            std::string{},
            true,
            std::string{},
            false,
            ui::SemanticCheckedState::NotApplicable,
            std::vector<ui::SemanticAction>{
                ui::SemanticAction::Select,
                ui::SemanticAction::Focus,
            });
    }

    Model model;
    ACCESSIBILITY_CHECK(model.replace(std::move(items)));
    ACCESSIBILITY_CHECK(model.generation() == 1);
    ACCESSIBILITY_CHECK(model.metadata_rebuild_count() == 1);
    ACCESSIBILITY_CHECK(model.metadata_item_build_count() == kItemCount);

    const auto shared_metadata = model.metadata_snapshot();
    const auto shared_token_index = model.token_index_snapshot();
    ACCESSIBILITY_CHECK(shared_metadata);
    ACCESSIBILITY_CHECK(shared_metadata->size() == kItemCount);
    ACCESSIBILITY_CHECK(shared_token_index);
    ACCESSIBILITY_CHECK(shared_token_index->size() == kItemCount);

    const ui::Rect list_bounds{0.0f, 0.0f, 120.0f, 40.0f};
    auto initial = model.semantic_children(
        std::size_t{0}, list_bounds, 20.0f, 0.0f);
    ACCESSIBILITY_CHECK(initial.metadata_snapshot().get() == shared_metadata.get());
    ACCESSIBILITY_CHECK(initial.token_index_snapshot().get() == shared_token_index.get());

    ui::detail::SemanticViewState view;
    ACCESSIBILITY_CHECK(view.publish(virtual_snapshot(
                   model.generation(),
                   initial.metadata_snapshot(),
                   initial.selected_token(),
                   initial.scroll_y(),
                   false,
                   initial.token_index_snapshot())) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto rebuild_count = model.metadata_rebuild_count();
    const auto item_build_count = model.metadata_item_build_count();

    for (std::size_t iteration = 1; iteration <= 1000; ++iteration) {
        const auto selected_key = iteration % 2 == 0
            ? std::size_t{0}
            : kItemCount - 1;
        const float scroll_y = static_cast<float>(iteration) * 0.5f;
        const bool focused = iteration % 2 != 0;

        // This path is the real virtual-list -> accessibility projection seam. Selection lookup
        // is indexed from the immutable dataset and publication reuses the exact
        // metadata object. The counters prove no O(N) metadata construction
        // occurs during these 1,000 semantic generations.
        auto projection = model.semantic_children(
            selected_key, list_bounds, 20.0f, scroll_y);
        ACCESSIBILITY_CHECK(projection.dataset_generation() == model.generation());
        ACCESSIBILITY_CHECK(projection.metadata_snapshot().get() == shared_metadata.get());
        ACCESSIBILITY_CHECK(projection.token_index_snapshot().get() == shared_token_index.get());
        ACCESSIBILITY_CHECK(model.metadata_rebuild_count() == rebuild_count);
        ACCESSIBILITY_CHECK(model.metadata_item_build_count() == item_build_count);

        const auto changes = view.publish(virtual_snapshot(
            model.generation(),
            projection.metadata_snapshot(),
            projection.selected_token(),
            projection.scroll_y(),
            focused,
            projection.token_index_snapshot()));
        const std::vector<ui::SemanticChange> expected{
            ui::SemanticChange::FocusChanged,
            ui::SemanticChange::SelectionChanged,
            ui::SemanticChange::BoundsChanged,
        };
        ACCESSIBILITY_CHECK(changes == expected);

        const auto current = view.current();
        ACCESSIBILITY_CHECK(current);
        ACCESSIBILITY_CHECK(current->generation == iteration + 1);
        ACCESSIBILITY_CHECK(current->nodes.size() == 1);
        ACCESSIBILITY_CHECK(current->nodes[0].virtual_children.has_value());
        const auto& virtual_children = *current->nodes[0].virtual_children;
        ACCESSIBILITY_CHECK(virtual_children.dataset_generation() == model.generation());
        ACCESSIBILITY_CHECK(virtual_children.size() == kItemCount);
        ACCESSIBILITY_CHECK(virtual_children.metadata_snapshot().get() == shared_metadata.get());
        ACCESSIBILITY_CHECK(virtual_children.token_index_snapshot().get() == shared_token_index.get());
    }

    ACCESSIBILITY_CHECK(model.metadata_rebuild_count() == 1);
    ACCESSIBILITY_CHECK(model.metadata_item_build_count() == kItemCount);
    const auto final_snapshot = view.current();
    ACCESSIBILITY_CHECK(final_snapshot);
    ACCESSIBILITY_CHECK(final_snapshot->generation == 1001);
    ACCESSIBILITY_CHECK(final_snapshot->nodes[0].virtual_children->metadata_snapshot().get() ==
               shared_metadata.get());
    ACCESSIBILITY_CHECK(final_snapshot->nodes[0].virtual_children->token_index_snapshot().get() ==
               shared_token_index.get());

    const auto last_token = model.token_for_key(kItemCount - 1);
    ACCESSIBILITY_CHECK(last_token.has_value());
    const auto last_proxy = ui::detail::SemanticSnapshotProxy::virtual_item(
        view.publisher(), 7, *last_token);
    const auto last_read = last_proxy.read();
    ACCESSIBILITY_CHECK(last_read.has_value());
    ACCESSIBILITY_CHECK(last_read->virtual_token() == last_token);
    ACCESSIBILITY_CHECK(last_read->bounds().y ==
               list_bounds.y + static_cast<float>(kItemCount - 1) * 20.0f -
                   final_snapshot->nodes[0].virtual_children->scroll_y());
}

void staged_changes_coalesce_to_one_generation_and_one_batch() {
    ui::detail::SemanticViewState view;
    ACCESSIBILITY_CHECK(view.publish(snapshot(7, "Initial")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto baseline = view.current();
    ACCESSIBILITY_CHECK(baseline);
    ACCESSIBILITY_CHECK(baseline->generation == 1);

    auto focus_only = snapshot(7, "Intermediate");
    focus_only.nodes[0].info.focused = true;
    view.stage(std::move(focus_only));

    auto final = snapshot(7, "Final");
    final.nodes[0].info.focused = true;
    final.nodes[0].info.selected = true;
    final.nodes[0].bounds = {1.0f, 2.0f, 80.0f, 24.0f};
    view.stage(std::move(final));

    ACCESSIBILITY_CHECK(view.has_pending_publication());
    ACCESSIBILITY_CHECK(view.current().get() == baseline.get());

    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::FocusChanged,
        ui::SemanticChange::SelectionChanged,
        ui::SemanticChange::ValueChanged,
        ui::SemanticChange::BoundsChanged,
    };
    ACCESSIBILITY_CHECK(view.checkpoint() == expected);

    const auto published = view.current();
    ACCESSIBILITY_CHECK(published);
    ACCESSIBILITY_CHECK(published.get() != baseline.get());
    ACCESSIBILITY_CHECK(published->generation == 2);
    ACCESSIBILITY_CHECK(published->nodes.size() == 1);
    ACCESSIBILITY_CHECK(published->nodes[0].info.name == "Final");
    ACCESSIBILITY_CHECK(published->nodes[0].info.focused);
    ACCESSIBILITY_CHECK(published->nodes[0].info.selected);
    ACCESSIBILITY_CHECK(published->nodes[0].bounds.x == 1.0f);
    ACCESSIBILITY_CHECK(published->nodes[0].bounds.y == 2.0f);
    ACCESSIBILITY_CHECK(published->nodes[0].bounds.w == 80.0f);
    ACCESSIBILITY_CHECK(published->nodes[0].bounds.h == 24.0f);
    ACCESSIBILITY_CHECK(!view.has_pending_publication());

    ACCESSIBILITY_CHECK(view.checkpoint().empty());
    ACCESSIBILITY_CHECK(view.current().get() == published.get());
    ACCESSIBILITY_CHECK(view.current()->generation == 2);
}

void virtual_bounds_follow_scroll_and_t043_fractional_conversion() {
    const auto metadata = virtual_metadata();
    ui::detail::SemanticViewState view;
    ACCESSIBILITY_CHECK(view.publish(virtual_snapshot(
                   1,
                   metadata,
                   ui::VirtualSemanticItemToken{20},
                   5.5f,
                   false)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto proxy = ui::detail::SemanticSnapshotProxy::virtual_item(
        view.publisher(), 7, 20);
    const auto first = proxy.read();
    ACCESSIBILITY_CHECK(first.has_value());
    const auto first_logical = first->bounds();
    ACCESSIBILITY_CHECK(first_logical.x == 0.0f);
    ACCESSIBILITY_CHECK(first_logical.y == 14.5f);
    ACCESSIBILITY_CHECK(first_logical.w == 120.0f);
    ACCESSIBILITY_CHECK(first_logical.h == 20.0f);

    const auto first_physical =
        ui::detail::logical_to_physical_covering_rect(first_logical, 1.25f);
    ACCESSIBILITY_CHECK(first_physical.x == 0.0f);
    ACCESSIBILITY_CHECK(first_physical.y == 18.0f);
    ACCESSIBILITY_CHECK(first_physical.w == 150.0f);
    ACCESSIBILITY_CHECK(first_physical.h == 26.0f);

    ACCESSIBILITY_CHECK(view.publish(virtual_snapshot(
                   1,
                   metadata,
                   ui::VirtualSemanticItemToken{20},
                   10.25f,
                   false)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::BoundsChanged});

    const auto second = proxy.read();
    ACCESSIBILITY_CHECK(second.has_value());
    const auto second_logical = second->bounds();
    ACCESSIBILITY_CHECK(second_logical.x == 0.0f);
    ACCESSIBILITY_CHECK(second_logical.y == 9.75f);
    ACCESSIBILITY_CHECK(second_logical.w == 120.0f);
    ACCESSIBILITY_CHECK(second_logical.h == 20.0f);

    const auto second_physical =
        ui::detail::logical_to_physical_covering_rect(second_logical, 1.25f);
    ACCESSIBILITY_CHECK(second_physical.x == 0.0f);
    ACCESSIBILITY_CHECK(second_physical.y == 12.0f);
    ACCESSIBILITY_CHECK(second_physical.w == 150.0f);
    ACCESSIBILITY_CHECK(second_physical.h == 26.0f);
}

} // namespace

int main() {
    try {
        independent_view_state_destroy_a_keeps_b_alive();
        no_change_publish_preserves_generation_and_snapshot_identity();
        identical_dataset_replacement_refreshes_backing_storage_without_semantic_generation();
        large_virtual_dataset_reuses_exact_metadata_across_1000_semantic_publications();
        staged_changes_coalesce_to_one_generation_and_one_batch();
        virtual_bounds_follow_scroll_and_t043_fractional_conversion();
        std::cout << "PASS accessibility semantic view state\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL accessibility semantic view state: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
