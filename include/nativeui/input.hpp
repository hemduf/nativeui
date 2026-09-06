#pragma once

#include <nativeui/geometry.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

enum class Key {
    None,
    Tab,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Backspace,
    Delete,
    Space,
    Enter,
    Escape,
    A,
    C,
    V,
    X,
    Y,
    Z,
    Quit
};

enum class Command {
    None,
    Copy,
    Cut,
    Paste,
    SelectAll,
    Undo,
    Redo
};

enum class InputType {
    None,
    KeyDown,
    KeyUp,
    Command,
    TextInput,
    Tick,
    PointerDown,
    PointerMove,
    PointerUp,
    PointerCancel,
    PointerWheel,
    DropOffer,
    DropData,
    Resize,
    Quit
};

/// Result returned by a component after receiving an input event.
///
/// `Handled` means the current component consumed the event and stops routing.
/// `Ignored` means the event may continue to the component's parent. NativeUI
/// first selects one authoritative leaf target (focus, hit-test, or capture),
/// then bubbles ignored events through that target's ancestor chain.
enum class EventResult {
    Ignored,
    Handled
};

[[nodiscard]] constexpr bool handled(EventResult result) noexcept {
    return result == EventResult::Handled;
}

struct InputEvent {
    InputType type{InputType::None};
    Key key{Key::None};
    Command command{Command::None};
    Point position{};
    Point delta{};
    std::string text;
    // Drag-and-drop payload. `drop_types` is populated for DropOffer, while
    // `drop_type` + `drop_data` are populated for DropData. Clipboard paste
    // continues to use TextInput and never populates these fields.
    std::vector<std::string> drop_types;
    std::string drop_type;
    std::vector<std::uint8_t> drop_data;
    int clicks{1};
    bool shift{};
    bool ctrl{};
    bool alt{};
    bool gui{};
    // Platform-normalized primary accelerator: Command on macOS, Ctrl on
    // Windows/Linux. Platform adapters set this explicitly.
    bool primary{};

    [[nodiscard]] bool primary_shortcut() const noexcept { return primary; }
    [[nodiscard]] bool offers_drop_type(std::string_view type) const noexcept {
        for (const auto& offered : drop_types) {
            if (offered == type) return true;
        }
        return false;
    }
};

[[nodiscard]] constexpr Command command_from_shortcut(const InputEvent& event) noexcept {
    if (event.type != InputType::KeyDown || !event.primary_shortcut()) return Command::None;
    switch (event.key) {
        case Key::A: return Command::SelectAll;
        case Key::C: return Command::Copy;
        case Key::X: return Command::Cut;
        case Key::V: return Command::Paste;
        case Key::Z: return event.shift ? Command::Redo : Command::Undo;
        case Key::Y: return Command::Redo;
        default: return Command::None;
    }
}

namespace detail {

struct NormalizedTabKey {
    bool is_tab{};
    bool shift{};
};

// Some native input systems represent reverse focus traversal as the ASCII
// BackTab control character (U+0019) rather than Tab with a Shift modifier.
// Normalize both forms before events enter the retained-mode tree so focus
// traversal stays platform-neutral and independently testable.
[[nodiscard]] constexpr NormalizedTabKey
normalize_tab_key(std::uint32_t key, bool shift) noexcept {
    if (key == 0x09U) return NormalizedTabKey{true, shift};
    if (key == 0x19U) return NormalizedTabKey{true, true};
    return NormalizedTabKey{false, shift};
}

} // namespace detail


} // namespace ui
