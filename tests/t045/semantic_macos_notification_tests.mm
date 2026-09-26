#include "../../src/detail/native_accessibility_binding.hpp"
#include "../../src/detail/native_accessibility_bridge.h"

#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("line " + std::to_string(__LINE__) +       \
                                     ": check failed: " #condition);            \
        }                                                                       \
    } while (false)

using Batch = ui::detail::SemanticNativePublicationBatch;
using Change = ui::SemanticChange;
using NativePublicationState = ui::detail::SemanticNativePublicationState;

Class test_view_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

struct NotificationRecord final {
    std::string name;
    const void* element{};
};

struct NotificationLog {
    std::vector<NotificationRecord> records;

    [[nodiscard]] std::size_t count(const char* name) const {
        std::size_t result = 0U;
        for (const auto& record : records) {
            if (record.name == name) ++result;
        }
        return result;
    }

    [[nodiscard]] bool has(const char* name) const { return count(name) > 0U; }

    [[nodiscard]] bool has_appkit(NSAccessibilityNotificationName name) const {
        return has([name UTF8String]);
    }
};

void record_notification(void* user_data,
                         const char* notification,
                         const void* element) {
    auto* log = static_cast<NotificationLog*>(user_data);
    log->records.push_back(
        NotificationRecord{notification ? notification : "", element});
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    bool focused = false,
    bool selected = false,
    ui::SemanticRole root_role = ui::SemanticRole::Group) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = root_role;
    root.info.name = "root";
    root.children = {2U};
    snapshot->nodes.push_back(std::move(root));

    ui::SemanticNodeSnapshot child;
    child.id = 2U;
    child.parent = 1U;
    child.info.role = ui::SemanticRole::Button;
    child.info.name = "action";
    child.info.focused = focused;
    child.info.selected = selected;
    snapshot->nodes.push_back(std::move(child));
    return snapshot;
}

struct BridgeFixture final {
    NativePublicationState publication;
    Class view_class{test_view_class(
        "NUI_semantic_macos_notifications_747474747474_PuglWrapperView")};
    NSView* view{[[view_class alloc] init]};
    NativeUIAccessibilityBridge* bridge{nullptr};
    NotificationLog log;

    ~BridgeFixture() {
        if (bridge) nativeuiAccessibilityDestroy(bridge);
        [view release];
    }

    void attach() {
        const ui::detail::NativeAccessibilityAttachBinding binding{
            publication.reader_source(), {}};
        bridge = nativeuiAccessibilityCreate(view, &binding);
        CHECK(bridge != nullptr);
        nativeuiAccessibilitySetNotificationRecorderForTest(
            bridge, &log, &record_notification);
    }

    [[nodiscard]] bool deliver(const Batch& batch) {
        return nativeuiAccessibilityDeliver(bridge, &batch);
    }

    [[nodiscard]] std::optional<Batch> publish_structure(
        std::uint64_t generation,
        bool focused = false,
        bool selected = false) {
        return publication.publish(
            ordinary_snapshot(generation, focused, selected),
            {Change::StructureChanged},
            ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    }
};

NSArray* appkit_children(id element) {
    using Callback = NSArray* (*)(id, SEL, NSString*, NSUInteger, NSUInteger);
    return reinterpret_cast<Callback>(objc_msgSend)(
        element,
        sel_registerName("accessibilityArrayAttributeValues:index:maxCount:"),
        NSAccessibilityChildrenAttribute,
        0U,
        64U);
}

id view_root(NSView* view) {
    NSArray* const children = [view accessibilityChildren];
    if (!children || [children count] == 0U) return nil;
    return [children objectAtIndex:0U];
}

void structure_batch_announces_created_root_and_children_layout() {
    BridgeFixture fixture;
    fixture.attach();

    const auto batch = fixture.publish_structure(1U);
    CHECK(batch.has_value());
    CHECK(fixture.deliver(*batch));

    CHECK(fixture.log.records.size() == 2U);
    CHECK(fixture.log.has_appkit(NSAccessibilityCreatedNotification));
    CHECK(fixture.log.has_appkit(NSAccessibilityLayoutChangedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityUIElementDestroyedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityValueChangedNotification));

    id const root = view_root(fixture.view);
    CHECK(root != nil);
    CHECK(fixture.log.records[0U].element == (const void*)root);
    CHECK(fixture.log.records[1U].element == (const void*)root);
    CHECK([fixture.view isAccessibilityElement] == NO);
    CHECK([root isAccessibilityElement] == YES);
}

void value_only_batch_never_reposts_structure() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));

    id const root = view_root(fixture.view);
    CHECK(root != nil);
    fixture.log.records.clear();

    const auto value_only = fixture.publication.publish(
        ordinary_snapshot(2U),
        {Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(value_only.has_value());
    CHECK(fixture.deliver(*value_only));

    CHECK(fixture.log.records.size() == 1U);
    CHECK(fixture.log.has_appkit(NSAccessibilityValueChangedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityCreatedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityLayoutChangedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityUIElementDestroyedNotification));
    CHECK(fixture.log.records[0U].element == (const void*)root);
    CHECK(view_root(fixture.view) == root);
}

void bounds_only_batch_maps_to_layout_without_structure() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    fixture.log.records.clear();

    const auto bounds_only = fixture.publication.publish(
        ordinary_snapshot(2U),
        {},
        ui::detail::SemanticNativeGeometry{1.5f, {12.0f, -34.0f}});
    CHECK(bounds_only.has_value());
    CHECK(bounds_only->changes.size() == 1U);
    CHECK(bounds_only->changes.front() == Change::BoundsChanged);
    CHECK(fixture.deliver(*bounds_only));

    CHECK(fixture.log.records.size() == 1U);
    CHECK(fixture.log.has_appkit(NSAccessibilityLayoutChangedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityCreatedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityUIElementDestroyedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityValueChangedNotification));
}

