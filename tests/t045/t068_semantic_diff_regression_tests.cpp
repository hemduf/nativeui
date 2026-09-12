#include <nativeui/detail/semantic_snapshot.hpp>
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
    float scroll_y = 0.0f) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 1;

    ui::SemanticNodeSnapshot list;
    list.id = 1;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.bounds = {0.0f, 0.0f, 100.0f, 40.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
        dataset_generation,
        std::move(items),
        selected,
        list.bounds,
        20.0f,
        scroll_y);
    tree.nodes.push_back(std::move(list));
    return tree;
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

} // namespace

int main() {
    try {
        metadata_generation_alone_is_not_structure();
        reorder_does_not_manufacture_value_change();
        structure_change_can_coexist_with_selection_value_and_bounds();
        std::cout << "PASS t068 semantic diff regressions\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic diff regressions: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
