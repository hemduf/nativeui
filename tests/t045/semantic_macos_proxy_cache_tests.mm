#include "../../src/detail/semantic_macos_proxy_cache.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);             \
        }                                                                       \
    } while (false)

using NativePublicationState = ui::detail::SemanticNativePublicationState;
using ProxyCache = ui::detail::MacOSAccessibilityProxyCache;
using ProxyCacheEndpoint = ui::detail::MacOSAccessibilityProxyCacheEndpoint;

Class test_anchor_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    bool include_child,
    ui::SemanticRole child_role = ui::SemanticRole::Button,
    ui::SemanticRole root_role = ui::SemanticRole::Group) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = root_role;
    if (include_child) {
        root.children.push_back(2U);
    }
    snapshot->nodes.push_back(std::move(root));

    if (include_child) {
        ui::SemanticNodeSnapshot child;
        child.id = 2U;
        child.parent = 1U;
        child.info.role = child_role;
        child.info.name = "action";
        snapshot->nodes.push_back(std::move(child));
    }
    return snapshot;
}

std::shared_ptr<ui::detail::SemanticActionViewEndpoint> inert_action_endpoint() {
    return std::make_shared<ui::detail::SemanticActionViewEndpoint>(
        std::weak_ptr<ui::detail::SemanticSnapshotPublisher>{},
        ui::Dispatcher{},
        std::weak_ptr<ui::detail::SemanticActionTarget>{},
        std::weak_ptr<const void>{});
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_snapshot(
    std::uint64_t generation,
    std::size_t count,
    ui::VirtualSemanticItemToken first_token) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 11U;

    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    metadata->reserve(count);
    token_index->reserve(count);

    for (std::size_t index = 0U; index < count; ++index) {
        const auto token = static_cast<ui::VirtualSemanticItemToken>(
            first_token + static_cast<ui::VirtualSemanticItemToken>(index));
        ui::VirtualSemanticItemMetadata item;
        item.token = token;
        item.name = "row";
        item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        metadata->push_back(std::move(item));
        token_index->emplace(token, index);
    }

    ui::SemanticNodeSnapshot list;
    list.id = 11U;
    list.info.role = ui::SemanticRole::ListView;
    list.bounds = {0.0f, 0.0f, 200.0f, 300.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        generation,
        std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
        std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
        std::nullopt,
        list.bounds,
        20.0f,
        0.0f);
    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

void ordinary_identity_is_stable_per_view_and_stale_entries_are_evicted() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_first_515151515151_PuglWrapperView");
    ProxyCache first_view{anchor, publication_state.reader_source()};
    ProxyCache second_view{anchor, publication_state.reader_source()};

    CHECK(first_view.ordinary_child_count(1U) ==
          std::optional<std::size_t>{1U});
    CHECK(first_view.virtual_child_count(1U) ==
          std::optional<std::size_t>{0U});
    CHECK(first_view.tracked_identities() == 0U);

    NSAccessibilityElement* first = first_view.ordinary(2U);
    NSAccessibilityElement* first_again = first_view.ordinary(2U);
    NSAccessibilityElement* indexed = first_view.ordinary_child_at(1U, 0U);
    NSAccessibilityElement* other_view = second_view.ordinary(2U);
    CHECK(first != nil);
    CHECK(first_again == first);
    CHECK(indexed == first);
    CHECK(first_view.ordinary_child_at(1U, 1U) == nil);
    CHECK(other_view != nil);
    CHECK(other_view != first);
    CHECK(first_view.tracked_identities() == 1U);
    CHECK(second_view.tracked_identities() == 1U);

    const auto ordinary_range = first_view.ordinary_children_range(1U, 0U, 8U);
    CHECK(ordinary_range.has_value());
    CHECK(ordinary_range->size() == 1U);
    CHECK((*ordinary_range)[0U] == first);
    CHECK(first_view.tracked_identities() == 1U);

    const auto empty_range = first_view.ordinary_children_range(1U, 1U, 8U);
    CHECK(empty_range.has_value());
    CHECK(empty_range->empty());
    CHECK(first_view.tracked_identities() == 1U);

    // Keep the old native proxy alive independently so its defunct behavior can
    // be checked after the cache drops its own strong reference.
    [first retain];

    CHECK(publication_state.publish(
        ordinary_snapshot(2U, false),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());
    CHECK(first_view.ordinary_child_count(1U) ==
          std::optional<std::size_t>{0U});
    CHECK(first_view.ordinary_child_at(1U, 0U) == nil);
    CHECK(first_view.ordinary(2U) == nil);
    CHECK(first_view.tracked_identities() == 0U);
    CHECK([first isAccessibilityElement] == NO);
    [first release];

    CHECK(second_view.prune_defunct() == 1U);
    CHECK(second_view.tracked_identities() == 0U);
}

void current_root_materialization_is_lazy_stable_and_fail_closed() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_root_757575757575_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};

    NSAccessibilityElement* root = cache.root();
    CHECK(root != nil);
    CHECK(cache.root() == root);
    CHECK(cache.tracked_identities() == 1U);

    auto* const root_state =
        ui::detail::macos_accessibility_proxy_stored_state(root);
    CHECK(root_state != nullptr);
    CHECK(root_state->node_id() == 1U);
    CHECK(!root_state->virtual_token().has_value());
    CHECK([[root accessibilityRole] isEqualToString:NSAccessibilityGroupRole]);

    [root retain];
    CHECK(publication_state.publish(
        ordinary_snapshot(
            2U,
            true,
            ui::SemanticRole::Button,
            ui::SemanticRole::None),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    CHECK(cache.root() == nil);
    CHECK([root isAccessibilityElement] == NO);
    CHECK(cache.prune_defunct() == 1U);
    CHECK(cache.tracked_identities() == 0U);
    [root release];

    CHECK(publication_state.publish(
        virtual_snapshot(3U, 100000U, 9000U),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    NSAccessibilityElement* list_root = cache.root();
    CHECK(list_root != nil);
    CHECK(cache.tracked_identities() == 1U);

    auto* const list_state =
        ui::detail::macos_accessibility_proxy_stored_state(list_root);
    CHECK(list_state != nullptr);
    CHECK(list_state->node_id() == 11U);
    CHECK(!list_state->virtual_token().has_value());
    CHECK([[list_root accessibilityRole] isEqualToString:NSAccessibilityListRole]);
    CHECK(cache.tracked_identities() == 1U);
}

void endpoint_lease_survives_facade_only_for_in_flight_work() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_endpoint_565656565656_PuglWrapperView");
    std::weak_ptr<ProxyCacheEndpoint> endpoint;
    std::shared_ptr<ProxyCacheEndpoint> in_flight;
    NSAccessibilityElement* child = nil;

    {
        ProxyCache cache{anchor, publication_state.reader_source()};
        endpoint = cache.endpoint();
        in_flight = endpoint.lock();
        CHECK(in_flight != nullptr);
        CHECK(in_flight->ordinary_child_count(1U) ==
              std::optional<std::size_t>{1U});
        child = in_flight->ordinary_child_at(1U, 0U);
        CHECK(child != nil);
        CHECK(cache.tracked_identities() == 1U);

        auto* const child_state =
            ui::detail::macos_accessibility_proxy_stored_state(child);
        CHECK(child_state != nullptr);
        CHECK(child_state->child_resolver_endpoint().lock() == in_flight);

        // Keep the proxy alive independently of the endpoint so we can prove its
        // resolver reference is weak rather than an ownership cycle.
        [child retain];
    }

    // One callback-local lease may finish after the owning facade retires. The
    // proxy and the facade both avoid owning the resolver; only this lease keeps
    // the endpoint alive until the in-flight callback completes.
    CHECK(!endpoint.expired());
    CHECK(in_flight->ordinary_child_at(1U, 0U) == child);

    auto* const retained_state =
        ui::detail::macos_accessibility_proxy_stored_state(child);
    CHECK(retained_state != nullptr);
    CHECK(retained_state->child_resolver_endpoint().lock() == in_flight);

    in_flight.reset();
    CHECK(endpoint.expired());
    CHECK(retained_state->child_resolver_endpoint().expired());
    CHECK([child isAccessibilityElement] == YES);
    [child release];
}


void action_endpoint_is_weak_and_scoped_per_cache() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto first_action_endpoint = inert_action_endpoint();
    auto second_action_endpoint = inert_action_endpoint();
    std::weak_ptr<const ui::detail::SemanticActionViewEndpoint> first_weak =
        first_action_endpoint;

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_actions_575757575757_PuglWrapperView");
    ProxyCache first_view{
        anchor, publication_state.reader_source(), first_action_endpoint};
    ProxyCache second_view{
        anchor, publication_state.reader_source(), second_action_endpoint};

    NSAccessibilityElement* first = first_view.ordinary(2U);
    NSAccessibilityElement* second = second_view.ordinary(2U);
    CHECK(first != nil);
    CHECK(second != nil);

    auto* const first_state =
        ui::detail::macos_accessibility_proxy_stored_state(first);
    auto* const second_state =
        ui::detail::macos_accessibility_proxy_stored_state(second);
    CHECK(first_state != nullptr);
    CHECK(second_state != nullptr);
    CHECK(first_state->action_endpoint().lock().get() ==
          first_action_endpoint.get());
    CHECK(second_state->action_endpoint().lock().get() ==
          second_action_endpoint.get());
    CHECK(first_state->action_endpoint().lock().get() !=
          second_state->action_endpoint().lock().get());

    // Cache/proxy state must not extend the action binding lifetime.
    first_action_endpoint.reset();
    CHECK(first_weak.expired());
    CHECK(first_state->action_endpoint().expired());
    CHECK(second_state->action_endpoint().lock().get() ==
          second_action_endpoint.get());

    constexpr ui::VirtualSemanticItemToken token = 9000U;
    CHECK(publication_state.publish(
        virtual_snapshot(2U, 1U, token),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    NSAccessibilityElement* row = second_view.virtual_item(11U, token);
    CHECK(row != nil);
    auto* const row_state =
        ui::detail::macos_accessibility_proxy_stored_state(row);
    CHECK(row_state != nullptr);
    CHECK(row_state->action_endpoint().lock().get() ==
          second_action_endpoint.get());
}

void exact_role_factory_does_not_reload_newer_generation() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true, ui::SemanticRole::Button),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto parent_state = ui::detail::MacOSAccessibilityProxyState::ordinary(
        publication_state.reader_source(), 1U);
    CHECK(parent_state.has_value());
    auto parent_read = parent_state->read();
    CHECK(parent_read.has_value());
    ui::detail::MacOSAccessibilityChildProjection projection{
        std::move(*parent_read)};
    const auto identity = projection.ordinary_child_identity_at(0U);
    const auto semantic_role = projection.ordinary_child_role_at(0U);
    CHECK(identity.has_value());
    CHECK(semantic_role ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::Button});
    const auto initial_mapping =
        ui::detail::macos_accessibility_role_mapping(*semantic_role);
    CHECK(initial_mapping.has_value());

    CHECK(publication_state.publish(
        ordinary_snapshot(2U, true, ui::SemanticRole::None),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    auto exact_state = ui::detail::MacOSAccessibilityProxyState::ordinary(
        publication_state.reader_source(), identity->node_id);
    CHECK(exact_state.has_value());
    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_exact_616161616161_PuglWrapperView");
    NSAccessibilityElement* exact =
        ui::detail::macos_accessibility_appkit_proxy_create(
            anchor, std::move(*exact_state), *initial_mapping);
    CHECK(exact != nil);
    CHECK([exact isAccessibilityElement] == NO);

    auto current_state = ui::detail::MacOSAccessibilityProxyState::ordinary(
        publication_state.reader_source(), identity->node_id);
    CHECK(current_state.has_value());
    CHECK(ui::detail::macos_accessibility_appkit_proxy_create(
        anchor, std::move(*current_state)) == nil);
}

void parent_materialization_uses_the_retained_child_generation() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(
            1U,
            true,
            ui::SemanticRole::Button,
            ui::SemanticRole::Group),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_parent_676767676767_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};
    NSAccessibilityElement* child = cache.ordinary(2U);
    CHECK(child != nil);
    CHECK(cache.tracked_identities() == 1U);

    auto* const child_state =
        ui::detail::macos_accessibility_proxy_stored_state(child);
    CHECK(child_state != nullptr);
    auto retained_child_read = child_state->read();
    CHECK(retained_child_read.has_value());
    ui::detail::MacOSAccessibilityChildProjection retained_child{
        std::move(*retained_child_read)};
    CHECK(retained_child.parent_role() ==
          std::optional<ui::SemanticRole>{ui::SemanticRole::Group});

    // Replace the current root role before the cache miss. The parent proxy must
    // still be constructible from the exact identity + role retained by the
    // child projection rather than reloading this newer generation.
    CHECK(publication_state.publish(
        ordinary_snapshot(
            2U,
            true,
            ui::SemanticRole::Button,
            ui::SemanticRole::None),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    const auto endpoint = cache.endpoint().lock();
    CHECK(endpoint != nullptr);
    NSAccessibilityElement* parent =
        endpoint->parent_from_projection(retained_child);
    CHECK(parent != nil);
    CHECK(cache.tracked_identities() == 2U);
    CHECK([parent isAccessibilityElement] == NO);

    constexpr ui::VirtualSemanticItemToken first_token = 7000U;
    CHECK(publication_state.publish(
        virtual_snapshot(3U, 3U, first_token),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    NSAccessibilityElement* row = cache.virtual_item(11U, first_token + 1U);
    CHECK(row != nil);
    auto* const row_state =
        ui::detail::macos_accessibility_proxy_stored_state(row);
    CHECK(row_state != nullptr);
    auto row_read = row_state->read();
    CHECK(row_read.has_value());
    ui::detail::MacOSAccessibilityChildProjection row_projection{
        std::move(*row_read)};
    NSAccessibilityElement* list_parent =
        endpoint->parent_from_projection(row_projection);
    CHECK(list_parent != nil);
    CHECK(list_parent == cache.ordinary(11U));

    auto* const list_state =
        ui::detail::macos_accessibility_proxy_stored_state(list_parent);
    CHECK(list_state != nullptr);
    auto list_read = list_state->read();
    CHECK(list_read.has_value());
    ui::detail::MacOSAccessibilityChildProjection list_projection{
        std::move(*list_read)};
    CHECK(endpoint->parent_from_projection(list_projection) == nil);
}

void virtual_range_lookup_materializes_only_requested_items() {
    NativePublicationState publication_state;
    constexpr std::size_t item_count = 100000U;
    constexpr ui::VirtualSemanticItemToken first_token = 1000U;
    constexpr std::size_t middle_index = 50000U;
    constexpr ui::VirtualSemanticItemToken middle_token =
        first_token + static_cast<ui::VirtualSemanticItemToken>(middle_index);
    constexpr ui::VirtualSemanticItemToken last_token =
        first_token + static_cast<ui::VirtualSemanticItemToken>(item_count - 1U);

    CHECK(publication_state.publish(
        virtual_snapshot(1U, item_count, first_token),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_virtual_626262626262_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};

    CHECK(cache.virtual_child_count(11U) ==
          std::optional<std::size_t>{item_count});
    CHECK(cache.ordinary_child_count(11U) ==
          std::optional<std::size_t>{0U});
    CHECK(cache.tracked_identities() == 0U);

    NSAccessibilityElement* last =
        cache.virtual_child_at(11U, item_count - 1U);
    CHECK(last != nil);
    CHECK(cache.tracked_identities() == 1U);
    CHECK(cache.virtual_item(11U, last_token) == last);
    CHECK(cache.virtual_child_at(11U, item_count) == nil);
    CHECK(cache.tracked_identities() == 1U);

    NSAccessibilityElement* middle = cache.virtual_child_at(11U, middle_index);
    CHECK(middle != nil);
    CHECK(middle != last);
    CHECK(cache.virtual_item(11U, middle_token) == middle);
    CHECK(cache.tracked_identities() == 2U);

    const auto range = cache.virtual_children_range(11U, middle_index, 3U);
    CHECK(range.has_value());
    CHECK(range->size() == 3U);
    CHECK((*range)[0U] == middle);
    CHECK((*range)[1U] != nil);
    CHECK((*range)[2U] != nil);
    CHECK((*range)[1U] != (*range)[2U]);
    CHECK(cache.tracked_identities() == 4U);

    const auto past_end = cache.virtual_children_range(11U, item_count, 3U);
    CHECK(past_end.has_value());
    CHECK(past_end->empty());
    const auto zero_count = cache.virtual_children_range(11U, 0U, 0U);
    CHECK(zero_count.has_value());
    CHECK(zero_count->empty());
    CHECK(cache.tracked_identities() == 4U);

    // Replacing the logical dataset does not eagerly visit or recreate all rows.
    // Only proxies that were explicitly requested are present to prune.
    CHECK(publication_state.publish(
        virtual_snapshot(2U, 0U, first_token),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());
    CHECK(cache.virtual_child_count(11U) ==
          std::optional<std::size_t>{0U});
    CHECK(cache.virtual_child_at(11U, 0U) == nil);
    CHECK(cache.prune_defunct() == 4U);
    CHECK(cache.tracked_identities() == 0U);
    CHECK(cache.virtual_item(11U, last_token) == nil);
}

void invalid_identity_and_retired_source_fail_closed() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(1U, true),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_proxy_cache_retired_737373737373_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};

    CHECK(cache.ordinary(ui::kInvalidSemanticId) == nil);
    CHECK(cache.virtual_item(11U, ui::kInvalidVirtualSemanticItemToken) == nil);
    CHECK(!cache.ordinary_child_count(ui::kInvalidSemanticId).has_value());
    CHECK(!cache.virtual_child_count(ui::kInvalidSemanticId).has_value());
    CHECK(cache.ordinary_child_at(ui::kInvalidSemanticId, 0U) == nil);
    CHECK(cache.virtual_child_at(ui::kInvalidSemanticId, 0U) == nil);
    CHECK(!cache.ordinary_children_range(ui::kInvalidSemanticId, 0U, 1U).has_value());
    CHECK(!cache.virtual_children_range(ui::kInvalidSemanticId, 0U, 1U).has_value());
    CHECK(cache.tracked_identities() == 0U);

    CHECK(cache.ordinary(2U) != nil);
    CHECK(cache.tracked_identities() == 1U);
    publication_state.shutdown();
    CHECK(!cache.ordinary_child_count(1U).has_value());
    CHECK(!cache.virtual_child_count(1U).has_value());
    CHECK(cache.ordinary_child_at(1U, 0U) == nil);
    CHECK(!cache.ordinary_children_range(1U, 0U, 1U).has_value());
    CHECK(cache.prune_defunct() == 1U);
    CHECK(cache.tracked_identities() == 0U);
    CHECK(cache.ordinary(2U) == nil);
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            ordinary_identity_is_stable_per_view_and_stale_entries_are_evicted();
            current_root_materialization_is_lazy_stable_and_fail_closed();
            endpoint_lease_survives_facade_only_for_in_flight_work();
            action_endpoint_is_weak_and_scoped_per_cache();
            exact_role_factory_does_not_reload_newer_generation();
            parent_materialization_uses_the_retained_child_generation();
            virtual_range_lookup_materializes_only_requested_items();
            invalid_identity_and_retired_source_fail_closed();
            std::cout << "PASS macOS accessibility proxy cache\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL macOS accessibility proxy cache: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}