void focus_and_selection_use_the_committed_categories_only() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    fixture.log.records.clear();

    const auto focus_only = fixture.publication.publish(
        ordinary_snapshot(2U, true, false),
        {Change::FocusChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(focus_only.has_value());
    CHECK(fixture.deliver(*focus_only));
    CHECK(fixture.log.records.size() == 1U);
    CHECK(fixture.log.has_appkit(
        NSAccessibilityFocusedUIElementChangedNotification));
    id const root = view_root(fixture.view);
    NSArray* const children = appkit_children(root);
    CHECK(children != nil && [children count] == 1U);
    CHECK(fixture.log.records[0U].element ==
          (const void*)[children objectAtIndex:0U]);
    fixture.log.records.clear();

    const auto selection_only = fixture.publication.publish(
        ordinary_snapshot(3U, false, true),
        {Change::SelectionChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(selection_only.has_value());
    CHECK(fixture.deliver(*selection_only));
    CHECK(fixture.log.records.size() == 1U);
    CHECK(fixture.log.has_appkit(
        NSAccessibilitySelectedChildrenChangedNotification));
    CHECK(fixture.log.records[0U].element == (const void*)root);
}

void combined_categories_emit_exactly_one_notification_per_category() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    fixture.log.records.clear();

    const auto combined = fixture.publication.publish(
        ordinary_snapshot(2U, true, true),
        {Change::StructureChanged, Change::FocusChanged, Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(combined.has_value());
    CHECK(fixture.deliver(*combined));

    CHECK(fixture.log.records.size() == 3U);
    CHECK(fixture.log.count([NSAccessibilityLayoutChangedNotification UTF8String]) == 1U);
    CHECK(fixture.log.count(
              [NSAccessibilityFocusedUIElementChangedNotification UTF8String]) == 1U);
    CHECK(fixture.log.count(
              [NSAccessibilityValueChangedNotification UTF8String]) == 1U);
    CHECK(!fixture.log.has_appkit(NSAccessibilityCreatedNotification));
    CHECK(!fixture.log.has_appkit(NSAccessibilityUIElementDestroyedNotification));
}

void root_replacement_posts_destroyed_then_created() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    id const first_root = view_root(fixture.view);
    CHECK(first_root != nil);
    [first_root retain];
    fixture.log.records.clear();

    auto replacement = std::make_shared<ui::SemanticTreeSnapshot>();
    replacement->generation = 2U;
    replacement->root = 7U;
    ui::SemanticNodeSnapshot root;
    root.id = 7U;
    root.info.role = ui::SemanticRole::Group;
    replacement->nodes.push_back(std::move(root));

    const auto changed = fixture.publication.publish(
        replacement,
        {Change::StructureChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(changed.has_value());
    CHECK(fixture.deliver(*changed));

    CHECK(fixture.log.records.size() == 3U);
    CHECK(fixture.log.records[0U].name ==
          [NSAccessibilityUIElementDestroyedNotification UTF8String]);
    CHECK(fixture.log.records[0U].element == (const void*)first_root);
    CHECK(fixture.log.records[1U].name ==
          [NSAccessibilityCreatedNotification UTF8String]);
    CHECK(fixture.log.records[2U].name ==
          [NSAccessibilityLayoutChangedNotification UTF8String]);
    id const second_root = view_root(fixture.view);
    CHECK(second_root != nil);
    CHECK(second_root != first_root);
    CHECK(fixture.log.records[1U].element == (const void*)second_root);
    CHECK(fixture.log.records[2U].element == (const void*)second_root);
    CHECK([first_root isAccessibilityElement] == NO);
    [first_root release];
}

void removed_root_posts_destroyed_and_empties_children() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    id const root = view_root(fixture.view);
    CHECK(root != nil);
    [root retain];
    fixture.log.records.clear();

    auto removed = std::make_shared<ui::SemanticTreeSnapshot>();
    removed->generation = 2U;
    removed->root = ui::kInvalidSemanticId;

    const auto changed = fixture.publication.publish(
        removed,
        {Change::StructureChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(changed.has_value());
    CHECK(fixture.deliver(*changed));

    CHECK(fixture.log.records.size() == 2U);
    CHECK(fixture.log.records[0U].name ==
          [NSAccessibilityUIElementDestroyedNotification UTF8String]);
    CHECK(fixture.log.records[0U].element == (const void*)root);
    CHECK(fixture.log.records[1U].name ==
          [NSAccessibilityLayoutChangedNotification UTF8String]);
    CHECK(fixture.log.records[1U].element == (const void*)fixture.view);
    CHECK([fixture.view accessibilityChildren] == nil ||
          [[fixture.view accessibilityChildren] count] == 0U);
    CHECK([root isAccessibilityElement] == NO);
    [root release];
}

void superseded_and_foreign_batches_are_ignored() {
    BridgeFixture first;
    BridgeFixture second;
    first.attach();
    second.attach();

    const auto first_initial = first.publish_structure(1U);
    CHECK(first_initial.has_value());
    CHECK(first.deliver(*first_initial));
    const auto second_initial = second.publish_structure(1U);
    CHECK(second_initial.has_value());
    CHECK(second.deliver(*second_initial));
    first.log.records.clear();
    second.log.records.clear();

    const auto superseded = first.publication.publish(
        ordinary_snapshot(2U, true, false),
        {Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(superseded.has_value());
    const auto current = first.publication.publish(
        ordinary_snapshot(3U, true, false),
        {Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(current.has_value());

    CHECK(!first.deliver(*superseded));
    CHECK(first.log.records.empty());
    CHECK(!first.deliver(*second_initial));
    CHECK(first.log.records.empty());
    CHECK(first.deliver(*current));
    CHECK(first.log.count([NSAccessibilityValueChangedNotification UTF8String]) == 1U);
    CHECK(second.log.records.empty());
}

void non_main_thread_and_nil_inputs_fail_closed() {
    BridgeFixture fixture;
    fixture.attach();
    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    fixture.log.records.clear();

    CHECK(!nativeuiAccessibilityDeliver(nullptr, &*initial));
    CHECK(!nativeuiAccessibilityDeliver(fixture.bridge, nullptr));

    std::atomic<bool> delivered{true};
    std::thread worker([&] {
        delivered = nativeuiAccessibilityDeliver(fixture.bridge, &*initial);
    });
    worker.join();
    CHECK(!delivered.load());
    CHECK(fixture.log.records.empty());
}

void throwing_recorder_is_contained_and_later_delivery_recovers() {
    struct ThrowingLog final : NotificationLog {
        int calls{};
        int throw_on_call{1};
        bool throw_objective_c{};
    } log;

    BridgeFixture fixture;
    fixture.attach();
    nativeuiAccessibilitySetNotificationRecorderForTest(
        fixture.bridge, &log, [](void* user_data, const char* name, const void* element) {
            auto* target = static_cast<ThrowingLog*>(user_data);
            ++target->calls;
            target->records.push_back(NotificationRecord{name ? name : "", element});
            if (target->calls == target->throw_on_call) {
                if (target->throw_objective_c) {
                    @throw [NSException exceptionWithName:@"NativeUITestRecorder"
                                                   reason:@"injected"
                                                 userInfo:nil];
                }
                throw std::runtime_error("injected notification recorder failure");
            }
        });

    const auto initial = fixture.publish_structure(1U);
    CHECK(initial.has_value());
    CHECK(fixture.deliver(*initial));
    CHECK(log.calls == 2);
    CHECK(log.records.size() == 2U);

    log.records.clear();
    log.calls = 0;
    log.throw_on_call = -1;
    const auto value_only = fixture.publication.publish(
        ordinary_snapshot(2U),
        {Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(value_only.has_value());
    CHECK(fixture.deliver(*value_only));
    CHECK(log.calls == 1);
    CHECK(log.records.size() == 1U);
    CHECK(log.records[0U].name ==
          [NSAccessibilityValueChangedNotification UTF8String]);

    log.records.clear();
    log.calls = 0;
    log.throw_on_call = 1;
    log.throw_objective_c = true;
    const auto after = fixture.publication.publish(
        ordinary_snapshot(3U),
        {Change::ValueChanged},
        ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}});
    CHECK(after.has_value());
    CHECK(fixture.deliver(*after));
    CHECK(log.records.size() == 1U);
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            structure_batch_announces_created_root_and_children_layout();
            value_only_batch_never_reposts_structure();
            bounds_only_batch_maps_to_layout_without_structure();
            focus_and_selection_use_the_committed_categories_only();
            combined_categories_emit_exactly_one_notification_per_category();
            root_replacement_posts_destroyed_then_created();
            removed_root_posts_destroyed_and_empties_children();
            superseded_and_foreign_batches_are_ignored();
            non_main_thread_and_nil_inputs_fail_closed();
            throwing_recorder_is_contained_and_later_delivery_recovers();
            std::cout << "PASS macOS accessibility notifications\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL macOS accessibility notifications: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
