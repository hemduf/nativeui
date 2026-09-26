#pragma once

#if !defined(__OBJC__)
#error "semantic_macos_interaction.hpp requires Objective-C++"
#endif

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <nativeui/detail/semantic_action.hpp>
#include <nativeui/detail/semantic_macos_mapping.hpp>

#include <cmath>
#include <optional>
#include <string>

namespace ui::detail {

/// Concrete AppKit dispatch forms for the backend-neutral macOS interaction
/// families. Action methods have no argument, BooleanSetter writes one BOOL, and
/// ValueSetter forwards the semantic action request's value through the native
/// boundary. This type is private to Objective-C++ so AppKit selectors never
/// enter NativeUI's platform-neutral semantic headers.
enum class MacOSAccessibilityAppKitInteractionKind {
    ActionMethod,
    BooleanSetter,
    ValueSetter,
};

struct MacOSAccessibilityAppKitInteraction final {
    SEL selector{nullptr};
    MacOSAccessibilityAppKitInteractionKind kind{
        MacOSAccessibilityAppKitInteractionKind::ActionMethod};
    std::optional<bool> boolean_value;
};

/// Translate one frozen semantic interaction family to the exact standard
/// AppKit selector shape without executing it. Malformed family/value pairs fail
/// closed so the eventual Objective-C runtime callback cannot silently invent a
/// writable-attribute value.
[[nodiscard]] inline std::optional<MacOSAccessibilityAppKitInteraction>
macos_accessibility_appkit_interaction(
    MacOSAccessibilityInteractionMapping mapping) noexcept {
    using Interaction = MacOSAccessibilityInteraction;
    using Kind = MacOSAccessibilityAppKitInteractionKind;
    using Result = MacOSAccessibilityAppKitInteraction;

    if (mapping.interaction != Interaction::Expanded &&
        mapping.boolean_value.has_value()) {
        return std::nullopt;
    }

    switch (mapping.interaction) {
        case Interaction::Press:
            return Result{
                @selector(accessibilityPerformPress), Kind::ActionMethod, std::nullopt};
        case Interaction::Selection:
            return Result{
                @selector(setAccessibilitySelected:), Kind::BooleanSetter, true};
        case Interaction::Focus:
            return Result{
                @selector(setAccessibilityFocused:), Kind::BooleanSetter, true};
        case Interaction::Increment:
            return Result{
                @selector(accessibilityPerformIncrement), Kind::ActionMethod, std::nullopt};
        case Interaction::Decrement:
            return Result{
                @selector(accessibilityPerformDecrement), Kind::ActionMethod, std::nullopt};
        case Interaction::SetValue:
            return Result{
                @selector(setAccessibilityValue:), Kind::ValueSetter, std::nullopt};
        case Interaction::Expanded:
            if (!mapping.boolean_value.has_value()) {
                return std::nullopt;
            }
            return Result{
                @selector(setAccessibilityExpanded:),
                Kind::BooleanSetter,
                *mapping.boolean_value};
    }

    return std::nullopt;
}

[[nodiscard]] inline bool macos_accessibility_appkit_action_selector(
    SEL selector) noexcept {
    if (!selector) {
        return false;
    }

    return sel_isEqual(selector, @selector(accessibilityPerformPress)) ||
           sel_isEqual(selector, @selector(accessibilityPerformIncrement)) ||
           sel_isEqual(selector, @selector(accessibilityPerformDecrement)) ||
           sel_isEqual(selector, @selector(setAccessibilityFocused:)) ||
           sel_isEqual(selector, @selector(setAccessibilitySelected:)) ||
           sel_isEqual(selector, @selector(setAccessibilityExpanded:)) ||
           sel_isEqual(selector, @selector(setAccessibilityValue:));
}

[[nodiscard]] inline bool macos_accessibility_appkit_selector_allowed(
    const SemanticInfo& info,
    SEL selector) noexcept {
    if (!macos_accessibility_appkit_action_selector(selector)) {
        return false;
    }

    std::optional<MacOSAccessibilityAppKitInteraction> matched;
    for (const SemanticAction action : info.actions) {
        if (!semantic_action_allowed(info, action)) {
            continue;
        }

        const auto mapped = macos_accessibility_interaction_mapping(action);
        if (!mapped) {
            continue;
        }
        const auto appkit = macos_accessibility_appkit_interaction(*mapped);
        if (!appkit || !sel_isEqual(appkit->selector, selector)) {
            continue;
        }

        if (action == SemanticAction::SetValue) {
            const bool numeric_domain =
                info.numeric_value.has_value() || info.value_range.has_value();
            const bool text_domain = info.text_value.has_value();
            if (numeric_domain == text_domain) {
                continue;
            }
        }

        if (!matched) {
            matched = *appkit;
            continue;
        }

        const bool opposite_boolean_directions =
            matched->kind == MacOSAccessibilityAppKitInteractionKind::BooleanSetter &&
            appkit->kind == MacOSAccessibilityAppKitInteractionKind::BooleanSetter &&
            matched->boolean_value.has_value() &&
            appkit->boolean_value.has_value() &&
            matched->boolean_value != appkit->boolean_value;
        if (!opposite_boolean_directions) {
            return false;
        }
    }

    return matched.has_value();
}

/// Resolve one concrete AppKit callback shape back to the single currently
/// advertised NativeUI semantic action that it represents.
///
/// This is deliberately driven by the current immutable SemanticInfo rather
/// than by role guesses. The two press actions (Activate/Toggle) share an AppKit
/// selector, so a malformed/custom node advertising both is ambiguous and fails
/// closed. Boolean setters match only their frozen direction: `YES` for Focus and
/// Select, `YES`/`NO` for Expand/Collapse. Disabled/read-only eligibility uses
/// the same policy as SemanticActionRouter before any work can be enqueued.
[[nodiscard]] inline std::optional<SemanticAction>
macos_accessibility_semantic_action_for_appkit(
    const SemanticInfo& info,
    SEL selector,
    std::optional<bool> boolean_value = std::nullopt) noexcept {
    if (!selector) {
        return std::nullopt;
    }

    std::optional<SemanticAction> matched;
    for (const SemanticAction action : info.actions) {
        if (!semantic_action_allowed(info, action)) {
            continue;
        }

        const auto mapped = macos_accessibility_interaction_mapping(action);
        if (!mapped) {
            continue;
        }
        const auto appkit = macos_accessibility_appkit_interaction(*mapped);
        if (!appkit || !sel_isEqual(appkit->selector, selector)) {
            continue;
        }

        const bool direction_matches =
            appkit->kind == MacOSAccessibilityAppKitInteractionKind::BooleanSetter
            ? boolean_value == appkit->boolean_value
            : !boolean_value.has_value();
        if (!direction_matches) {
            continue;
        }

        if (matched.has_value()) {
            // One native callback must never silently choose between two
            // currently advertised NativeUI actions.
            return std::nullopt;
        }
        matched = action;
    }

    return matched;
}

/// Prepare an argument-free or Boolean AppKit action request. SetValue is kept
/// out of this overload so a writable value callback cannot accidentally enqueue
/// a mutation without carrying the native value supplied by AppKit.
[[nodiscard]] inline std::optional<SemanticActionRequest>
macos_accessibility_appkit_action_request(
    const SemanticInfo& info,
    SEL selector,
    std::optional<bool> boolean_value = std::nullopt) noexcept {
    const auto action = macos_accessibility_semantic_action_for_appkit(
        info, selector, boolean_value);
    if (!action || *action == SemanticAction::SetValue) {
        return std::nullopt;
    }

    SemanticActionRequest request;
    request.action = *action;
    return request;
}

/// Prepare the writable-value request at the Objective-C++ boundary.
///
/// The current immutable semantic value determines the accepted payload domain:
/// numeric/ranged values accept NSNumber, text values accept NSString, and an
/// absent or ambiguous value domain fails closed. Conversion is bounded to the
/// supplied scalar/string and contains both Objective-C and C++ exceptions; no
/// exception may escape the eventual NSAccessibility callback. Range policy is
/// intentionally left to the existing UI-thread semantic action handler after
/// T065 revalidation, while non-finite numeric values are rejected here.
[[nodiscard]] inline std::optional<SemanticActionRequest>
macos_accessibility_appkit_value_action_request(
    const SemanticInfo& info,
    SEL selector,
    id value) noexcept {
    try {
        const auto action = macos_accessibility_semantic_action_for_appkit(
            info, selector, std::nullopt);
        if (!action || *action != SemanticAction::SetValue || !value) {
            return std::nullopt;
        }

        const bool numeric_domain =
            info.numeric_value.has_value() || info.value_range.has_value();
        const bool text_domain = info.text_value.has_value();
        if (numeric_domain == text_domain) {
            return std::nullopt;
        }

        SemanticActionRequest request;
        request.action = SemanticAction::SetValue;

        @try {
            if (numeric_domain) {
                if (![value isKindOfClass:[NSNumber class]]) {
                    return std::nullopt;
                }
                const double numeric_value = [(NSNumber*)value doubleValue];
                if (!std::isfinite(numeric_value)) {
                    return std::nullopt;
                }
                request.numeric_value = numeric_value;
                return request;
            }

            if (![value isKindOfClass:[NSString class]]) {
                return std::nullopt;
            }
            const char* const utf8 = [(NSString*)value UTF8String];
            if (!utf8) {
                return std::nullopt;
            }
            request.text_value = std::string{utf8};
            return request;
        } @catch (...) {
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace ui::detail
