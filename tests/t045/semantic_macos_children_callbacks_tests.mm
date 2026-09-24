#include "../../src/detail/semantic_macos_proxy_cache.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
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

Class test_anchor_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

NSUInteger appkit_child_count(NSAccessibilityElement* element) {
    using Callback = NSUInteger (*)(id, SEL, NSString*);
    const SEL selector = sel_registerName("accessibilityArrayAttributeCount:");
    return reinterpret_cast<Callback>(objc_msgSend)(
        element, selector, NSAccessibilityChildrenAttribute);
}

NSArray* appkit_child_range(
    NSAccessibilityElement* element,
    NSUInteger index,
    NSUInteger max_count) {
    using Callback = NSArray* (*)(id, SEL, NSString*, NSUInteger, NSUInteger);
    const SEL selector =
        sel_registerName("accessibilityArrayAttributeValues:index:maxCount:");
    return reinterpret_cast<Callback>(objc_msgSend)(
        element,
        selector,
        NSAccessibilityChildrenAttribute,
        index,
        max_count);
}

NSUInteger appkit_child_index(NSAccessibilityElement* parent, id child) {
    using Callback = NSUInteger (*)(id, SEL, id);
    const SEL selector = sel_registerName("accessibilityIndexOfChild:");
    return reinterpret_cast<Callback>(objc_msgSend)(parent, selector, child);
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot() {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = 1U;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = ui::SemanticRole::Group;
    root.children = {2U, 3U};
    snapshot->nodes.push_back(std::move(root));

    ui::SemanticNodeSnapshot first;
    first.id = 2U;
    first.parent = 1U;
    first.info.role = ui::SemanticRole::Button;
    first.info.name = "first";
    snapshot->nodes.push_back(std::move(first));

    ui::SemanticNodeSnapshot second;
    second.id = 3U;
    second.parent = 1U;
    second.info.role = ui::SemanticRole::Button;
    second.info.name = "second";
    snapshot->nodes.push_back(std::move(second));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_snapshot(
    std::size_t count,
    ui::VirtualSemanticItemToken first_token) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = 1U;
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
        1U,
        std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
        std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
        std::nullopt,
        list.bounds,
        20.0f,
        0.0f);
    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

void ordinary_children_callbacks_use_bounded_stable_proxies() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_children_callbacks_ordinary_818181818181_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};
    NSAccessibilityElement* root = cache.ordinary(1U);
    CHECK(root != nil);
    CHECK(cache.tracked_identities() == 1U);

    CHECK(appkit_child_count(root) == 2U);
    CHECK(cache.tracked_identities() == 1U);

    NSArray* second_only = appkit_child_range(root, 1U, 1U);
    CHECK(second_only != nil);
    CHECK([second_only count] == 1U);
    CHECK(cache.tracked_identities() == 2U);
    CHECK([second_only objectAtIndex:0U] == cache.ordinary_child_at(1U, 1U));

    NSArray* both = appkit_child_range(root, 0U, 8U);
    CHECK(both != nil);
    CHECK([both count] == 2U);
    CHECK(cache.tracked_identities() == 3U);
    CHECK([both objectAtIndex:1U] == [second_only objectAtIndex:0U]);

    NSAccessibilityElement* first = [both objectAtIndex:0U];
    NSAccessibilityElement* second = [both objectAtIndex:1U];
    CHECK(appkit_child_index(root, first) == 0U);
    CHECK(appkit_child_index(root, second) == 1U);
    CHECK(appkit_child_index(root, root) == NSNotFound);

    ProxyCache other_cache{anchor, publication_state.reader_source()};
    NSAccessibilityElement* other_first = other_cache.ordinary_child_at(1U, 0U);
    CHECK(other_first != nil);
    CHECK(appkit_child_index(root, other_first) == NSNotFound);

    NSArray* past_end = appkit_child_range(root, 2U, 4U);
    CHECK(past_end != nil);
    CHECK([past_end count] == 0U);
    CHECK(cache.tracked_identities() == 3U);
}

void virtual_children_callbacks_do_not_materialize_the_collection() {
    NativePublicationState publication_state;
    constexpr std::size_t item_count = 100000U;
    constexpr ui::VirtualSemanticItemToken first_token = 3000U;
    constexpr NSUInteger middle_index = 50000U;

    CHECK(publication_state.publish(
        virtual_snapshot(item_count, first_token),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_children_callbacks_virtual_828282828282_PuglWrapperView");
    ProxyCache cache{anchor, publication_state.reader_source()};
    NSAccessibilityElement* list = cache.ordinary(11U);
    CHECK(list != nil);
    CHECK(cache.tracked_identities() == 1U);

    CHECK(appkit_child_count(list) == item_count);
    CHECK(cache.tracked_identities() == 1U);

    NSArray* window = appkit_child_range(list, middle_index, 2U);
    CHECK(window != nil);
    CHECK([window count] == 2U);
    CHECK(cache.tracked_identities() == 3U);

    NSAccessibilityElement* first = [window objectAtIndex:0U];
    NSAccessibilityElement* second = [window objectAtIndex:1U];
    CHECK(first != nil);
    CHECK(second != nil);
    CHECK(first != second);
    CHECK(appkit_child_index(list, first) == middle_index);
    CHECK(appkit_child_index(list, second) == middle_index + 1U);

    NSArray* same_window = appkit_child_range(list, middle_index, 2U);
    CHECK(same_window != nil);
    CHECK([same_window count] == 2U);
    CHECK([same_window objectAtIndex:0U] == first);
    CHECK([same_window objectAtIndex:1U] == second);
    CHECK(cache.tracked_identities() == 3U);

    NSAccessibilityElement* last = cache.virtual_child_at(11U, item_count - 1U);
    CHECK(last != nil);
    CHECK(appkit_child_index(list, last) ==
          static_cast<NSUInteger>(item_count - 1U));
    CHECK(cache.tracked_identities() == 4U);

    CHECK(appkit_child_count(first) == 0U);
    CHECK(appkit_child_index(first, second) == NSNotFound);
    NSArray* row_children = appkit_child_range(first, 0U, 1U);
    CHECK(row_children != nil);
    CHECK([row_children count] == 0U);
    CHECK(cache.tracked_identities() == 4U);
}

void retired_resolver_fails_closed_without_retaining_the_cache() {
    NativePublicationState publication_state;
    CHECK(publication_state.publish(
        ordinary_snapshot(),
        {ui::SemanticChange::StructureChanged},
        {}).has_value());

    Class anchor = test_anchor_class(
        "NUI_semantic_children_callbacks_retired_838383838383_PuglWrapperView");
    NSAccessibilityElement* root = nil;
    NSAccessibilityElement* child = nil;
    {
        ProxyCache cache{anchor, publication_state.reader_source()};
        root = cache.ordinary(1U);
        child = cache.ordinary_child_at(1U, 0U);
        CHECK(root != nil);
        CHECK(child != nil);
        [root retain];
        [child retain];
        CHECK(appkit_child_count(root) == 2U);
        CHECK(appkit_child_index(root, child) == 0U);
    }

    CHECK(appkit_child_count(root) == 0U);
    CHECK(appkit_child_range(root, 0U, 1U) == nil);
    CHECK(appkit_child_index(root, child) == NSNotFound);
    CHECK([root isAccessibilityElement] == YES);
    [child release];
    [root release];
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            ordinary_children_callbacks_use_bounded_stable_proxies();
            virtual_children_callbacks_do_not_materialize_the_collection();
            retired_resolver_fails_closed_without_retaining_the_cache();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
