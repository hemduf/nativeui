#include "native_accessibility_bridge.h"

#if !defined(__OBJC__)
#error "native_accessibility_macos.mm requires Objective-C++"
#endif

#include "native_accessibility_binding.hpp"

#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include "semantic_macos_proxy_cache.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

namespace {

using ui::detail::MacOSAccessibilityProxyCache;
using ui::detail::NativeAccessibilityAttachBinding;
using ui::detail::SemanticNativePublicationBatch;
using ui::detail::SemanticNativePublicationSource;

/// Per-object association key. The key is immutable and carries no
/// instance-dependent state; each native view owns its own retained NSValue box
/// and therefore its own bridge pointer. No process-global bridge registry exists.
const char kAccessibilityBridgeAssociationKey = 0;

constexpr char kConsumerRuntimePrefix[] = "NUI_";
constexpr char kAccessibilityViewSuffix[] = "_NativeUIAccessibilityView";

} // namespace

/// One per-native-view macOS accessibility bridge.
///
/// The struct owns only the lazy proxy cache (whose elements weakly observe the
/// per-view publication source/action endpoint), the last announced root element
/// retained for created/destroyed notification targeting, and the optional test
/// recorder. It never stores a retained Tree/Node/Component, and the two
/// semantic endpoints are weak copies from the attach binding.
struct NativeUIAccessibilityBridge final {
    NSView* view{};
    Class original_view_class{Nil};
    Class accessibility_view_class{Nil};
    std::weak_ptr<const ui::detail::SemanticNativePublicationSource> publication_source;
    std::weak_ptr<const ui::detail::SemanticActionViewEndpoint> action_endpoint;
    std::unique_ptr<MacOSAccessibilityProxyCache> cache;
    NSAccessibilityElement* last_root{};
    void* recorder_user_data{};
    NativeUIAccessibilityNotificationRecorder recorder{};
};

