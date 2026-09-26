#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/detail/semantic_view_state.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
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

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::VirtualSemanticChildren::MetadataSnapshot metadata(
    std::initializer_list<std::pair<ui::VirtualSemanticItemToken, std::string>> entries) {
    auto values = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    values->reserve(entries.size());
    for (auto [token, name] : entries) {
        values->push_back({
            token,
            std::move(name),
            "",
            true,
            false,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select, ui::SemanticAction::Focus},
        });
    }
    return values;
}

ui::SemanticTreeSnapshot snapshot(
    std::uint64_t dataset_generation,
    ui::VirtualSemanticChildren::MetadataSnapshot items,
    std::optional<ui::VirtualSemanticItemToken> selected,
    float scroll_y = 0.0f,
    ui::VirtualSemanticChildren::TokenIndexSnapshot token_index = {}) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 1;

    ui::SemanticNodeSnapshot list;
    list.id = 1;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
    list.virtual_children = token_index
        ? ui::VirtualSemanticChildren::from_indexed_metadata(
              dataset_generation,
              std::move(items),
              std::move(token_index),
              selected,
              list.bounds,
              20.0f,
              scroll_y)
        : ui::VirtualSemanticChildren::from_metadata(
              dataset_generation,
              std::move(items),
              selected,
              list.bounds,
              20.0f,
              scroll_y);
    tree.nodes.push_back(std::move(list));
    return tree;
}

ui::SemanticTreeSnapshot ordinary_snapshot(bool include_child,
                                           bool child_focused,
                                           bool child_selected) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 1;

    ui::SemanticNodeSnapshot root;
    root.id = 1;
    root.info.role = ui::SemanticRole::Group;
    root.info.name = "Root";
    if (include_child) root.children.push_back(2);
    tree.nodes.push_back(std::move(root));

    if (include_child) {
        ui::SemanticNodeSnapshot child;
        child.id = 2;
        child.parent = 1;
        child.info.role = ui::SemanticRole::Button;
        child.info.name = "Child";
        child.info.focused = child_focused;
        child.info.selected = child_selected;
        tree.nodes.push_back(std::move(child));
    }
    return tree;
}

ui::SemanticTreeSnapshot virtual_selection_snapshot(bool include_list) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 1;

    ui::SemanticNodeSnapshot root;
    root.id = 1;
    root.info.role = ui::SemanticRole::Group;
    root.info.name = "Root";
    if (include_list) root.children.push_back(2);
    tree.nodes.push_back(std::move(root));

    if (include_list) {
        ui::SemanticNodeSnapshot list;
        list.id = 2;
        list.parent = 1;
        list.info.role = ui::SemanticRole::ListView;
        list.info.name = "Selected list";
        list.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
        list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
            1,
            metadata({{10, "Ten"}, {20, "Twenty"}}),
            ui::VirtualSemanticItemToken{20},
            list.bounds,
            20.0f,
            0.0f);
        tree.nodes.push_back(std::move(list));
    }
    return tree;
}

ui::VirtualSemanticChildren::TokenIndexSnapshot token_index(
    std::initializer_list<ui::VirtualSemanticItemToken> tokens) {
    auto result = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    std::size_t index = 0;
    for (const auto token : tokens) {
        result->emplace(token, index++);
    }
    return result;
}

void storage_only_index_refresh_keeps_semantic_generation() {
    ui::detail::SemanticViewState view;
    const auto items = metadata({{10, "Ten"}, {20, "Twenty"}});
    const auto first_index = token_index({10, 20});
    const auto replacement_index = token_index({10, 20});

    T068_CHECK(first_index.get() != replacement_index.get());
    T068_CHECK(view.publish(snapshot(1, items, 10, 0.0f, first_index)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto baseline = view.current();
    T068_CHECK(baseline);
    T068_CHECK(baseline->generation == 1);
    T068_CHECK(baseline->nodes[0].virtual_children->token_index_snapshot().get() ==
               first_index.get());

    T068_CHECK(view.publish(snapshot(1, items, 10, 0.0f, replacement_index)).empty());

    const auto refreshed = view.current();
    T068_CHECK(refreshed);
    T068_CHECK(refreshed.get() != baseline.get());
    T068_CHECK(refreshed->generation == baseline->generation);
    T068_CHECK(refreshed->nodes[0].virtual_children->metadata_snapshot().get() ==
               items.get());
    T068_CHECK(refreshed->nodes[0].virtual_children->token_index_snapshot().get() ==
               replacement_index.get());
    T068_CHECK(baseline->nodes[0].virtual_children->token_index_snapshot().get() ==
               first_index.get());
}

void metadata_generation_alone_is_not_structure() {
    const auto before = snapshot(1, metadata({{10, "Ten"}, {20, "Twenty"}}), 10);
    const auto after = snapshot(2, metadata({{10, "Ten"}, {20, "Twenty updated"}}), 10);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    T068_CHECK(changes ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
}

void reorder_does_not_manufacture_value_change() {
    const auto before = snapshot(7, metadata({{10, "Ten"}, {20, "Twenty"}}), 10);
    const auto after = snapshot(8, metadata({{20, "Twenty"}, {10, "Ten"}}), 10);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    T068_CHECK(changes ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
}

void structure_change_can_coexist_with_selection_value_and_bounds() {
    const auto before = snapshot(12, metadata({{10, "Ten"}, {20, "Twenty"}}), 10, 0.0f);
    const auto after = snapshot(
        13,
        metadata({{20, "Twenty updated"}, {10, "Ten"}, {30, "Thirty"}}),
        20,
        5.0f);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::StructureChanged,
        ui::SemanticChange::SelectionChanged,
        ui::SemanticChange::ValueChanged,
        ui::SemanticChange::BoundsChanged,
    };
    T068_CHECK(changes == expected);
}

void removing_focused_selected_node_keeps_state_notifications_in_structure_batch() {
    const auto before = ordinary_snapshot(true, true, true);
    const auto after = ordinary_snapshot(false, false, false);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::StructureChanged,
        ui::SemanticChange::FocusChanged,
        ui::SemanticChange::SelectionChanged,
    };
    T068_CHECK(changes == expected);
}

void adding_focused_selected_node_keeps_state_notifications_in_structure_batch() {
    const auto before = ordinary_snapshot(false, false, false);
    const auto after = ordinary_snapshot(true, true, true);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::StructureChanged,
        ui::SemanticChange::FocusChanged,
        ui::SemanticChange::SelectionChanged,
    };
    T068_CHECK(changes == expected);
}

void removing_selected_virtual_container_keeps_selection_notification() {
    const auto before = virtual_selection_snapshot(true);
    const auto after = virtual_selection_snapshot(false);

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::StructureChanged,
        ui::SemanticChange::SelectionChanged,
    };
    T068_CHECK(changes == expected);
}

