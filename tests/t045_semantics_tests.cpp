#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/semantics.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
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

#define T045_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

void semantic_role_and_action_contract() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Button;
    info.name = "Apply";
    info.enabled = true;
    info.focusable = true;
    info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};

    T045_CHECK(info.role == ui::SemanticRole::Button);
    T045_CHECK(info.name == "Apply");
    T045_CHECK(info.enabled);
    T045_CHECK(info.focusable);
    T045_CHECK(info.supports(ui::SemanticAction::Activate));
    T045_CHECK(info.supports(ui::SemanticAction::Focus));
    T045_CHECK(!info.supports(ui::SemanticAction::SetValue));
}

void semantic_snapshot_owns_data() {
    std::string source_name = "Original";
    ui::SemanticNodeSnapshot snapshot;
    snapshot.id = ui::SemanticId{17};
    snapshot.parent = ui::kInvalidSemanticId;
    snapshot.bounds = {1.0f, 2.0f, 30.0f, 40.0f};
    snapshot.info.role = ui::SemanticRole::Text;
    snapshot.info.name = source_name;
    snapshot.children = {ui::SemanticId{18}, ui::SemanticId{19}};

    source_name = "Mutated";
    T045_CHECK(snapshot.info.name == "Original");
    T045_CHECK(snapshot.children.size() == 2);
    T045_CHECK(snapshot.bounds.w == 30.0f);
}

void virtual_collection_is_lazy_and_metadata_shared() {
    auto mutable_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    mutable_metadata->reserve(100000);
    for (std::uint64_t index = 0; index < 100000; ++index) {
        ui::VirtualSemanticItemMetadata item;
        item.token = index + 1;
        item.name = "Item";
        item.enabled = true;
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        mutable_metadata->push_back(std::move(item));
    }

    ui::VirtualSemanticChildren::MetadataSnapshot metadata = mutable_metadata;
    const auto first = ui::VirtualSemanticChildren::from_metadata(
        7, metadata, ui::VirtualSemanticItemToken{50001},
        {10.0f, 20.0f, 200.0f, 400.0f}, 20.0f, 100.0f);
    const auto second = ui::VirtualSemanticChildren::from_metadata(
        7, metadata, ui::VirtualSemanticItemToken{50002},
        {10.0f, 20.0f, 200.0f, 400.0f}, 20.0f, 120.0f);

    T045_CHECK(first.size() == 100000);
    T045_CHECK(first.dataset_generation() == 7);
    T045_CHECK(second.dataset_generation() == 7);
    T045_CHECK(first.metadata_snapshot().get() == second.metadata_snapshot().get());

    const auto middle = first.item_at(50000);
    T045_CHECK(middle.has_value());
    T045_CHECK(middle->token == ui::VirtualSemanticItemToken{50001});
    T045_CHECK(middle->info.name == "Item");
    T045_CHECK(middle->info.selected);
    T045_CHECK(middle->logical_bounds.x == 10.0f);
    T045_CHECK(middle->logical_bounds.y == 999920.0f);
    T045_CHECK(middle->logical_bounds.w == 200.0f);
    T045_CHECK(middle->logical_bounds.h == 20.0f);
    T045_CHECK(first.index_of_selected_item() == std::optional<std::size_t>{50000});
    T045_CHECK(!first.item_at(100000).has_value());
}

void virtual_metadata_is_immutable_after_publication() {
    auto mutable_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    mutable_metadata->push_back({1, "Before", "", true, false,
                                 ui::SemanticCheckedState::NotApplicable,
                                 {ui::SemanticAction::Select}});

    ui::VirtualSemanticChildren::MetadataSnapshot metadata = mutable_metadata;
    const auto snapshot = ui::VirtualSemanticChildren::from_metadata(
        1, metadata, std::nullopt, {0.0f, 0.0f, 100.0f, 20.0f}, 20.0f, 0.0f);

    mutable_metadata.reset();
    const auto item = snapshot.item_at(0);
    T045_CHECK(item.has_value());
    T045_CHECK(item->info.name == "Before");
    T045_CHECK(snapshot.metadata_snapshot().use_count() >= 1);
}

