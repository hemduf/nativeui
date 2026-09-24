#include "../../src/detail/semantic_macos_children.hpp"

#include <nativeui/detail/semantic_native_publication.hpp>
#include <nativeui/detail/semantic_native_query.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

using ChildIdentity = ui::detail::MacOSAccessibilityChildIdentity;
using ChildProjection = ui::detail::MacOSAccessibilityChildProjection;
using NativePublicationState = ui::detail::SemanticNativePublicationState;

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_hierarchy_snapshot(
    std::uint64_t generation) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = ui::SemanticRole::Group;
    root.children = {2U, 7U};

    ui::SemanticNodeSnapshot first;
    first.id = 2U;
    first.parent = 1U;
    first.info.role = ui::SemanticRole::Button;

    ui::SemanticNodeSnapshot second;
    second.id = 7U;
    second.parent = 1U;
    second.info.role = ui::SemanticRole::ListView;

    snapshot->nodes.push_back(std::move(root));
    snapshot->nodes.push_back(std::move(first));
    snapshot->nodes.push_back(std::move(second));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_list_snapshot(
    std::uint64_t semantic_generation,
    std::uint64_t dataset_generation,
    std::size_t item_count,
    ui::VirtualSemanticItemToken first_token) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = semantic_generation;
    snapshot->root = 11U;

    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    metadata->reserve(item_count);
    token_index->reserve(item_count);

    for (std::size_t index = 0; index < item_count; ++index) {
        const auto token = static_cast<ui::VirtualSemanticItemToken>(
            first_token + static_cast<ui::VirtualSemanticItemToken>(index));
        ui::VirtualSemanticItemMetadata item;
        item.token = token;
        metadata->push_back(std::move(item));
        token_index->emplace(token, index);
    }

    ui::SemanticNodeSnapshot list;
    list.id = 11U;
    list.info.role = ui::SemanticRole::ListView;
    list.bounds = {0.0f, 0.0f, 200.0f, 300.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        dataset_generation,
        std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
        std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
        std::nullopt,
        list.bounds,
        20.0f,
        0.0f);

    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

ChildProjection current_projection(
    const NativePublicationState& publication_state,
    ui::SemanticId node_id) {
    const auto source = publication_state.reader_source().lock();
    CHECK(source != nullptr);
    auto publication = source->current();
    CHECK(publication != nullptr);
    auto read = ui::detail::SemanticNativeSnapshotQuery::ordinary(
        std::move(publication), node_id);
    CHECK(read.has_value());
    return ChildProjection{std::move(*read)};
}

ChildProjection current_virtual_projection(
    const NativePublicationState& publication_state,
    ui::SemanticId list_node_id,
    ui::VirtualSemanticItemToken token) {
    const auto source = publication_state.reader_source().lock();
    CHECK(source != nullptr);
    auto publication = source->current();
    CHECK(publication != nullptr);
    auto read = ui::detail::SemanticNativeSnapshotQuery::virtual_item(
        std::move(publication), list_node_id, token);
    CHECK(read.has_value());
    return ChildProjection{std::move(*read)};
}

void ordinary_children_preserve_ordered_stable_identity() {
    NativePublicationState publication_state;
    const auto batch = publication_state.publish(
        ordinary_hierarchy_snapshot(1U),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(batch.has_value());

    auto projection = current_projection(publication_state, 1U);
    CHECK(projection.generation() == batch->generation());
    CHECK(projection.semantic_generation() == 1U);
    CHECK(!projection.parent_identity().has_value());
    CHECK(!projection.parent_role().has_value());
    CHECK(projection.ordinary_child_count() == 2U);
    CHECK(projection.virtual_child_count() == 0U);

    const auto first = projection.ordinary_child_identity_at(0U);
    const auto second = projection.ordinary_child_identity_at(1U);
    CHECK((first == std::optional<ChildIdentity>{
        ChildIdentity{2U, std::nullopt}}));
    CHECK((second == std::optional<ChildIdentity>{
        ChildIdentity{7U, std::nullopt}}));
    CHECK(first->valid());
    CHECK(second->valid());
    CHECK(!projection.ordinary_child_identity_at(2U).has_value());
    CHECK(!projection.virtual_child_identity_at(0U).has_value());

    CHECK(projection.ordinary_child_index_of(2U) ==
          std::optional<std::size_t>{0U});
    CHECK(projection.ordinary_child_index_of(7U) ==
          std::optional<std::size_t>{1U});
    CHECK(!projection.ordinary_child_index_of(99U).has_value());
    CHECK(!projection.ordinary_child_index_of(ui::kInvalidSemanticId).has_value());

    CHECK(projection.ordinary_child_role_at(0U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::Button});
    CHECK(projection.ordinary_child_role_at(1U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListView});
    CHECK(!projection.ordinary_child_role_at(2U).has_value());
    CHECK(!projection.virtual_child_role_at(0U).has_value());
}

void parent_identity_stays_inside_the_retained_generation() {
    NativePublicationState publication_state;
    const auto first_batch = publication_state.publish(
        ordinary_hierarchy_snapshot(1U),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(first_batch.has_value());

    auto ordinary_child = current_projection(publication_state, 2U);
    CHECK((ordinary_child.parent_identity() ==
          std::optional<ChildIdentity>{ChildIdentity{1U, std::nullopt}}));
    CHECK(ordinary_child.parent_role() ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::Group});

    constexpr std::size_t item_count = 100000U;
    constexpr ui::VirtualSemanticItemToken first_token = 1000U;
    constexpr std::size_t middle_index = item_count / 2U;
    constexpr ui::VirtualSemanticItemToken middle_token =
        first_token + static_cast<ui::VirtualSemanticItemToken>(middle_index);

    const auto second_batch = publication_state.publish(
        virtual_list_snapshot(2U, 1U, item_count, first_token),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(second_batch.has_value());

    auto virtual_child = current_virtual_projection(
        publication_state, 11U, middle_token);
    CHECK((virtual_child.parent_identity() ==
          std::optional<ChildIdentity>{ChildIdentity{11U, std::nullopt}}));
    CHECK(virtual_child.parent_role() ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListView});
    CHECK(virtual_child.ordinary_child_count() == 0U);
    CHECK(virtual_child.virtual_child_count() == 0U);

    // The older ordinary child keeps its exact immutable parent relationship and
    // role after a successor publication replaces the current semantic root.
    CHECK(ordinary_child.generation() == first_batch->generation());
    CHECK(ordinary_child.semantic_generation() == 1U);
    CHECK((ordinary_child.parent_identity() ==
          std::optional<ChildIdentity>{ChildIdentity{1U, std::nullopt}}));
    CHECK(ordinary_child.parent_role() ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::Group});
}

