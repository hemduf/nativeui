#pragma once

#if !defined(__OBJC__)
#error "semantic_macos_appkit.hpp requires Objective-C++"
#endif

#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <nativeui/detail/semantic_macos_mapping.hpp>
#include <nativeui/detail/semantic_native_query.hpp>

#include "semantic_macos_children.hpp"
#include "semantic_macos_frame.hpp"
#include "semantic_native_bounds.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace ui::detail {

/// Lifetime-safe callback surface used by a macOS accessibility proxy to resolve
/// its children without owning the per-view proxy cache. Implementations remain
/// AppKit/main-thread confined and must answer from immutable semantic publication
/// data only. A proxy stores this surface weakly, so retiring the owning view
/// cannot leave a raw back-pointer or form a proxy/cache ownership cycle.
class MacOSAccessibilityChildResolver {
public:
    using ElementRange = std::vector<NSAccessibilityElement*>;

    virtual ~MacOSAccessibilityChildResolver() noexcept = default;

    [[nodiscard]] virtual std::optional<std::size_t> appkit_child_count(
        SemanticId parent_node_id) noexcept = 0;

    [[nodiscard]] virtual std::optional<ElementRange> appkit_children_range(
        SemanticId parent_node_id,
        std::size_t start,
        std::size_t max_count) noexcept = 0;
};

// Private Objective-C++ boundary for the fixed macOS accessibility role mapping. Keep AppKit
// objects behind this installed platform-source header so normal NativeUI C++
// headers and immutable semantic snapshots remain platform-neutral.
[[nodiscard]] inline NSString*
macos_accessibility_appkit_role(MacOSAccessibilityRole role) noexcept {
    switch (role) {
        case MacOSAccessibilityRole::Button:
            return NSAccessibilityButtonRole;
        case MacOSAccessibilityRole::CheckBox:
            return NSAccessibilityCheckBoxRole;
        case MacOSAccessibilityRole::RadioButton:
            return NSAccessibilityRadioButtonRole;
        case MacOSAccessibilityRole::Slider:
            return NSAccessibilitySliderRole;
        case MacOSAccessibilityRole::ProgressIndicator:
            return NSAccessibilityProgressIndicatorRole;
        case MacOSAccessibilityRole::LevelIndicator:
            return NSAccessibilityLevelIndicatorRole;
        case MacOSAccessibilityRole::StaticText:
            return NSAccessibilityStaticTextRole;
        case MacOSAccessibilityRole::TextField:
            return NSAccessibilityTextFieldRole;
        case MacOSAccessibilityRole::TextArea:
            return NSAccessibilityTextAreaRole;
        case MacOSAccessibilityRole::ComboBox:
            return NSAccessibilityComboBoxRole;
        case MacOSAccessibilityRole::Menu:
            return NSAccessibilityMenuRole;
        case MacOSAccessibilityRole::MenuItem:
            return NSAccessibilityMenuItemRole;
        case MacOSAccessibilityRole::List:
            return NSAccessibilityListRole;
        case MacOSAccessibilityRole::Row:
            return NSAccessibilityRowRole;
        case MacOSAccessibilityRole::TabGroup:
            return NSAccessibilityTabGroupRole;
        case MacOSAccessibilityRole::Group:
            return NSAccessibilityGroupRole;
        case MacOSAccessibilityRole::Window:
            return NSAccessibilityWindowRole;
        case MacOSAccessibilityRole::Image:
            return NSAccessibilityImageRole;
    }

    return nil;
}

[[nodiscard]] inline NSString*
macos_accessibility_appkit_subrole(MacOSAccessibilitySubrole subrole) noexcept {
    switch (subrole) {
        case MacOSAccessibilitySubrole::None:
            return nil;
        case MacOSAccessibilitySubrole::TabButton:
            return NSAccessibilityTabButtonSubrole;
        case MacOSAccessibilitySubrole::Dialog:
            return NSAccessibilityDialogSubrole;
    }

    return nil;
}