void virtual_tokens_and_tristate_contract() {
    T045_CHECK(ui::kInvalidVirtualSemanticItemToken == 0);

    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Checkbox;
    info.checked = ui::SemanticCheckedState::Mixed;
    info.expanded = ui::SemanticExpandedState::Collapsed;
    info.numeric_value = 0.5;
    info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};

    T045_CHECK(info.checked == ui::SemanticCheckedState::Mixed);
    T045_CHECK(info.expanded == ui::SemanticExpandedState::Collapsed);
    T045_CHECK(info.numeric_value.has_value());
    T045_CHECK(info.value_range.has_value());
    T045_CHECK(info.value_range->minimum == 0.0);
    T045_CHECK(info.value_range->maximum == 1.0);
}

void semantic_tree_generation_contract() {
    ui::SemanticTreeSnapshot tree;
    tree.generation = 12;
    tree.root = 1;
    ui::SemanticNodeSnapshot root;
    root.id = 1;
    tree.nodes.push_back(std::move(root));

    T045_CHECK(tree.generation == 12);
    T045_CHECK(tree.root == 1);
    T045_CHECK(tree.nodes.size() == 1);
}

ui::SemanticTreeSnapshot make_diff_snapshot() {
    ui::SemanticTreeSnapshot tree;
    tree.generation = 3;
    tree.root = 1;

    ui::SemanticNodeSnapshot root;
    root.id = 1;
    root.parent = ui::kInvalidSemanticId;
    root.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
    root.info.role = ui::SemanticRole::Button;
    root.info.name = "Apply";
    root.info.enabled = true;
    root.info.focusable = true;
    root.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    tree.nodes.push_back(std::move(root));
    return tree;
}

void semantic_snapshot_diff_ignores_generation_only_changes() {
    auto before = make_diff_snapshot();
    auto after = before;
    after.generation = 99;

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    T045_CHECK(changes.empty());
}

void semantic_snapshot_diff_classifies_exposed_changes_once() {
    const auto before = make_diff_snapshot();
    auto after = before;

    ui::SemanticNodeSnapshot child;
    child.id = 2;
    child.parent = 1;
    child.bounds = {0.0f, 20.0f, 100.0f, 20.0f};
    child.info.role = ui::SemanticRole::Text;
    child.info.name = "Status";
    after.nodes.push_back(child);
    after.nodes[0].children.push_back(2);

    after.nodes[0].info.focused = true;
    after.nodes[0].info.selected = true;
    after.nodes[0].info.name = "Apply now";
    after.nodes[0].bounds.x = 4.0f;

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    const std::vector<ui::SemanticChange> expected{
        ui::SemanticChange::StructureChanged,
        ui::SemanticChange::FocusChanged,
        ui::SemanticChange::SelectionChanged,
        ui::SemanticChange::ValueChanged,
        ui::SemanticChange::BoundsChanged,
    };
    T045_CHECK(changes == expected);
}

void semantic_snapshot_diff_coalesces_multiple_value_fields() {
    const auto before = make_diff_snapshot();
    auto after = before;
    after.nodes[0].info.enabled = false;
    after.nodes[0].info.read_only = true;
    after.nodes[0].info.description = "Unavailable while processing";
    after.nodes[0].info.numeric_value = 0.5;
    after.nodes[0].info.checked = ui::SemanticCheckedState::Mixed;
    after.nodes[0].info.expanded = ui::SemanticExpandedState::Expanded;
    after.nodes[0].info.actions = {ui::SemanticAction::Focus};

    const auto changes = ui::detail::diff_semantic_snapshots(before, after);
    T045_CHECK(changes == std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged});
}

} // namespace

int main() {
    try {
        semantic_role_and_action_contract();
        semantic_snapshot_owns_data();
        virtual_collection_is_lazy_and_metadata_shared();
        virtual_metadata_is_immutable_after_publication();
        virtual_tokens_and_tristate_contract();
        semantic_tree_generation_contract();
        semantic_snapshot_diff_ignores_generation_only_changes();
        semantic_snapshot_diff_classifies_exposed_changes_once();
        semantic_snapshot_diff_coalesces_multiple_value_fields();
        std::cout << "PASS t045 semantics\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t045 semantics: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}