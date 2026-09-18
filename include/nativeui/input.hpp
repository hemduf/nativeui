#pragma once

#include <nativeui/geometry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
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
    Quit,
    // The remaining printable ASCII letters were appended after Quit so every
    // pre-existing public enumerator keeps its numeric value. Letter values are
    // therefore intentionally not contiguous; never derive Key values with
    // arithmetic on the enum representation.
    B,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    W
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

enum class CompositionType {
    Start,
    Update,
    Commit,
    Cancel
};

struct CompositionEvent {
    CompositionType type{CompositionType::Start};
    std::string text;
    std::size_t cursor_byte{};
    std::size_t selection_bytes{};
};

enum class InputType {
    None,
    KeyDown,
    KeyUp,
    Command,
    TextInput,
    Composition,
    Tick,
    PointerDown,
    PointerMove,
    PointerLeave,
    PointerUp,
    PointerCancel,
    PointerWheel,
    DropOffer,
    DropData,
    Resize,
    Quit,
    // Appended after Quit so every pre-existing public enumerator keeps its
    // numeric value. A context-menu request (right-button press or the
    // platform's equivalent) carries the logical position and modifiers like a
    // pointer press; routing must not change focus, capture or interaction state.
    ContextMenu
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
    CompositionEvent composition{};
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
    [[nodiscard]] bool offers_drop_type(std::string_view requested_type) const noexcept {
        for (const auto& offered : drop_types) {
            if (offered == requested_type) return true;
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

inline constexpr std::array<Key, 26> kAsciiLetterKeys{
    Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G,
    Key::H, Key::I, Key::J, Key::K, Key::L, Key::M, Key::N,
    Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T, Key::U,
    Key::V, Key::W, Key::X, Key::Y, Key::Z};

/// Translate the ASCII portion of a native key event after platform-specific
/// special keys have been handled. `primary` preserves NativeUI's existing
/// Primary+Q window-close shortcut; otherwise every ASCII letter A-Z maps to
/// its public Key value regardless of case. Text/IME input remains separate.
[[nodiscard]] constexpr Key translate_ascii_key(std::uint32_t key, bool primary) noexcept {
    if (key == static_cast<std::uint32_t>(' ')) return Key::Space;
    if (primary &&
        (key == static_cast<std::uint32_t>('q') ||
         key == static_cast<std::uint32_t>('Q'))) {
        return Key::Quit;
    }

    if (key >= static_cast<std::uint32_t>('A') &&
        key <= static_cast<std::uint32_t>('Z')) {
        key += static_cast<std::uint32_t>('a' - 'A');
    }
    if (key < static_cast<std::uint32_t>('a') ||
        key > static_cast<std::uint32_t>('z')) {
        return Key::None;
    }

    return kAsciiLetterKeys[static_cast<std::size_t>(
        key - static_cast<std::uint32_t>('a'))];
}

[[nodiscard]] constexpr std::pair<Rect, float> scale_text_input_geometry(
    Rect logical_area, float logical_cursor_offset, float scale_factor) noexcept {
    const float scale = scale_factor > 0.0f ? scale_factor : 1.0f;
    return {
        Rect{
            logical_area.x * scale,
            logical_area.y * scale,
            logical_area.w * scale,
            logical_area.h * scale},
        logical_cursor_offset * scale};
}

[[nodiscard]] constexpr bool text_input_boundary_needs_update(
    bool current_active,
    Rect current_physical_area,
    float current_physical_cursor_offset,
    bool requested_active,
    Rect requested_logical_area,
    float requested_logical_cursor_offset,
    float scale_factor) noexcept {
    if (current_active != requested_active) return true;
    if (!requested_active) return false;

    const auto scaled = scale_text_input_geometry(
        requested_logical_area, requested_logical_cursor_offset, scale_factor);
    const Rect requested_physical_area = scaled.first;
    const float requested_physical_cursor_offset = scaled.second;

    return current_physical_area.x != requested_physical_area.x ||
           current_physical_area.y != requested_physical_area.y ||
           current_physical_area.w != requested_physical_area.w ||
           current_physical_area.h != requested_physical_area.h ||
           current_physical_cursor_offset != requested_physical_cursor_offset;
}

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
