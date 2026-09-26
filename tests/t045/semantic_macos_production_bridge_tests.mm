#include "../../src/detail/native_accessibility_binding.hpp"
#include "../../src/detail/native_accessibility_bridge.h"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>

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
using Geometry = ui::detail::SemanticNativeGeometry;

Class test_view_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

NSUInteger appkit_child_count(id element) {
    using Callback = NSUInteger (*)(id, SEL, NSString*);
    return reinterpret_cast<Callback>(objc_msgSend)(
        element,
        sel_registerName("accessibilityArrayAttributeCount:"),
        NSAccessibilityChildrenAttribute);
}

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

bool selector_allowed(id element, SEL selector) {
    using Callback = BOOL (*)(id, SEL, SEL);
    return reinterpret_cast<Callback>(objc_msgSend)(
               element, sel_registerName("isAccessibilitySelectorAllowed:"), selector)
        ? true
        : false;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> button_snapshot(
    bool with_child = true,
    bool focused_child = false,
    bool selected_child = false,
    ui::SemanticRole root_role = ui::SemanticRole::Group,
    ui::SemanticRole child_role = ui::SemanticRole::Button) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = 1U;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = root_role;
    root.info.name = "root";
    if (with_child) root.children = {2U};
    snapshot->nodes.push_back(std::move(root));

    if (with_child) {
        ui::SemanticNodeSnapshot child;
        child.id = 2U;
        child.parent = 1U;
        child.info.role = child_role;
        child.info.name = "action";
        child.info.focused = focused_child;
        child.info.selected = selected_child;
        child.info.enabled = true;
        child.info.actions = {ui::SemanticAction::Activate};
        snapshot->nodes.push_back(std::move(child));
    }
    return snapshot;
}

class RecordingTarget final : public ui::detail::SemanticActionTarget {
public:
    RecordingTarget(std::shared_ptr<int> activations, bool dispatch_throws = false)
        : activations_(std::move(activations)),
          dispatch_throws_(dispatch_throws) {}

    [[nodiscard]] std::optional<ui::SemanticInfo> current_semantics(
        const ui::detail::SemanticIdentity& identity) const override {
        if (identity.virtual_token) return std::nullopt;
        if (identity.node_id == 1U) {
            ui::SemanticInfo info;
            info.role = ui::SemanticRole::Group;
            info.name = "root";
            return info;
        }
        if (identity.node_id == 2U) {
            ui::SemanticInfo info;
            info.role = ui::SemanticRole::Button;
            info.name = "action";
            info.enabled = true;
            info.actions = {ui::SemanticAction::Activate};
            return info;
        }
        return std::nullopt;
    }

    bool dispatch_semantic_action(
        const ui::detail::SemanticIdentity& identity,
        const ui::detail::SemanticActionRequest& request) override {
        if (identity.node_id != 2U || identity.virtual_token ||
            request.action != ui::SemanticAction::Activate) {
            return false;
        }
        if (dispatch_throws_) {
            throw std::runtime_error("injected semantic action failure");
        }
        ++*activations_;
        return true;
    }

private:
    std::shared_ptr<int> activations_;
    bool dispatch_throws_{};
};

struct BridgeFixture final {
    ui::detail::DispatcherOwner owner;
    ui::detail::SemanticNativeViewBridge view_bridge;
    Class view_class;
    NSView* view;
    NativeUIAccessibilityBridge* bridge{nullptr};
    std::shared_ptr<int> activations{std::make_shared<int>(0)};
    bool dispatch_throws{};

    explicit BridgeFixture(const char* runtime_name, bool throwing = false)
        : view_class(test_view_class(runtime_name)),
          view([[view_class alloc] init]),
          dispatch_throws(throwing) {}

    ~BridgeFixture() {
        if (bridge) nativeuiAccessibilityDestroy(bridge);
        [view release];
    }

    void bind_actions() {
        auto target = std::make_shared<RecordingTarget>(activations, dispatch_throws);
        view_bridge.bind_actions(owner.dispatcher(), std::move(target));
    }

    [[nodiscard]] std::optional<Batch> publish(
        const std::shared_ptr<const ui::SemanticTreeSnapshot>& snapshot,
        Geometry geometry = Geometry{1.0f, {0.0f, 0.0f}}) {
        view_bridge.stage(*snapshot);
        return view_bridge.checkpoint_native_publication(geometry);
    }

    void attach() {
        const ui::detail::NativeAccessibilityAttachBinding binding{
            view_bridge.native_reader_source(),
            view_bridge.native_action_endpoint()};
        bridge = nativeuiAccessibilityCreate(view, &binding);
        CHECK(bridge != nullptr);
    }
};