namespace {

[[nodiscard]] NativeUIAccessibilityBridge* bridge_for_view(id view) noexcept {
    if (!view) return nullptr;
    NSValue* const value =
        objc_getAssociatedObject(view, &kAccessibilityBridgeAssociationKey);
    return value ? static_cast<NativeUIAccessibilityBridge*>([value pointerValue])
                 : nullptr;
}

[[nodiscard]] bool has_change(const std::vector<ui::SemanticChange>& changes,
                              ui::SemanticChange expected) noexcept {
    return std::find(changes.begin(), changes.end(), expected) != changes.end();
}

void emit_notification(NativeUIAccessibilityBridge* bridge,
                       NSAccessibilityNotificationName name,
                       id element) noexcept {
    if (!bridge || !name) return;
    if (!element) element = bridge->view;
    if (!element) return;

    if (bridge->recorder) {
        const char* utf8 = nullptr;
        @try {
            utf8 = [name UTF8String];
        } @catch (...) {
            utf8 = nullptr;
        }
        try {
            bridge->recorder(bridge->recorder_user_data, utf8 ? utf8 : "",
                             reinterpret_cast<const void*>(element));
        } catch (...) {
            // A test recorder must never unwind into the native pump.
        }
        return;
    }

    @try {
        NSAccessibilityPostNotification(element, name);
    } @catch (...) {
        // Accessibility notification posting is a foreign ABI boundary.
    }
}

/// Resolve the currently focused ordinary semantic node through the lazy cache.
/// Virtual focus continues to live on the owning list until the virtual focus
/// contract is exposed; ordinary focus is the bounded frozen v1 path.
[[nodiscard]] NSAccessibilityElement* focused_element(
    NativeUIAccessibilityBridge* bridge) noexcept {
    if (!bridge || !bridge->cache || ![NSThread isMainThread]) return nil;

    try {
        const auto source = bridge->publication_source.lock();
        if (!source) return nil;
        const auto publication = source->current();
        if (!publication || !publication->semantic_snapshot) return nil;

        for (const auto& node : publication->semantic_snapshot->nodes) {
            if (node.info.focused) {
                return bridge->cache->ordinary(node.id);
            }
        }
    } catch (...) {
        return nil;
    }
    return nil;
}

/// Map one committed batch onto the closed macOS notification set. Structure
/// posts created/destroyed notifications for an announced root transition plus
/// the AppKit layout notification that replaced the removed historical
/// children-changed constant; value-only and bounds-only batches never announce
/// a root recreation.
void emit_committed_batch(NativeUIAccessibilityBridge* bridge,
                          const SemanticNativePublicationBatch& batch) noexcept {
    if (!bridge) return;

    const bool structure =
        has_change(batch.changes, ui::SemanticChange::StructureChanged);
    const bool focus = has_change(batch.changes, ui::SemanticChange::FocusChanged);
    const bool selection =
        has_change(batch.changes, ui::SemanticChange::SelectionChanged);
    const bool value = has_change(batch.changes, ui::SemanticChange::ValueChanged);
    const bool bounds = has_change(batch.changes, ui::SemanticChange::BoundsChanged);

    if (structure) {
        if (bridge->cache) {
            (void)bridge->cache->apply_publication_batch(batch);
        }
        NSAccessibilityElement* const current =
            bridge->cache ? bridge->cache->root() : nil;
        if (current != bridge->last_root) {
            if (bridge->last_root) {
                emit_notification(
                    bridge, NSAccessibilityUIElementDestroyedNotification,
                    bridge->last_root);
            }
            if (current) {
                emit_notification(bridge, NSAccessibilityCreatedNotification, current);
            }
        }
        [current retain];
        [bridge->last_root release];
        bridge->last_root = current;
    }

    if (structure || bounds) {
        id const target = bridge->last_root ? (id)bridge->last_root : (id)bridge->view;
        emit_notification(bridge, NSAccessibilityLayoutChangedNotification, target);
    }

    if (focus) {
        NSAccessibilityElement* const focused = focused_element(bridge);
        emit_notification(bridge, NSAccessibilityFocusedUIElementChangedNotification,
                          focused ? (id)focused : (id)bridge->view);
    }

    if (selection) {
        id const target = bridge->last_root ? (id)bridge->last_root : (id)bridge->view;
        emit_notification(bridge, NSAccessibilitySelectedChildrenChangedNotification,
                          target);
    }

    if (value) {
        id const target = bridge->last_root ? (id)bridge->last_root : (id)bridge->view;
        emit_notification(bridge, NSAccessibilityValueChangedNotification, target);
    }
}

[[nodiscard]] bool add_view_override(Class subclass,
                                     Class original,
                                     const char* selector_name,
                                     IMP implementation) noexcept {
    const SEL selector = sel_registerName(selector_name);
    Method const inherited = class_getInstanceMethod(original, selector);
    return inherited &&
           class_addMethod(subclass, selector, implementation,
                           method_getTypeEncoding(inherited));
}

// The native view is the accessibility container for the semantic root. It is
// never itself an element; its children are the lazily materialized proxies.
BOOL nativeui_accessibility_view_is_element(id self, SEL) {
    (void)self;
    return NO;
}

NSArray* nativeui_accessibility_view_children(id self, SEL) {
    if (![NSThread isMainThread]) return @[];
    NativeUIAccessibilityBridge* const bridge = bridge_for_view(self);
    if (!bridge || !bridge->cache) return @[];
    NSAccessibilityElement* const root = bridge->cache->root();
    if (!root) return @[];
    return @[root];
}

id nativeui_accessibility_view_focused_element(id self, SEL) {
    if (![NSThread isMainThread]) return nil;
    NativeUIAccessibilityBridge* const bridge = bridge_for_view(self);
    if (!bridge) return nil;
    return focused_element(bridge);
}

/// Lazily build the consumer-scoped view subclass. A pre-existing conflicting
/// class (wrong superclass) fails closed; an existing identical class is reused
/// so several views of the same consumer class share one runtime class.
[[nodiscard]] Class accessibility_view_subclass(Class original) noexcept {
    if (!original) return Nil;
    const char* const original_name = class_getName(original);
    if (!original_name) return Nil;

    const std::size_t name_size =
        std::strlen(original_name) + sizeof(kAccessibilityViewSuffix);
    char* const runtime_name = static_cast<char*>(std::calloc(name_size, 1U));
    if (!runtime_name) return Nil;
    std::snprintf(runtime_name, name_size, "%s%s", original_name,
                  kAccessibilityViewSuffix);

    if (Class existing = objc_lookUpClass(runtime_name)) {
        std::free(runtime_name);
        return class_getSuperclass(existing) == original ? existing : Nil;
    }

    Class const subclass = objc_allocateClassPair(original, runtime_name, 0U);
    std::free(runtime_name);
    if (!subclass) return Nil;

    const bool configured =
        add_view_override(subclass, original, "isAccessibilityElement",
                          reinterpret_cast<IMP>(
                              &nativeui_accessibility_view_is_element)) &&
        add_view_override(subclass, original, "accessibilityChildren",
                          reinterpret_cast<IMP>(
                              &nativeui_accessibility_view_children)) &&
        add_view_override(subclass, original, "accessibilityFocusedUIElement",
                          reinterpret_cast<IMP>(
                              &nativeui_accessibility_view_focused_element));
    if (!configured) {
        objc_disposeClassPair(subclass);
        return Nil;
    }

    objc_registerClassPair(subclass);
    return subclass;
}

} // namespace

