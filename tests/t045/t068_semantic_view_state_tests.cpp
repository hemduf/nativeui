#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_view_state.hpp>
#include <nativeui/semantics.hpp>

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

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

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

ui::SemanticTreeSnapshot virtual_snapshot(
    std::uint64_t dataset_generation,
    ui::VirtualSemanticChildren::MetadataSnapshot metadata,
    std::optional<ui::VirtualSemanticItemToken> selected = ui::VirtualSemanticItemToken{20},
    float scroll_y = 0.0f,
    bool focused = false) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 7;

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.info.focused = focused;
    list.bounds = {0.0f, 0.0f, 120.0f, 40.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
        dataset_generation,
        std::move(metadata),
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

    T068_CHECK(first->publish(snapshot(11, "First")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(second->publish(snapshot(22, "Second")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto first_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        first->publisher(), 11);
    const auto second_proxy = ui::detail::SemanticSnapshotProxy::ordinary(
        second->publisher(), 22);

    T068_CHECK(first_proxy.read().has_value());
    T068_CHECK(first_proxy.read()->info().name == "First");
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->info().name == "Second");

    first.reset();
    T068_CHECK(!first_proxy.read().has_value());
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->info().name == "Second");

    T068_CHECK(second->publish(snapshot(22, "Second updated")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
    T068_CHECK(second_proxy.read().has_value());
    T068_CHECK(second_proxy.read()->generation() == 2);
    T068_CHECK(second_proxy.read()->info().name == "Second updated");
}

void no_change_publish_preserves_generation_and_snapshot_identity() {
    ui::detail::SemanticViewState view;
    T068_CHECK(!view.publish(snapshot(7, "Stable")).empty());

    const auto first = view.current();
    T068_CHECK(first);
    T068_CHECK(first->generation == 1);

    T068_CHECK(view.publish(snapshot(7, "Stable")).empty());
    const auto second = view.current();
    T068_CHECK(second.get() == first.get());
    T068_CHECK(second->generation == 1);
}

void identical_dataset_replacement_refreshes_backing_storage_without_semantic_generation() {
    ui::detail::SemanticViewState view;
    const auto first_metadata = virtual_metadata();
    T068_CHECK(view.publish(virtual_snapshot(1, first_metadata)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    const auto first = view.current();
    T068_CHECK(first);
    T068_CHECK(first->generation == 1);
    T068_CHECK(first->nodes.size() == 1);
    T068_CHECK(first->nodes[0].virtual_children.has_value());
    T068_CHECK(first->nodes[0].virtual_children->dataset_generation() == 1);
    T068_CHECK(first->nodes[0].virtual_children->metadata_snapshot().get() ==
               first_metadata.get());

    // Model an explicit dataset replacement that happens to expose identical
    // accessibility values. T068 must publish the new T067 immutable metadata
    // object so subsequent native readers use the current dataset generation,
    // while preserving the semantic generation because no exposed data changed.
    const auto replacement_metadata = virtual_metadata();
    T068_CHECK(replacement_metadata.get() != first_metadata.get());
    T068_CHECK(view.publish(virtual_snapshot(2, replacement_metadata)).empty());

    const auto second = view.current();
    T068_CHECK(second);
    T068_CHECK(second.get() != first.get());
    T068_CHECK(second->generation == first->generation);
    T068_CHECK(second->nodes[0].virtual_children.has_value());
    T068_CHECK(second->nodes[0].virtual_children->dataset_generation() == 2);
    T068_CHECK(second->nodes[0].virtual_children->metadata_snapshot().get() ==
               replacement_metadata.get());

    // A reader that retained the old immutable generation remains valid and
    // continues to own the original metadata object after publication swaps.
    T068_CHECK(first->nodes[0].virtual_children->dataset_generation() == 1);
    T068_CHECK(first->nodes[0].virtual_children->metadata_snapshot().get() ==
               first_metadata.get());
    T068_CHECK(first->nodes[0].virtual_children->item_at(1)->info.name == "Twenty");
    T068_CHECK(second->nodes[0].virtual_children->item_at(1)->info.name == "Twenty");
}

void large_virtual_dataset_reuses_exact_metadata_across_1000_semantic_publications() {
    constexpr std::size_t kItemCount = 100000;
    constexpr std::uint64_t kDatasetGeneration = 41;

    auto mutable_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    mutable_metadata->reserve(kItemCount);
    for (std::size_t index = 0; index < kItemCount; ++index) {
        mutable_metadata->push_back({
            static_cast<ui::VirtualSemanticItemToken>(index + 1),
            {},
            {},
            true,
            false,
            ui::SemanticCheckedState::NotApplicable,
            {ui::SemanticAction::Select, ui::SemanticAction::Focus},
        });
    }
    const ui::VirtualSemanticChildren::MetadataSnapshot shared_metadata = mutable_metadata;

    ui::detail::SemanticViewState view;
    T068_CHECK(view.publish(virtual_snapshot(
                   kDatasetGeneration,
                   shared_metadata,
                   ui::VirtualSemanticItemToken{1},
                   0.0f,
                   false)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    for (std::size_t iteration = 1; iteration <= 1000; ++iteration) {
        const auto selected = iteration % 2 == 0
            ? ui::VirtualSemanticItemToken{1}
            : ui::VirtualSemanticItemToken{kItemCount};
        const float scroll_y = static_cast<float>(iteration) * 0.5f;
        const bool focused = iteration % 2 != 0;

        const auto changes = view.publish(virtual_snapshot(
            kDatasetGeneration,
            shared_metadata,
            selected,
            scroll_y,
            focused));
        const std::vector<ui::SemanticChange> expected{
            ui::SemanticChange::FocusChanged,
            ui::SemanticChange::SelectionChanged,
            ui::SemanticChange::BoundsChanged,
        };
        T068_CHECK(changes == expected);

        const auto current = view.current();
        T068_CHECK(current);
        T068_CHECK(current->generation == iteration + 1);
        T068_CHECK(current->nodes.size() == 1);
        T068_CHECK(current->nodes[0].virtual_children.has_value());
        const auto& virtual_children = *current->nodes[0].virtual_children;
        T068_CHECK(virtual_children.dataset_generation() == kDatasetGeneration);
        T068_CHECK(virtual_children.size() == kItemCount);
        T068_CHECK(virtual_children.metadata_snapshot().get() == shared_metadata.get());
    }

    const auto final_snapshot = view.current();
    T068_CHECK(final_snapshot);
    T068_CHECK(final_snapshot->generation == 1001);
    T068_CHECK(final_snapshot->nodes[0].virtual_children->metadata_snapshot().get() ==
               shared_metadata.get());
}

} // namespace

int main() {
    try {
        independent_view_state_destroy_a_keeps_b_alive();
        no_change_publish_preserves_generation_and_snapshot_identity();
        identical_dataset_replacement_refreshes_backing_storage_without_semantic_generation();
        large_virtual_dataset_reuses_exact_metadata_across_1000_semantic_publications();
        std::cout << "PASS t068 semantic view state\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic view state: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