void view_exposes_the_semantic_root_and_its_children() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_root_848484848484_PuglWrapperView");
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot());
    CHECK(batch.has_value());
    CHECK(batch->publication != nullptr);
    fixture.attach();

    CHECK([fixture.view isAccessibilityElement] == NO);
    CHECK([fixture.view accessibilityFocusedUIElement] == nil);

    id const root = view_root(fixture.view);
    CHECK(root != nil);
    CHECK([root isAccessibilityElement] == YES);
    CHECK([[root accessibilityRole] isEqualToString:NSAccessibilityGroupRole]);
    CHECK([[root accessibilityLabel] isEqualToString:@"root"]);
    CHECK(appkit_child_count(root) == 1U);

    NSArray* const children = appkit_children(root);
    CHECK(children != nil);
    CHECK([children count] == 1U);
    id const child = [children objectAtIndex:0U];
    CHECK([[child accessibilityRole] isEqualToString:NSAccessibilityButtonRole]);
    CHECK([[child accessibilityLabel] isEqualToString:@"action"]);
    CHECK(selector_allowed(child, @selector(accessibilityPerformPress)));
    CHECK(!selector_allowed(child, @selector(accessibilityPerformIncrement)));
    CHECK([child accessibilityValue] == nil);
}

void focused_element_is_exposed_through_the_view() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_focus_858585858585_PuglWrapperView");
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot(true, true, false));
    CHECK(batch.has_value());
    fixture.attach();

    id const root = view_root(fixture.view);
    CHECK(root != nil);
    NSArray* const children = appkit_children(root);
    CHECK(children != nil && [children count] == 1U);
    id const child = [children objectAtIndex:0U];

    CHECK([fixture.view accessibilityFocusedUIElement] == child);
    CHECK([child isAccessibilityFocused] == YES);

    const auto second = fixture.publish(button_snapshot(true, false, false));
    CHECK(second.has_value());
    CHECK([fixture.view accessibilityFocusedUIElement] == nil);
}

void attribute_reads_and_actions_use_the_proxy_cache() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_actions_868686868686_PuglWrapperView");
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot());
    CHECK(batch.has_value());
    fixture.attach();

    id const root = view_root(fixture.view);
    NSArray* const children = appkit_children(root);
    CHECK(children != nil && [children count] == 1U);
    id const child = [children objectAtIndex:0U];

    CHECK([child accessibilityPerformPress] == YES);
    CHECK(*fixture.activations == 0);
    CHECK(fixture.owner.checkpoint() == 1U);
    CHECK(*fixture.activations == 1);

    CHECK([child accessibilityPerformPress] == YES);
    CHECK(fixture.owner.checkpoint() == 1U);
    CHECK(*fixture.activations == 2);

    // Attribute reads never reload a newer generation and stay stable for the
    // same semantic identity.
    CHECK([[root accessibilityRole] isEqualToString:NSAccessibilityGroupRole]);
    CHECK([[child accessibilityRole] isEqualToString:NSAccessibilityButtonRole]);
    CHECK([child accessibilityHelp] == nil);
    CHECK([child accessibilityMinValue] == nil);
    CHECK([child accessibilityMaxValue] == nil);
}

void defunct_after_root_removal() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_defunct_878787878787_PuglWrapperView");
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot());
    CHECK(batch.has_value());
    fixture.attach();

    id const root = view_root(fixture.view);
    CHECK(root != nil);
    [root retain];

    auto removed = std::make_shared<ui::SemanticTreeSnapshot>();
    removed->generation = 2U;
    removed->root = ui::kInvalidSemanticId;
    const auto changed = fixture.publish(removed);
    CHECK(changed.has_value());
    CHECK(changed->publication != nullptr);

    CHECK([fixture.view accessibilityChildren] != nil);
    CHECK([[fixture.view accessibilityChildren] count] == 0U);
    CHECK(view_root(fixture.view) == nil);
    CHECK([root isAccessibilityElement] == NO);
    CHECK(selector_allowed(root, @selector(accessibilityPerformPress)) == false);
    CHECK([fixture.view accessibilityFocusedUIElement] == nil);
    [root release];
}