NativeUIAccessibilityBridge*
nativeuiAccessibilityCreate(void* native_view, const void* binding)
{
    if (!native_view || !binding || ![NSThread isMainThread]) {
        return NULL;
    }

    NSView* const view = reinterpret_cast<NSView*>(native_view);
    const auto* const attach_binding =
        static_cast<const NativeAccessibilityAttachBinding*>(binding);
    if (attach_binding->publication_source.expired()) {
        return NULL;
    }
    if (bridge_for_view(view)) {
        return NULL;
    }

    Class const original = object_getClass(view);
    const char* const original_name = original ? class_getName(original) : nullptr;
    if (!original_name ||
        std::strncmp(original_name, kConsumerRuntimePrefix,
                     sizeof(kConsumerRuntimePrefix) - 1U) != 0) {
        return NULL;
    }

    Class const target = accessibility_view_subclass(original);
    if (!target) {
        return NULL;
    }

    auto* const bridge = new (std::nothrow) NativeUIAccessibilityBridge();
    if (!bridge) {
        return NULL;
    }

    @try {
        bridge->cache = std::make_unique<MacOSAccessibilityProxyCache>(
            original,
            attach_binding->publication_source,
            attach_binding->action_endpoint);
    } @catch (...) {
        delete bridge;
        return NULL;
    }

    bridge->view = view;
    bridge->original_view_class = original;
    bridge->accessibility_view_class = target;
    bridge->publication_source = attach_binding->publication_source;
    bridge->action_endpoint = attach_binding->action_endpoint;

    objc_setAssociatedObject(
        view,
        &kAccessibilityBridgeAssociationKey,
        [NSValue valueWithPointer:bridge],
        OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    object_setClass(view, target);
    return bridge;
}

void
nativeuiAccessibilityDestroy(NativeUIAccessibilityBridge* bridge)
{
    if (!bridge) {
        return;
    }

    NSView* const view = bridge->view;
    if (view) {
        if (object_getClass(view) == bridge->accessibility_view_class) {
            object_setClass(view, bridge->original_view_class);
        }
        objc_setAssociatedObject(
            view, &kAccessibilityBridgeAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
        bridge->view = nil;
    }

    [bridge->last_root release];
    bridge->last_root = nil;
    bridge->cache.reset();
    delete bridge;
}

bool
nativeuiAccessibilityDeliver(NativeUIAccessibilityBridge* bridge, const void* batch)
{
    if (!bridge || !batch || ![NSThread isMainThread]) {
        return false;
    }

    try {
        const auto& publication_batch =
            *static_cast<const SemanticNativePublicationBatch*>(batch);
        if (!publication_batch.publication) {
            return false;
        }

        const auto source = bridge->publication_source.lock();
        if (!source) {
            return false;
        }
        const auto current = source->current();
        if (!current || current.get() != publication_batch.publication.get()) {
            // A superseded or foreign batch must never notify for this view's
            // current accessibility tree.
            return false;
        }

        emit_committed_batch(bridge, publication_batch);
        return true;
    } catch (...) {
        return false;
    }
}

void*
nativeuiAccessibilityTargetClass(NativeUIAccessibilityBridge* bridge)
{
    return bridge ? reinterpret_cast<void*>(bridge->accessibility_view_class) : NULL;
}

void
nativeuiAccessibilitySetNotificationRecorderForTest(
    NativeUIAccessibilityBridge* bridge,
    void* user_data,
    NativeUIAccessibilityNotificationRecorder recorder)
{
    if (!bridge) {
        return;
    }
    bridge->recorder_user_data = user_data;
    bridge->recorder = recorder;
}