void publication_failure_keeps_previous_generation_and_retries_pending_state() {
    using FailurePoint = ui::detail::SemanticSnapshotPublisher::FailurePointForTest;

    ui::detail::SemanticViewState view;
    const auto initial_items = metadata({{10, "Ten"}, {20, "Twenty"}});
    T068_CHECK(view.publish(snapshot(1, initial_items, 10)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto baseline = view.current();
    T068_CHECK(baseline);
    T068_CHECK(baseline->generation == 1);

    const auto updated_items = metadata({{10, "Ten"}, {20, "Twenty updated"}});
    view.publisher()->fail_next_publish_at_for_test(FailurePoint::DiffPreparation);
    bool failed = false;
    try {
        (void)view.publish(snapshot(2, updated_items, 10));
    } catch (const std::bad_alloc&) {
        failed = true;
    }

    T068_CHECK(failed);
    T068_CHECK(view.has_pending_publication());
    T068_CHECK(view.current().get() == baseline.get());
    T068_CHECK(view.current()->generation == 1);
    T068_CHECK(view.current()->nodes[0].virtual_children->dataset_generation() == 1);
    T068_CHECK(view.current()->nodes[0].virtual_children->metadata_snapshot().get() ==
               initial_items.get());

    T068_CHECK(view.checkpoint() ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    T068_CHECK(!view.has_pending_publication());

    const auto after_value_change = view.current();
    T068_CHECK(after_value_change);
    T068_CHECK(after_value_change->generation == 2);
    T068_CHECK(after_value_change->nodes[0].virtual_children->dataset_generation() == 2);
    T068_CHECK(after_value_change->nodes[0].virtual_children->metadata_snapshot().get() ==
               updated_items.get());

    const auto replacement_items = metadata({{10, "Ten"}, {20, "Twenty updated"}});
    T068_CHECK(replacement_items.get() != updated_items.get());
    view.publisher()->fail_next_publish_at_for_test(FailurePoint::SnapshotAllocation);
    failed = false;
    try {
        (void)view.publish(snapshot(3, replacement_items, 10));
    } catch (const std::bad_alloc&) {
        failed = true;
    }

    T068_CHECK(failed);
    T068_CHECK(view.has_pending_publication());
    T068_CHECK(view.current().get() == after_value_change.get());
    T068_CHECK(view.current()->generation == 2);
    T068_CHECK(view.current()->nodes[0].virtual_children->dataset_generation() == 2);
    T068_CHECK(view.current()->nodes[0].virtual_children->metadata_snapshot().get() ==
               updated_items.get());

    T068_CHECK(view.checkpoint().empty());
    T068_CHECK(!view.has_pending_publication());

    const auto refreshed = view.current();
    T068_CHECK(refreshed);
    T068_CHECK(refreshed.get() != after_value_change.get());
    T068_CHECK(refreshed->generation == 2);
    T068_CHECK(refreshed->nodes[0].virtual_children->dataset_generation() == 3);
    T068_CHECK(refreshed->nodes[0].virtual_children->metadata_snapshot().get() ==
               replacement_items.get());
}

} // namespace

int main() {
    try {
        storage_only_index_refresh_keeps_semantic_generation();
        metadata_generation_alone_is_not_structure();
        reorder_does_not_manufacture_value_change();
        structure_change_can_coexist_with_selection_value_and_bounds();
        removing_focused_selected_node_keeps_state_notifications_in_structure_batch();
        adding_focused_selected_node_keeps_state_notifications_in_structure_batch();
        removing_selected_virtual_container_keeps_selection_notification();
        publication_failure_keeps_previous_generation_and_retries_pending_state();
        std::cout << "PASS t068 semantic diff regressions\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic diff regressions: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