/// Lifetime-safe state carried by one lazy macOS accessibility proxy.
///
/// The state stores only weak per-view publication and child-resolver endpoints
/// plus stable semantic identity. Every semantic read loads the latest exact
/// semantic+geometry publication once and resolves through
/// SemanticNativeSnapshotQuery. It never retains a bridge, ViewCore, Tree, Node,
/// Component, native view, or cache owner. Once the owning view retires either
/// endpoint, future work fails closed while an already-started callback may
/// finish from the immutable generation or endpoint lease it already retained.
class MacOSAccessibilityProxyState final {
public:
    MacOSAccessibilityProxyState(const MacOSAccessibilityProxyState&) = default;
    MacOSAccessibilityProxyState& operator=(const MacOSAccessibilityProxyState&) = default;
    MacOSAccessibilityProxyState(MacOSAccessibilityProxyState&&) noexcept = default;
    MacOSAccessibilityProxyState& operator=(MacOSAccessibilityProxyState&&) noexcept = default;

    [[nodiscard]] static std::optional<MacOSAccessibilityProxyState> ordinary(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId node_id,
        std::weak_ptr<MacOSAccessibilityChildResolver> child_resolver = {}) noexcept {
        if (node_id == kInvalidSemanticId) {
            return std::nullopt;
        }
        return MacOSAccessibilityProxyState{
            std::move(publication_source),
            node_id,
            std::nullopt,
            std::move(child_resolver)};
    }

    [[nodiscard]] static std::optional<MacOSAccessibilityProxyState> virtual_item(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId list_node_id,
        VirtualSemanticItemToken token,
        std::weak_ptr<MacOSAccessibilityChildResolver> child_resolver = {}) noexcept {
        if (list_node_id == kInvalidSemanticId ||
            token == kInvalidVirtualSemanticItemToken) {
            return std::nullopt;
        }
        return MacOSAccessibilityProxyState{
            std::move(publication_source),
            list_node_id,
            token,
            std::move(child_resolver)};
    }

