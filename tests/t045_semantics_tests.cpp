#include "test_support.hpp"

#include <nativeui/semantics.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

void semantic_role_and_action_contract() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Button;
    info.name = "Apply";
    info.enabled = true;
    info.focusable = true;
    info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};

    NUI_CHECK(info.role == ui::SemanticRole::Button);
    NUI_CHECK(info.name == "Apply");
    NUI_CHECK(info.enabled);
    NUI_CHECK(info.focusable);
    NUI_CHECK(std::find(info.actions.begin(), info.actions.end(), ui::SemanticAction::Activate) !=
              info.actions.end());
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
    NUI_CHECK(snapshot.info.name == "Original");
    NUI_CHECK(snapshot.children.size() == 2);
    NUI_CHECK(snapshot.bounds.w == 30.0f);
}

void virtual_collection_is_snapshot_based() {
    std::vector<ui::VirtualSemanticItem> items;
    items.reserve(100000);
    for (std::uint64_t index = 0; index < 100000; ++index) {
        ui::VirtualSemanticItem item;
        item.id = ui::SemanticId{index + 1};
        item.name = "Item";
        item.enabled = true;
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        items.push_back(std::move(item));
    }

    auto snapshot = ui::VirtualSemanticChildren::from_items(std::move(items));
    NUI_CHECK(snapshot.size() == 100000);
    const auto middle = snapshot.item_at(50000);
    NUI_CHECK(middle.has_value());
    NUI_CHECK(middle->id == ui::SemanticId{50001});
    NUI_CHECK(middle->name == "Item");
    NUI_CHECK(!snapshot.item_at(100000).has_value());
}

void checked_and_value_contract() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Checkbox;
    info.checked = ui::SemanticCheckedState::Mixed;
    info.numeric_value = 0.5;
    info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};

    NUI_CHECK(info.checked == ui::SemanticCheckedState::Mixed);
    NUI_CHECK(info.numeric_value.has_value());
    NUI_CHECK(info.value_range.has_value());
    NUI_CHECK(info.value_range->minimum == 0.0);
    NUI_CHECK(info.value_range->maximum == 1.0);
}

} // namespace

int main() {
    return test::run("t045 semantics", [] {
        semantic_role_and_action_contract();
        semantic_snapshot_owns_data();
        virtual_collection_is_snapshot_based();
        checked_and_value_contract();
    });
}
