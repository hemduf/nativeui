#pragma once

#include <nativeui/semantics.hpp>

#include <optional>

namespace ui::detail {

// Backend-neutral tokens for the fixed macOS accessibility role mapping.
// The Objective-C++ boundary translates these tokens to AppKit constants; no
// AppKit type crosses a NativeUI C++ header or immutable semantic snapshot.
enum class MacOSAccessibilityRole {
    Button,
    CheckBox,
    RadioButton,
    Slider,
    ProgressIndicator,
    LevelIndicator,
    StaticText,
    TextField,
    TextArea,
    ComboBox,
    Menu,
    MenuItem,
    List,
    Row,
    TabGroup,
    Group,
    Window,
    Image,
};

enum class MacOSAccessibilitySubrole {
    None,
    TabButton,
    Dialog,
};

struct MacOSAccessibilityRoleMapping final {
    MacOSAccessibilityRole role{};
    MacOSAccessibilitySubrole subrole{MacOSAccessibilitySubrole::None};

    bool operator==(const MacOSAccessibilityRoleMapping&) const = default;
};

/// Backend-neutral interaction tokens for the fixed macOS action contract.
/// The Objective-C++ boundary remains responsible for selecting the concrete
/// AppKit selector or writable attribute appropriate to the current role. In
/// particular, Selection is deliberately distinct from Press so role-specific
/// selection semantics are not guessed in this platform-neutral layer.
enum class MacOSAccessibilityInteraction {
    Press,
    Selection,
    Focus,
    Increment,
    Decrement,
    SetValue,
    Expanded,
};

struct MacOSAccessibilityInteractionMapping final {
    MacOSAccessibilityInteraction interaction{};
    std::optional<bool> boolean_value;

    bool operator==(const MacOSAccessibilityInteractionMapping&) const = default;
};

/// Map the closed NativeUI semantic role set to the fixed macOS accessibility
/// contract. SemanticRole::None is intentionally absent because flattened
/// layout wrappers never receive a native accessibility object of their own.
[[nodiscard]] constexpr std::optional<MacOSAccessibilityRoleMapping>
macos_accessibility_role_mapping(SemanticRole role) noexcept {
    using Mapping = MacOSAccessibilityRoleMapping;
    using NativeRole = MacOSAccessibilityRole;
    using Subrole = MacOSAccessibilitySubrole;

    switch (role) {
        case SemanticRole::None:
            return std::nullopt;
        case SemanticRole::Button:
            return Mapping{NativeRole::Button};
        case SemanticRole::Checkbox:
            return Mapping{NativeRole::CheckBox};
        case SemanticRole::RadioButton:
            return Mapping{NativeRole::RadioButton};
        case SemanticRole::Toggle:
            return Mapping{NativeRole::CheckBox};
        case SemanticRole::Slider:
        case SemanticRole::RangeSliderHandle:
            return Mapping{NativeRole::Slider};
        case SemanticRole::ProgressBar:
            return Mapping{NativeRole::ProgressIndicator};
        case SemanticRole::Meter:
            return Mapping{NativeRole::LevelIndicator};
        case SemanticRole::Text:
            return Mapping{NativeRole::StaticText};
        case SemanticRole::TextInput:
            return Mapping{NativeRole::TextField};
        case SemanticRole::TextArea:
            return Mapping{NativeRole::TextArea};
        case SemanticRole::ComboBox:
            return Mapping{NativeRole::ComboBox};
        case SemanticRole::PopupMenu:
            return Mapping{NativeRole::Menu};
        case SemanticRole::MenuItem:
            return Mapping{NativeRole::MenuItem};
        case SemanticRole::ListView:
            return Mapping{NativeRole::List};
        case SemanticRole::ListItem:
            return Mapping{NativeRole::Row};
        case SemanticRole::Tabs:
            return Mapping{NativeRole::TabGroup};
        case SemanticRole::Tab:
            return Mapping{NativeRole::RadioButton, Subrole::TabButton};
        case SemanticRole::TabPanel:
            return Mapping{NativeRole::Group};
        case SemanticRole::Dialog:
            return Mapping{NativeRole::Window, Subrole::Dialog};
        case SemanticRole::Group:
            return Mapping{NativeRole::Group};
        case SemanticRole::Image:
            return Mapping{NativeRole::Image};
        case SemanticRole::Custom:
            return Mapping{NativeRole::Group};
    }

    return std::nullopt;
}

/// Project one advertised NativeUI semantic action onto the closed macOS
/// interaction families. This intentionally does not decide role-specific
/// AppKit selector/attribute details; it only preserves the frozen semantic
/// distinction needed by the Objective-C++ boundary.
[[nodiscard]] constexpr std::optional<MacOSAccessibilityInteractionMapping>
macos_accessibility_interaction_mapping(SemanticAction action) noexcept {
    using Interaction = MacOSAccessibilityInteraction;
    using Mapping = MacOSAccessibilityInteractionMapping;

    switch (action) {
        case SemanticAction::Activate:
        case SemanticAction::Toggle:
            return Mapping{Interaction::Press};
        case SemanticAction::Focus:
            return Mapping{Interaction::Focus};
        case SemanticAction::Increment:
            return Mapping{Interaction::Increment};
        case SemanticAction::Decrement:
            return Mapping{Interaction::Decrement};
        case SemanticAction::SetValue:
            return Mapping{Interaction::SetValue};
        case SemanticAction::Select:
            return Mapping{Interaction::Selection};
        case SemanticAction::Expand:
            return Mapping{Interaction::Expanded, true};
        case SemanticAction::Collapse:
            return Mapping{Interaction::Expanded, false};
    }

    return std::nullopt;
}

} // namespace ui::detail