    [[nodiscard]] SemanticId node_id() const noexcept {
        return node_id_;
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_token() const noexcept {
        return virtual_token_;
    }

    [[nodiscard]] std::weak_ptr<MacOSAccessibilityChildResolver>
    child_resolver_endpoint() const noexcept {
        return child_resolver_endpoint_;
    }

    [[nodiscard]] std::optional<SemanticNativeSnapshotRead> read() const {
        const auto source = publication_source_.lock();
        if (!source) {
            return std::nullopt;
        }

        auto publication = source->current();
        if (!publication) {
            return std::nullopt;
        }

        if (virtual_token_) {
            return SemanticNativeSnapshotQuery::virtual_item(
                std::move(publication), node_id_, *virtual_token_);
        }
        return SemanticNativeSnapshotQuery::ordinary(
            std::move(publication), node_id_);
    }

private:
    MacOSAccessibilityProxyState(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId node_id,
        std::optional<VirtualSemanticItemToken> virtual_token,
        std::weak_ptr<MacOSAccessibilityChildResolver> child_resolver) noexcept
        : publication_source_(std::move(publication_source)),
          child_resolver_endpoint_(std::move(child_resolver)),
          node_id_(node_id),
          virtual_token_(virtual_token) {}

    std::weak_ptr<const SemanticNativePublicationSource> publication_source_;
    std::weak_ptr<MacOSAccessibilityChildResolver> child_resolver_endpoint_;
    SemanticId node_id_{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token_;
};

/// One callback-local macOS accessibility read projected from exactly one native
/// publication generation. Role, semantic properties and screen geometry are
/// therefore impossible to mix across generations even if the UI publishes while
/// AppKit is servicing an accessibility query. The object retains only immutable
/// publication data and remains valid after later publication or view teardown.
class MacOSAccessibilityProxyRead final {
public:
    [[nodiscard]] static std::optional<MacOSAccessibilityProxyRead> from_state(
        const MacOSAccessibilityProxyState& state) {
        auto read = state.read();
        if (!read) {
            return std::nullopt;
        }

        const auto mapping = macos_accessibility_role_mapping(read->info().role);
        if (!mapping) {
            // SemanticRole::None is flattened and must never materialize a native
            // accessibility object of its own.
            return std::nullopt;
        }

        return MacOSAccessibilityProxyRead{std::move(*read), *mapping};
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return read_.generation();
    }

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return read_.semantic_generation();
    }

    [[nodiscard]] SemanticId node_id() const noexcept {
        return read_.node_id();
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_token() const noexcept {
        return read_.virtual_token();
    }

    [[nodiscard]] const SemanticInfo& info() const noexcept {
        return read_.info();
    }

    [[nodiscard]] Rect logical_bounds() const noexcept {
        return read_.logical_bounds();
    }

    [[nodiscard]] const SemanticNativeGeometry& geometry() const noexcept {
        return read_.geometry();
    }

    [[nodiscard]] const MacOSAccessibilityRoleMapping& role_mapping() const noexcept {
        return role_mapping_;
    }

    [[nodiscard]] Rect physical_screen_bounds() const noexcept {
        return physical_screen_bounds_;
    }

private:
    MacOSAccessibilityProxyRead(
        SemanticNativeSnapshotRead read,
        MacOSAccessibilityRoleMapping role_mapping) noexcept
        : read_(std::move(read)),
          role_mapping_(role_mapping),
          physical_screen_bounds_(
              SemanticNativeBoundsTransform{read_.geometry()}
                  .physical_screen_bounds(read_.logical_bounds())) {}

    SemanticNativeSnapshotRead read_;
    MacOSAccessibilityRoleMapping role_mapping_{};
    Rect physical_screen_bounds_{};
};

[[nodiscard]] inline std::optional<MacOSAccessibilityProxyRead>
macos_accessibility_proxy_read(const MacOSAccessibilityProxyState& state) {
    return MacOSAccessibilityProxyRead::from_state(state);
}

inline constexpr char kMacOSAccessibilityProxyStateIvar[] =
    "_nativeuiAccessibilityState";

[[nodiscard]] constexpr std::uint8_t
macos_accessibility_pointer_alignment_log2() noexcept {
    std::size_t alignment = alignof(void*);
    std::uint8_t power = 0U;
    while ((std::size_t{1U} << power) < alignment) {
        ++power;
    }
    return power;
}

[[nodiscard]] inline Ivar macos_accessibility_proxy_state_ivar(Class proxy_class) noexcept {
    return proxy_class
        ? class_getInstanceVariable(proxy_class, kMacOSAccessibilityProxyStateIvar)
        : nullptr;
}

[[nodiscard]] inline MacOSAccessibilityProxyState*
macos_accessibility_proxy_stored_state(id object) noexcept {
    if (!object) return nullptr;
    const Ivar ivar = macos_accessibility_proxy_state_ivar(object_getClass(object));
    if (!ivar) return nullptr;
    const std::ptrdiff_t offset = ivar_getOffset(ivar);
    if (offset < 0) return nullptr;

    MacOSAccessibilityProxyState* state = nullptr;
    const auto* bytes = reinterpret_cast<const unsigned char*>(object);
    std::memcpy(&state, bytes + offset, sizeof(state));
    return state;
}

[[nodiscard]] inline bool macos_accessibility_proxy_store_state(
    id object,
    MacOSAccessibilityProxyState* state) noexcept {
    if (!object) return false;
    const Ivar ivar = macos_accessibility_proxy_state_ivar(object_getClass(object));
    if (!ivar) return false;
    const std::ptrdiff_t offset = ivar_getOffset(ivar);
    if (offset < 0) return false;

    auto* bytes = reinterpret_cast<unsigned char*>(object);
    std::memcpy(bytes + offset, &state, sizeof(state));
    return true;
}

[[nodiscard]] inline std::optional<MacOSAccessibilityProxyRead>
macos_accessibility_proxy_callback_read(id object) noexcept {
    try {
        auto* const state = macos_accessibility_proxy_stored_state(object);
        if (!state) return std::nullopt;
        return macos_accessibility_proxy_read(*state);
    } catch (...) {
        return std::nullopt;
    }
}

inline BOOL macos_accessibility_proxy_is_element(id object, SEL) noexcept {
    return macos_accessibility_proxy_callback_read(object).has_value() ? YES : NO;
}

inline NSString* macos_accessibility_proxy_role(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    return read
        ? macos_accessibility_appkit_role(read->role_mapping().role)
        : nil;
}

inline NSString* macos_accessibility_proxy_subrole(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    return read
        ? macos_accessibility_appkit_subrole(read->role_mapping().subrole)
        : nil;
}

inline NSString* macos_accessibility_proxy_label(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    if (!read || read->info().name.empty()) return nil;

    @try {
        return [NSString stringWithUTF8String:read->info().name.c_str()];
    } @catch (...) {
        return nil;
    }
}

inline NSString* macos_accessibility_proxy_help(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    if (!read || read->info().description.empty()) return nil;

    @try {
        return [NSString stringWithUTF8String:read->info().description.c_str()];
    } @catch (...) {
        return nil;
    }
}

inline BOOL macos_accessibility_proxy_enabled(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    return read && read->info().enabled ? YES : NO;
}

inline BOOL macos_accessibility_proxy_focused(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    return read && read->info().focused ? YES : NO;
}

inline id macos_accessibility_proxy_value(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    if (!read) return nil;

    @try {
        const auto& info = read->info();
        if (info.text_value) {
            return [NSString stringWithUTF8String:info.text_value->c_str()];
        }
        if (info.numeric_value) {
            return [NSNumber numberWithDouble:*info.numeric_value];
        }
        switch (info.checked) {
            case SemanticCheckedState::Unchecked:
                return [NSNumber numberWithInteger:0];
            case SemanticCheckedState::Checked:
                return [NSNumber numberWithInteger:1];
            case SemanticCheckedState::Mixed:
                return [NSNumber numberWithInteger:2];
            case SemanticCheckedState::NotApplicable:
                break;
        }
    } @catch (...) {
        return nil;
    }

    return nil;
}

inline NSRect macos_accessibility_proxy_frame(id object, SEL) noexcept {
    const auto read = macos_accessibility_proxy_callback_read(object);
    if (!read) return NSZeroRect;

    @try {
        NSScreen* const main_screen = [NSScreen mainScreen];
        if (!main_screen) return NSZeroRect;

        const NSRect main_frame = [main_screen frame];
        const auto frame = macos_accessibility_screen_frame(
            read->physical_screen_bounds(),
            read->geometry().scale,
            static_cast<double>(main_frame.size.height));
        if (!frame) return NSZeroRect;

        return NSMakeRect(
            static_cast<CGFloat>(frame->x),
            static_cast<CGFloat>(frame->y),
            static_cast<CGFloat>(frame->width),
            static_cast<CGFloat>(frame->height));
    } @catch (...) {
        return NSZeroRect;
    }
}

[[nodiscard]] inline NSUInteger macos_accessibility_super_array_attribute_count(
    id object,
    SEL selector,
    NSString* attribute) noexcept {
    Class const proxy_class = object ? object_getClass(object) : Nil;
    Class const superclass = proxy_class ? class_getSuperclass(proxy_class) : Nil;
    if (!object || !superclass) return 0U;

    struct objc_super super_call { object, superclass };
    @try {
        using SuperCall = NSUInteger (*)(struct objc_super*, SEL, NSString*);
        return reinterpret_cast<SuperCall>(objc_msgSendSuper)(
            &super_call, selector, attribute);
    } @catch (...) {
        return 0U;
    }
}

[[nodiscard]] inline NSArray* macos_accessibility_super_array_attribute_values(
    id object,
    SEL selector,
    NSString* attribute,
    NSUInteger index,
    NSUInteger max_count) noexcept {
    Class const proxy_class = object ? object_getClass(object) : Nil;
    Class const superclass = proxy_class ? class_getSuperclass(proxy_class) : Nil;
    if (!object || !superclass) return nil;

    struct objc_super super_call { object, superclass };
    @try {
        using SuperCall = NSArray* (*)(
            struct objc_super*, SEL, NSString*, NSUInteger, NSUInteger);
        return reinterpret_cast<SuperCall>(objc_msgSendSuper)(
            &super_call, selector, attribute, index, max_count);
    } @catch (...) {
        return nil;
    }
}

inline NSUInteger macos_accessibility_proxy_array_attribute_count(
    id object,
    SEL selector,
    NSString* attribute) noexcept {
    @try {
        if (!attribute || ![attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
            return macos_accessibility_super_array_attribute_count(
                object, selector, attribute);
        }
    } @catch (...) {
        return 0U;
    }

    auto* const state = macos_accessibility_proxy_stored_state(object);
    if (!state || state->virtual_token().has_value()) return 0U;

    const auto resolver = state->child_resolver_endpoint().lock();
    if (!resolver) return 0U;

    const auto count = resolver->appkit_child_count(state->node_id());
    if (!count ||
        *count > static_cast<std::size_t>(std::numeric_limits<NSUInteger>::max())) {
        return 0U;
    }
    return static_cast<NSUInteger>(*count);
}

inline NSArray* macos_accessibility_proxy_array_attribute_values(
    id object,
    SEL selector,
    NSString* attribute,
    NSUInteger index,
    NSUInteger max_count) noexcept {
    @try {
        if (!attribute || ![attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
            return macos_accessibility_super_array_attribute_values(
                object, selector, attribute, index, max_count);
        }
    } @catch (...) {
        return nil;
    }

    auto* const state = macos_accessibility_proxy_stored_state(object);
    if (!state) return nil;

    @try {
        if (state->virtual_token().has_value() || max_count == 0U) {
            return [NSArray array];
        }
    } @catch (...) {
        return nil;
    }

    const auto resolver = state->child_resolver_endpoint().lock();
    if (!resolver) return nil;

    const auto range = resolver->appkit_children_range(
        state->node_id(),
        static_cast<std::size_t>(index),
        static_cast<std::size_t>(max_count));
    if (!range) return nil;

    @try {
        NSMutableArray* result = [NSMutableArray arrayWithCapacity:range->size()];
        for (NSAccessibilityElement* const element : *range) {
            if (!element) return nil;
            [result addObject:element];
        }
        return result;
    } @catch (...) {
        return nil;
    }
}

inline NSUInteger macos_accessibility_proxy_index_of_child(
    id object,
    SEL,
    id child) noexcept {
    auto* const parent_state = macos_accessibility_proxy_stored_state(object);
    auto* const child_state = macos_accessibility_proxy_stored_state(child);
    if (!parent_state || !child_state || parent_state->virtual_token().has_value()) {
        return NSNotFound;
    }

    const auto parent_resolver = parent_state->child_resolver_endpoint().lock();
    const auto child_resolver = child_state->child_resolver_endpoint().lock();
    if (!parent_resolver || !child_resolver ||
        parent_resolver.get() != child_resolver.get()) {
        return NSNotFound;
    }

    try {
        auto parent_read = parent_state->read();
        if (!parent_read || !macos_accessibility_role_mapping(parent_read->info().role)) {
            return NSNotFound;
        }

        MacOSAccessibilityChildProjection projection{std::move(*parent_read)};
        const std::size_t ordinary_count = projection.ordinary_child_count();
        const std::size_t virtual_count = projection.virtual_child_count();
        if (ordinary_count != 0U && virtual_count != 0U) {
            return NSNotFound;
        }

        std::optional<std::size_t> index;
        const auto child_token = child_state->virtual_token();
        if (child_token) {
            if (virtual_count == 0U || child_state->node_id() != parent_state->node_id()) {
                return NSNotFound;
            }
            index = projection.virtual_child_index_of(*child_token);
        } else {
            index = projection.ordinary_child_index_of(child_state->node_id());
        }

        if (!index ||
            *index > static_cast<std::size_t>(std::numeric_limits<NSUInteger>::max())) {
            return NSNotFound;
        }
        return static_cast<NSUInteger>(*index);
    } catch (...) {
        return NSNotFound;
    }
}

inline void macos_accessibility_proxy_dealloc(id object, SEL selector) noexcept {
    auto* const state = macos_accessibility_proxy_stored_state(object);
    (void)macos_accessibility_proxy_store_state(object, nullptr);
    delete state;

    Class const proxy_class = object ? object_getClass(object) : Nil;
    Class const superclass = proxy_class ? class_getSuperclass(proxy_class) : Nil;
    if (!object || !superclass) return;

    struct objc_super super_call { object, superclass };
    @try {
        reinterpret_cast<void (*)(struct objc_super*, SEL)>(objc_msgSendSuper)(
            &super_call, selector);
    } @catch (...) {
        // A foreign exception must never escape the Objective-C runtime callback.
    }
}

[[nodiscard]] inline bool macos_accessibility_add_proxy_override(
    Class proxy_class,
    SEL selector,
    IMP implementation) noexcept {
    if (!proxy_class || !selector || !implementation) return false;
    Method const inherited = class_getInstanceMethod(
        [NSAccessibilityElement class], selector);
    const char* const encoding = inherited ? method_getTypeEncoding(inherited) : nullptr;
    return encoding && class_addMethod(proxy_class, selector, implementation, encoding);
}

/// Return the runtime class used by lazy macOS accessibility proxy objects.
///
/// The supplied anchor is the already consumer-scoped Pugl view class. Deriving
/// the proxy name from it preserves that namespace without introducing another
/// global consumer registry or a generic NativeUI Objective-C class. Registration
/// is restricted to the main thread so lookup/allocation cannot race another
/// registration path. A conflicting pre-existing class fails closed.
[[nodiscard]] inline Class
macos_accessibility_appkit_proxy_class(Class consumer_view_class) noexcept {
    if (!consumer_view_class || ![NSThread isMainThread]) {
        return Nil;
    }

    const char* const anchor_name = class_getName(consumer_view_class);
    if (!anchor_name || std::strncmp(anchor_name, "NUI_", 4U) != 0) {
        return Nil;
    }

    static constexpr char kProxySuffix[] = "_NativeUIAccessibilityElement";
    const std::size_t anchor_size = std::strlen(anchor_name);
    const std::size_t runtime_name_size = anchor_size + sizeof(kProxySuffix);
    char* const runtime_name =
        static_cast<char*>(std::calloc(runtime_name_size, 1U));
    if (!runtime_name) {
        return Nil;
    }

    std::memcpy(runtime_name, anchor_name, anchor_size);
    std::memcpy(runtime_name + anchor_size, kProxySuffix, sizeof(kProxySuffix));

    if (Class existing = objc_lookUpClass(runtime_name)) {
        std::free(runtime_name);
        return class_getSuperclass(existing) == [NSAccessibilityElement class] &&
               macos_accessibility_proxy_state_ivar(existing)
            ? existing
            : Nil;
    }

    Class proxy_class = objc_allocateClassPair(
        [NSAccessibilityElement class], runtime_name, 0U);
    std::free(runtime_name);
    if (!proxy_class) {
        return Nil;
    }

    const bool configured =
        class_addIvar(
            proxy_class,
            kMacOSAccessibilityProxyStateIvar,
            sizeof(void*),
            macos_accessibility_pointer_alignment_log2(),
            "^v") &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(isAccessibilityElement),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_is_element)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilityRole),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_role)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilitySubrole),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_subrole)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilityLabel),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_label)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilityHelp),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_help)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(isAccessibilityEnabled),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_enabled)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(isAccessibilityFocused),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_focused)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilityValue),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_value)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            @selector(accessibilityFrame),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_frame)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            sel_registerName("accessibilityArrayAttributeCount:"),
            reinterpret_cast<IMP>(
                &macos_accessibility_proxy_array_attribute_count)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            sel_registerName("accessibilityArrayAttributeValues:index:maxCount:"),
            reinterpret_cast<IMP>(
                &macos_accessibility_proxy_array_attribute_values)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            sel_registerName("accessibilityIndexOfChild:"),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_index_of_child)) &&
        macos_accessibility_add_proxy_override(
            proxy_class,
            sel_registerName("dealloc"),
            reinterpret_cast<IMP>(&macos_accessibility_proxy_dealloc));

    if (!configured) {
        objc_disposeClassPair(proxy_class);
        return Nil;
    }

    objc_registerClassPair(proxy_class);
    return proxy_class;
}

