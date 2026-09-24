#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

ui::SemanticTreeSnapshot hierarchy_snapshot() {
    ui::SemanticTreeSnapshot tree;
    tree.root = 1;

    ui::SemanticNodeSnapshot root;
    root.id = 1;
    root.parent = ui::kInvalidSemanticId;
    root.info.role = ui::SemanticRole::Group;
    root.info.name = "Root";
    root.children = {2, 7};

    ui::SemanticNodeSnapshot child;
    child.id = 2;
    child.parent = 1;
    child.info.role = ui::SemanticRole::Button;
    child.info.name = "Apply";
    child.info.actions = {ui::SemanticAction::Activate};

    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    metadata->push_back({10, "Ten", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    metadata->push_back({20, "Twenty", "", true, false,
                         ui::SemanticCheckedState::NotApplicable,
                         {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    token_index->emplace(10, 0);
    token_index->emplace(20, 1);

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.parent = 1;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        3, metadata, token_index, ui::VirtualSemanticItemToken{20},
        {0.0f, 20.0f, 120.0f, 40.0f}, 20.0f, 0.0f);

    tree.nodes.push_back(std::move(root));
    tree.nodes.push_back(std::move(child));
    tree.nodes.push_back(std::move(list));
    return tree;
}

void ordinary_reads_expose_parent_and_ordered_children() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(hierarchy_snapshot()).empty());

    const auto root = ui::detail::SemanticSnapshotProxy::ordinary(publisher, 1).read();
    T068_CHECK(root.has_value());
    T068_CHECK(root->parent_id() == ui::kInvalidSemanticId);
    T068_CHECK(root->child_count() == 2);
    T068_CHECK(root->child_at(0) == std::optional<ui::SemanticId>{2});
    T068_CHECK(root->child_at(1) == std::optional<ui::SemanticId>{7});
    T068_CHECK(!root->child_at(2).has_value());
    T068_CHECK(root->virtual_child_count() == 0);
    T068_CHECK(!root->virtual_child_token_at(0).has_value());

    const auto child = ui::detail::SemanticSnapshotProxy::ordinary(publisher, 2).read();
    T068_CHECK(child.has_value());
    T068_CHECK(child->parent_id() == 1);
    T068_CHECK(child->child_count() == 0);
    T068_CHECK(!child->child_at(0).has_value());
}

void virtual_collection_reads_expose_tokens_without_materialization() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(!publisher->publish(hierarchy_snapshot()).empty());

    const auto list = ui::detail::SemanticSnapshotProxy::ordinary(publisher, 7).read();
    T068_CHECK(list.has_value());
    T068_CHECK(list->parent_id() == 1);
    T068_CHECK(list->child_count() == 0);
    T068_CHECK(list->virtual_child_count() == 2);
    T068_CHECK(list->virtual_child_token_at(0) ==
               std::optional<ui::VirtualSemanticItemToken>{10});
    T068_CHECK(list->virtual_child_token_at(1) ==
               std::optional<ui::VirtualSemanticItemToken>{20});
    T068_CHECK(!list->virtual_child_token_at(2).has_value());

    const auto item =
        ui::detail::SemanticSnapshotProxy::virtual_item(publisher, 7, 20).read();
    T068_CHECK(item.has_value());
    T068_CHECK(item->parent_id() == 7);
    T068_CHECK(item->child_count() == 0);
    T068_CHECK(item->virtual_child_count() == 0);
    T068_CHECK(!item->child_at(0).has_value());
    T068_CHECK(!item->virtual_child_token_at(0).has_value());
}

} // namespace

int main() {
    try {
        ordinary_reads_expose_parent_and_ordered_children();
        virtual_collection_reads_expose_tokens_without_materialization();
        std::cout << "PASS t068 semantic children query\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic children query: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