void large_virtual_collection_exposes_indexed_identity_without_native_materialization() {
    NativePublicationState publication_state;
    constexpr std::size_t item_count = 100000U;
    constexpr ui::VirtualSemanticItemToken first_token = 1000U;
    constexpr std::size_t middle_index = item_count / 2U;

    const auto batch = publication_state.publish(
        virtual_list_snapshot(7U, 3U, item_count, first_token),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(batch.has_value());

    auto projection = current_projection(publication_state, 11U);
    CHECK(projection.ordinary_child_count() == 0U);
    CHECK(projection.virtual_child_count() == item_count);

    const auto first = projection.virtual_child_identity_at(0U);
    const auto last = projection.virtual_child_identity_at(item_count - 1U);
    CHECK(first.has_value());
    CHECK(last.has_value());
    CHECK(first->node_id == 11U);
    CHECK(first->virtual_token ==
          std::optional<ui::VirtualSemanticItemToken>{first_token});
    CHECK(last->node_id == 11U);
    CHECK(last->virtual_token == std::optional<ui::VirtualSemanticItemToken>{
        first_token + static_cast<ui::VirtualSemanticItemToken>(item_count - 1U)});
    CHECK(first->valid());
    CHECK(last->valid());
    CHECK(!projection.virtual_child_identity_at(item_count).has_value());

    const auto middle_token = static_cast<ui::VirtualSemanticItemToken>(
        first_token + static_cast<ui::VirtualSemanticItemToken>(middle_index));
    CHECK(projection.virtual_child_index_of(first_token) ==
          std::optional<std::size_t>{0U});
    CHECK(projection.virtual_child_index_of(middle_token) ==
          std::optional<std::size_t>{middle_index});
    CHECK(projection.virtual_child_index_of(
              first_token + static_cast<ui::VirtualSemanticItemToken>(item_count - 1U)) ==
          std::optional<std::size_t>{item_count - 1U});
    CHECK(!projection.virtual_child_index_of(
        first_token + static_cast<ui::VirtualSemanticItemToken>(item_count)).has_value());
    CHECK(!projection.virtual_child_index_of(
        ui::kInvalidVirtualSemanticItemToken).has_value());

    CHECK(projection.virtual_child_role_at(0U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListItem});
    CHECK(projection.virtual_child_role_at(item_count - 1U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListItem});
    CHECK(!projection.virtual_child_role_at(item_count).has_value());
}

void old_child_projection_retains_exact_dataset_generation() {
    NativePublicationState publication_state;
    constexpr ui::VirtualSemanticItemToken old_first_token = 50U;
    constexpr ui::VirtualSemanticItemToken new_token = 9000U;

    const auto first_batch = publication_state.publish(
        virtual_list_snapshot(1U, 1U, 2U, old_first_token),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(first_batch.has_value());
    auto old_projection = current_projection(publication_state, 11U);
    CHECK(old_projection.virtual_child_count() == 2U);

    const auto second_batch = publication_state.publish(
        virtual_list_snapshot(2U, 2U, 1U, new_token),
        {ui::SemanticChange::StructureChanged},
        {});
    CHECK(second_batch.has_value());
    auto current = current_projection(publication_state, 11U);
    CHECK(current.virtual_child_count() == 1U);
    const auto current_identity = current.virtual_child_identity_at(0U);
    CHECK(current_identity.has_value());
    CHECK(current_identity->virtual_token ==
          std::optional<ui::VirtualSemanticItemToken>{new_token});
    CHECK(current.virtual_child_index_of(new_token) ==
          std::optional<std::size_t>{0U});
    CHECK(!current.virtual_child_index_of(old_first_token).has_value());
    CHECK(current.virtual_child_role_at(0U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListItem});

    CHECK(old_projection.generation() == first_batch->generation());
    CHECK(old_projection.semantic_generation() == 1U);
    CHECK(old_projection.virtual_child_count() == 2U);
    const auto old_identity = old_projection.virtual_child_identity_at(1U);
    CHECK(old_identity.has_value());
    CHECK(old_identity->virtual_token ==
          std::optional<ui::VirtualSemanticItemToken>{old_first_token + 1U});
    CHECK(old_projection.virtual_child_index_of(old_first_token + 1U) ==
          std::optional<std::size_t>{1U});
    CHECK(!old_projection.virtual_child_index_of(new_token).has_value());
    CHECK(old_projection.virtual_child_role_at(1U) ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::ListItem});
}

} // namespace

int main() {
    try {
        ordinary_children_preserve_ordered_stable_identity();
        parent_identity_stays_inside_the_retained_generation();
        large_virtual_collection_exposes_indexed_identity_without_native_materialization();
        old_child_projection_retains_exact_dataset_generation();
        std::cout << "PASS semantic macOS child projection\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic macOS child projection: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}