/// Create one lazy consumer-scoped AppKit accessibility element from an exact
/// role mapping already retained by the caller. This overload deliberately does
/// not read the publication source again, so cache-miss construction cannot mix
/// a child identity from one generation with the role from a later generation.
[[nodiscard]] inline NSAccessibilityElement*
macos_accessibility_appkit_proxy_create(
    Class consumer_view_class,
    MacOSAccessibilityProxyState state,
    MacOSAccessibilityRoleMapping initial_role_mapping) noexcept {
    NSString* const initial_role =
        macos_accessibility_appkit_role(initial_role_mapping.role);
    if (!initial_role) return nil;

    Class const proxy_class =
        macos_accessibility_appkit_proxy_class(consumer_view_class);
    if (!proxy_class) return nil;

    NSAccessibilityElement* element = nil;
    @try {
        using Factory = NSAccessibilityElement* (*)(
            id, SEL, NSString*, NSRect, NSString*, id);
        const auto factory = reinterpret_cast<Factory>(objc_msgSend);
        element = factory(
            reinterpret_cast<id>(proxy_class),
            @selector(accessibilityElementWithRole:frame:label:parent:),
            initial_role,
            NSZeroRect,
            nil,
            nil);
    } @catch (...) {
        return nil;
    }
    if (!element || object_getClass(element) != proxy_class) return nil;

    auto* const stored_state =
        new (std::nothrow) MacOSAccessibilityProxyState(std::move(state));
    if (!stored_state) return nil;
    if (!macos_accessibility_proxy_store_state(element, stored_state)) {
        delete stored_state;
        return nil;
    }
    return element;
}

/// Convenience creation path when no exact callback-local read already exists.
/// It takes one current immutable read to validate the identity and then delegates
/// to the no-reload overload above. The returned object is autoreleased by AppKit;
/// its dynamic dealloc override owns deletion of the C++ state.
[[nodiscard]] inline NSAccessibilityElement*
macos_accessibility_appkit_proxy_create(
    Class consumer_view_class,
    MacOSAccessibilityProxyState state) noexcept {
    std::optional<MacOSAccessibilityProxyRead> initial_read;
    try {
        initial_read = macos_accessibility_proxy_read(state);
    } catch (...) {
        return nil;
    }
    if (!initial_read) return nil;

    return macos_accessibility_appkit_proxy_create(
        consumer_view_class,
        std::move(state),
        initial_read->role_mapping());
}

} // namespace ui::detail
