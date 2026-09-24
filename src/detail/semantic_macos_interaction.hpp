#pragma once

#if !defined(__OBJC__)
#error "semantic_macos_interaction.hpp requires Objective-C++"
#endif

#import <AppKit/AppKit.h>

#include <nativeui/detail/semantic_macos_mapping.hpp>

#include <optional>

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

} // namespace ui::detail
