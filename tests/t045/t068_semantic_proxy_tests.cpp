#include <nativeui/detail/semantic_proxy.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>
#include <nativeui/semantics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
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

ui::SemanticTreeSnapshot ordinary_snapshot(std::string name) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 42;

    ui::SemanticNodeSnapshot node;
    node.id = 42;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = std::move(name);
    node.info.enabled = true;
    node.info.focusable = true;
    node.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    node.bounds = {10.0f, 20.0f, 80.0f, 24.0f};
    tree.nodes.push_back(std::move(node));
    return tree;
}

void ordinary_proxy_reads_current_snapshot_and_keeps_old_read_alive() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();
    T068_CHECK(publisher->publish(ordinary_snapshot("Apply")) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});

    auto proxy = ui::detail::SemanticSnapshotProxy::ordinary(publisher, 42);
    const auto first = proxy.read();
    T068_CHECK(first.has_value());
    T068_CHECK(first->generation() == 1);
    T068_CHECK(first->node_id() == 42);
    T068_CHECK(!first->virtual_token().has_value());
    T068_CHECK(first->info().name == "Apply");
    T068_CHECK(first->bounds().x == 10.0f);

    auto changed = ordinary_snapshot("Apply now");
    changed.nodes[0].bounds.x = 12.0f;
    T068_CHECK(publisher->publish(std::move(changed)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::ValueChanged,
                                               ui::SemanticChange::BoundsChanged});

    const auto second = proxy.read();
    T068_CHECK(second.has_value());
    T068_CHECK(second->generation() == 2);
    T068_CHECK(second->info().name == "Apply now");
    T068_CHECK(second->bounds().x == 12.0f);

    // The first read owns its immutable generation. Publishing a replacement
    // cannot mutate or invalidate data a native reader already retained.
    T068_CHECK(first->generation() == 1);
    T068_CHECK(first->info().name == "Apply");
    T068_CHECK(first->bounds().x == 10.0f);

    ui::SemanticTreeSnapshot removed;
    T068_CHECK(publisher->publish(std::move(removed)) ==
               std::vector<ui::SemanticChange>{ui::SemanticChange::StructureChanged});
    T068_CHECK(!proxy.read().has_value());

    publisher.reset();
    T068_CHECK(!proxy.read().has_value());
    T068_CHECK(first->info().name == "Apply");
}

ui::SemanticTreeSnapshot virtual_snapshot(
    std::uint64_t dataset_generation,
    ui::VirtualSemanticChildren::MetadataSnapshot metadata,
    std::optional<ui::VirtualSemanticItemToken> selected,
    float scroll_y) {
    ui::SemanticTreeSnapshot tree;
    tree.root = 7;

    ui::SemanticNodeSnapshot list;
    list.id = 7;
    list.info.role = ui::SemanticRole::ListView;
    list.info.name = "Items";
    list.bounds = {0.0f, 0.0f, 120.0f, 40.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_metadata(
        dataset_generation, std::move(metadata), selected,
        list.bounds, 20.0f, scroll_y);
    tree.nodes.push_back(std::move(list));
    return tree;
}

void virtual_proxy_follows_token_across_reorder_and_becomes_defunct_on_removal() {
    auto publisher = std::make_shared<ui::detail::SemanticSnapshotPublisher>();

    auto first_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    first_metadata->push_back({10, "Ten", "", true, false,
                               ui::SemanticCheckedState::NotApplicable,
                               {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    first_metadata->push_back({20, "Twenty", "", true, false,
                               ui::SemanticCheckedState::NotApplicable,
                               {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    T068_CHECK(!publisher->publish(virtual_snapshot(1, first_metadata, 20, 0.0f)).empty());

    auto proxy = ui::detail::SemanticSnapshotProxy::virtual_item(publisher, 7, 20);
    const auto first = proxy.read();
    T068_CHECK(first.has_value());
    T068_CHECK(first->node_id() == 7);
    T068_CHECK(first->virtual_token() == std::optional<ui::VirtualSemanticItemToken>{20});
    T068_CHECK(first->info().name == "Twenty");
    T068_CHECK(first->info().selected);
    T068_CHECK(first->bounds().y == 20.0f);

    auto reordered_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    reordered_metadata->push_back({20, "Twenty reordered", "", true, false,
                                   ui::SemanticCheckedState::NotApplicable,
                                   {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    reordered_metadata->push_back({10, "Ten", "", true, false,
                                   ui::SemanticCheckedState::NotApplicable,
                                   {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    T068_CHECK(!publisher->publish(virtual_snapshot(2, reordered_metadata, 10, 5.0f)).empty());

    const auto second = proxy.read();
    T068_CHECK(second.has_value());
    T068_CHECK(second->generation() == 2);
    T068_CHECK(second->virtual_token() == std::optional<ui::VirtualSemanticItemToken>{20});
    T068_CHECK(second->info().name == "Twenty reordered");
    T068_CHECK(!second->info().selected);
    T068_CHECK(second->bounds().y == -5.0f);

    T068_CHECK(first->generation() == 1);
    T068_CHECK(first->info().name == "Twenty");
    T068_CHECK(first->bounds().y == 20.0f);

    auto removed_metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    removed_metadata->push_back({10, "Ten", "", true, false,
                                 ui::SemanticCheckedState::NotApplicable,
                                 {ui::SemanticAction::Select, ui::SemanticAction::Focus}});
    T068_CHECK(!publisher->publish(virtual_snapshot(3, removed_metadata, 10, 0.0f)).empty());
    T068_CHECK(!proxy.read().has_value());
}

} // namespace

int main() {
    try {
        ordinary_proxy_reads_current_snapshot_and_keeps_old_read_alive();
        virtual_proxy_follows_token_across_reorder_and_becomes_defunct_on_removal();
        std::cout << "PASS t068 semantic proxy\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic proxy: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
