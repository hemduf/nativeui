#include "test_support.hpp"

#include <nativeui/semantics.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
    NUI_CHECK(info.supports(ui::SemanticAction::Activate));
    NUI_CHECK(info.supports(ui::SemanticAction::Focus));
    NUI_CHECK(!info.supports(ui::SemanticAction::SetValue));
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

struct VirtualMetadata {
    struct Entry {
        ui::VirtualSemanticItemToken token{};
        std::string name;
        bool enabled{true};
    };

    std::uint64_t generation{};
    std::vector<Entry> entries;
};

class FixedHeightVirtualSnapshot final : public ui::VirtualSemanticChildren {
public:
    FixedHeightVirtualSnapshot(std::shared_ptr<const VirtualMetadata> metadata,
                               std::optional<ui::VirtualSemanticItemToken> selected,
                               float row_height,
                               float scroll_y,
                               int& visual_factory_calls)
        : metadata_(std::move(metadata)),
          selected_(selected),
          row_height_(row_height),
          scroll_y_(scroll_y),
          visual_factory_calls_(&visual_factory_calls) {}

    [[nodiscard]] std::uint64_t dataset_generation() const noexcept override {
        return metadata_->generation;
    }

    [[nodiscard]] std::size_t size() const noexcept override {
        return metadata_->entries.size();
    }

    [[nodiscard]] std::optional<ui::VirtualSemanticItem> item_at(
        std::size_t index) const override {
        if (index >= metadata_->entries.size()) {
            return std::nullopt;
        }

        const auto& entry = metadata_->entries[index];
        ui::VirtualSemanticItem item;
        item.token = entry.token;
        item.info.role = ui::SemanticRole::ListItem;
        item.info.name = entry.name;
        item.info.enabled = entry.enabled;
        item.info.selected = selected_.has_value() && *selected_ == entry.token;
        item.info.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        item.logical_bounds = {
            0.0f,
            static_cast<float>(index) * row_height_ - scroll_y_,
            200.0f,
            row_height_,
        };
        return item;
    }

    [[nodiscard]] std::optional<std::size_t> index_of_selected_item() const noexcept override {
        if (!selected_.has_value()) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < metadata_->entries.size(); ++index) {
            if (metadata_->entries[index].token == *selected_) {
                return index;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] const VirtualMetadata* metadata_address() const noexcept {
        return metadata_.get();
    }

    [[nodiscard]] int visual_factory_calls() const noexcept {
        return *visual_factory_calls_;
    }

private:
    std::shared_ptr<const VirtualMetadata> metadata_;
    std::optional<ui::VirtualSemanticItemToken> selected_;
    float row_height_{};
    float scroll_y_{};
    int* visual_factory_calls_{};
};

void virtual_collection_is_lazy_and_metadata_shared() {
    auto metadata = std::make_shared<VirtualMetadata>();
    metadata->generation = 7;
    metadata->entries.reserve(100000);
    for (std::uint64_t index = 0; index < 100000; ++index) {
        metadata->entries.push_back({index + 1, "Item", true});
    }

    int visual_factory_calls = 0;
    auto first = std::make_shared<const FixedHeightVirtualSnapshot>(
        metadata, ui::VirtualSemanticItemToken{50001}, 20.0f, 100.0f, visual_factory_calls);
    auto second = std::make_shared<const FixedHeightVirtualSnapshot>(
        metadata, ui::VirtualSemanticItemToken{50002}, 20.0f, 120.0f, visual_factory_calls);

    ui::VirtualSemanticChildrenSnapshot first_view = first;
    ui::VirtualSemanticChildrenSnapshot second_view = second;

    NUI_CHECK(first_view->size() == 100000);
    NUI_CHECK(first_view->dataset_generation() == 7);
    NUI_CHECK(second_view->dataset_generation() == 7);
    NUI_CHECK(first->metadata_address() == second->metadata_address());

    const auto middle = first_view->item_at(50000);
    NUI_CHECK(middle.has_value());
    NUI_CHECK(middle->token == ui::VirtualSemanticItemToken{50001});
    NUI_CHECK(middle->info.name == "Item");
    NUI_CHECK(middle->info.selected);
    NUI_CHECK(middle->logical_bounds.y == 999900.0f);
    NUI_CHECK(first_view->index_of_selected_item() == std::optional<std::size_t>{50000});
    NUI_CHECK(!first_view->item_at(100000).has_value());
    NUI_CHECK(first->visual_factory_calls() == 0);
    NUI_CHECK(second->visual_factory_calls() == 0);
}

void virtual_tokens_and_tristate_contract() {
    NUI_CHECK(ui::kInvalidVirtualSemanticItemToken == 0);

    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Checkbox;
    info.checked = ui::SemanticCheckedState::Mixed;
    info.expanded = ui::SemanticExpandedState::Collapsed;
    info.numeric_value = 0.5;
    info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};

    NUI_CHECK(info.checked == ui::SemanticCheckedState::Mixed);
    NUI_CHECK(info.expanded == ui::SemanticExpandedState::Collapsed);
    NUI_CHECK(info.numeric_value.has_value());
    NUI_CHECK(info.value_range.has_value());
    NUI_CHECK(info.value_range->minimum == 0.0);
    NUI_CHECK(info.value_range->maximum == 1.0);
}

void semantic_tree_generation_contract() {
    ui::SemanticTreeSnapshot tree;
    tree.generation = 12;
    tree.root = 1;
    tree.nodes.push_back(ui::SemanticNodeSnapshot{.id = 1});

    NUI_CHECK(tree.generation == 12);
    NUI_CHECK(tree.root == 1);
    NUI_CHECK(tree.nodes.size() == 1);
}

} // namespace

int main() {
    return test::run("t045 semantics", [] {
        semantic_role_and_action_contract();
        semantic_snapshot_owns_data();
        virtual_collection_is_lazy_and_metadata_shared();
        virtual_tokens_and_tristate_contract();
        semantic_tree_generation_contract();
    });
}