void nil_and_expired_endpoints_fail_closed() {
    Class const view_class = test_view_class(
        "NUI_semantic_macos_bridge_nil_888888888888_PuglWrapperView");
    NSView* const view = [[view_class alloc] init];

    const ui::detail::NativeAccessibilityAttachBinding empty_binding{};
    CHECK(nativeuiAccessibilityCreate(view, &empty_binding) == nullptr);
    CHECK(nativeuiAccessibilityCreate(nullptr, &empty_binding) == nullptr);
    CHECK(nativeuiAccessibilityCreate(view, nullptr) == nullptr);
    CHECK(nativeuiAccessibilityTargetClass(nullptr) == nullptr);
    CHECK(nativeuiAccessibilityDeliver(nullptr, nullptr) == false);
    CHECK(nativeuiAccessibilityDeliver(nullptr, &empty_binding) == false);

    // A live publication source with no action endpoint is a valid read-only
    // view: queries work, every action fails closed.
    auto publication_owner =
        std::make_unique<ui::detail::SemanticNativeViewBridge>();
    publication_owner->stage(*button_snapshot());
    const auto batch = publication_owner->checkpoint_native_publication(
        Geometry{1.0f, {0.0f, 0.0f}});
    CHECK(batch.has_value());

    const ui::detail::NativeAccessibilityAttachBinding read_only{
        publication_owner->native_reader_source(), {}};
    NativeUIAccessibilityBridge* bridge =
        nativeuiAccessibilityCreate(view, &read_only);
    CHECK(bridge != nullptr);

    id const root = view_root(view);
    CHECK(root != nil);
    NSArray* const children = appkit_children(root);
    CHECK(children != nil && [children count] == 1U);
    id const child = [children objectAtIndex:0U];
    CHECK([child accessibilityPerformPress] == NO);
    CHECK(!selector_allowed(child, @selector(accessibilityPerformPress)));

    // The action request is contained by the proxy boundary; setters must not
    // throw even when no action endpoint exists.
    [child setAccessibilityValue:@1.0];
    [child setAccessibilityFocused:YES];
    [child setAccessibilitySelected:YES];
    [child setAccessibilityExpanded:YES];

    nativeuiAccessibilityDestroy(bridge);

    // An expired publication source rejects the attach entirely.
    publication_owner.reset();
    Class const expired_class = test_view_class(
        "NUI_semantic_macos_bridge_expired_898989898989_PuglWrapperView");
    NSView* const expired_view = [[expired_class alloc] init];
    CHECK(nativeuiAccessibilityCreate(expired_view, &read_only) == nullptr);
    [expired_view release];
    [view release];
}

void non_main_thread_attach_is_rejected() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_thread_909090909090_PuglWrapperView");
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot());
    CHECK(batch.has_value());

    std::atomic<void*> created{nullptr};
    std::thread worker([&] {
        const ui::detail::NativeAccessibilityAttachBinding binding{
            fixture.view_bridge.native_reader_source(),
            fixture.view_bridge.native_action_endpoint()};
        created.store(nativeuiAccessibilityCreate(fixture.view, &binding));
    });
    worker.join();
    CHECK(created.load() == nullptr);
    CHECK(object_getClass(fixture.view) == fixture.view_class);

    fixture.attach();
    CHECK(view_root(fixture.view) != nil);
}

void conflicting_pre_existing_runtime_class_fails_closed() {
    Class const view_class = test_view_class(
        "NUI_semantic_macos_bridge_conflict_919191919191_PuglWrapperView");
    NSView* const view = [[view_class alloc] init];

    const char* const view_name = class_getName(view_class);
    const std::string target_name =
        std::string{view_name} + "_NativeUIAccessibilityView";
    Class conflicting = objc_allocateClassPair([NSObject class], target_name.c_str(), 0U);
    CHECK(conflicting != Nil);
    objc_registerClassPair(conflicting);
    CHECK(objc_lookUpClass(target_name.c_str()) == conflicting);

    auto publication_owner =
        std::make_unique<ui::detail::SemanticNativeViewBridge>();
    publication_owner->stage(*button_snapshot());
    const auto batch = publication_owner->checkpoint_native_publication(
        Geometry{1.0f, {0.0f, 0.0f}});
    CHECK(batch.has_value());
    const ui::detail::NativeAccessibilityAttachBinding binding{
        publication_owner->native_reader_source(), {}};

    CHECK(nativeuiAccessibilityCreate(view, &binding) == nullptr);
    CHECK(object_getClass(view) == view_class);
    [view release];
}

void root_materialization_failure_recovers_on_next_query() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_retry_929292929292_PuglWrapperView");
    fixture.bind_actions();
    const auto flattened = fixture.publish(
        button_snapshot(false, false, false, ui::SemanticRole::None));
    CHECK(flattened.has_value());
    fixture.attach();

    CHECK([fixture.view accessibilityChildren] != nil);
    CHECK([[fixture.view accessibilityChildren] count] == 0U);
    CHECK(view_root(fixture.view) == nil);

    const auto recovered = fixture.publish(button_snapshot());
    CHECK(recovered.has_value());
    id const root = view_root(fixture.view);
    CHECK(root != nil);
    CHECK(appkit_child_count(root) == 1U);
}

void multi_view_isolation_and_foreign_batch_rejection() {
    BridgeFixture first(
        "NUI_semantic_macos_bridge_isolate_a_a1a1a1a1a1a1_PuglWrapperView");
    BridgeFixture second(
        "NUI_semantic_macos_bridge_isolate_b_b2b2b2b2b2b2_PuglOpenGLView");
    first.bind_actions();
    second.bind_actions();

    const auto first_batch = first.publish(button_snapshot());
    CHECK(first_batch.has_value());
    first.attach();
    const auto second_batch = second.publish(button_snapshot());
    CHECK(second_batch.has_value());
    second.attach();

    id const first_root = view_root(first.view);
    id const second_root = view_root(second.view);
    CHECK(first_root != nil);
    CHECK(second_root != nil);
    CHECK(first_root != second_root);
    CHECK(view_root(first.view) == first_root);

    NSArray* const first_children = appkit_children(first_root);
    NSArray* const second_children = appkit_children(second_root);
    CHECK([first_children objectAtIndex:0U] != [second_children objectAtIndex:0U]);

    id const first_child = [first_children objectAtIndex:0U];
    CHECK([first_child accessibilityPerformPress] == YES);
    CHECK(first.owner.checkpoint() == 1U);
    CHECK(*first.activations == 1);
    CHECK(*second.activations == 0);

    // A foreign batch is ignored by the exact-current publication check.
    CHECK(nativeuiAccessibilityDeliver(first.bridge, &*second_batch) == false);

    // Actions stay on their own view after the sibling is retired.
    nativeuiAccessibilityDestroy(second.bridge);
    second.bridge = nullptr;
    CHECK([first_child accessibilityPerformPress] == YES);
    CHECK(first.owner.checkpoint() == 1U);
    CHECK(*first.activations == 2);
}

void setter_actions_contain_failures_on_live_and_defunct_proxies() {
    BridgeFixture fixture(
        "NUI_semantic_macos_bridge_setters_c3c3c3c3c3c3_PuglWrapperView",
        /*throwing=*/true);
    fixture.bind_actions();
    const auto batch = fixture.publish(button_snapshot());
    CHECK(batch.has_value());
    fixture.attach();

    id const root = view_root(fixture.view);
    NSArray* const children = appkit_children(root);
    CHECK(children != nil && [children count] == 1U);
    id const child = [children objectAtIndex:0U];

    // Posting succeeds, but the throwing live handler is revalidated on the UI
    // thread. The proxy boundary never observes the handler failure; the T065
    // pump propagates it like any other component fault and no action is
    // applied. The next accepted press still posts without guard poisoning.
    CHECK([child accessibilityPerformPress] == YES);
    CHECK(*fixture.activations == 0);
    bool drain_threw = false;
    try {
        (void)fixture.owner.checkpoint();
    } catch (const std::runtime_error&) {
        drain_threw = true;
    }
    CHECK(drain_threw);
    CHECK(*fixture.activations == 0);

    auto removed = std::make_shared<ui::SemanticTreeSnapshot>();
    removed->generation = 2U;
    removed->root = ui::kInvalidSemanticId;
    const auto changed = fixture.publish(removed);
    CHECK(changed.has_value());

    [child setAccessibilityValue:@2.0];
    [child setAccessibilityFocused:YES];
    [child setAccessibilitySelected:YES];
    [child setAccessibilityExpanded:YES];
    CHECK([child accessibilityPerformPress] == NO);
    CHECK([child accessibilityPerformIncrement] == NO);
    CHECK(!selector_allowed(child, @selector(accessibilityPerformPress)));
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            view_exposes_the_semantic_root_and_its_children();
            focused_element_is_exposed_through_the_view();
            attribute_reads_and_actions_use_the_proxy_cache();
            defunct_after_root_removal();
            nil_and_expired_endpoints_fail_closed();
            non_main_thread_attach_is_rejected();
            conflicting_pre_existing_runtime_class_fails_closed();
            root_materialization_failure_recovers_on_next_query();
            multi_view_isolation_and_foreign_batch_rejection();
            setter_actions_contain_failures_on_live_and_defunct_proxies();
            std::cout << "PASS macOS accessibility production bridge\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL macOS accessibility production bridge: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
