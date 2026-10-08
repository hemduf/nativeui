#pragma once

#include <nativeui/geometry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
    W,
    F2,
    F3,
    PageUp,
    PageDown,
    Menu,
    F10,
    // Appended to preserve every previously published Key value.
    // Symbols represent native keys, not committed Unicode/IME text.
    F1, F4, F5, F6, F7, F8, F9, F11, F12,
    Insert, PrintScreen, Pause, NumLock, ScrollLock, CapsLock,
    ShiftLeft, ShiftRight, ControlLeft, ControlRight,
    AltLeft, AltRight, SuperLeft, SuperRight,
    // Main keyboard digits remain distinct from keypad digits.
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    Exclamation, DoubleQuote, Hash, Dollar, Percent, Ampersand, Apostrophe, LeftParen, RightParen, Asterisk, Plus, Comma, Minus, Period, Slash, Colon, Semicolon, LessThan, Equal, GreaterThan, Question, At, LeftBracket, Backslash, RightBracket, Caret, Underscore, Grave, LeftBrace, Pipe, RightBrace, Tilde,
    Pad0, Pad1, Pad2, Pad3, Pad4, Pad5, Pad6, Pad7, Pad8, Pad9,
    PadEnter, PadPageUp, PadPageDown, PadEnd, PadHome,
    PadLeft, PadUp, PadRight, PadDown, PadClear, PadInsert, PadDelete,
    PadEqual, PadMultiply, PadAdd, PadSeparator, PadSubtract, PadDecimal, PadDivide
};

enum class Command {
    None,
    Copy,
    Cut,
    Paste,
    SelectAll,
    Undo,
    Redo,
    Submit,
    Cancel,
    FindNext,
    FindPrevious
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

using PointerId = std::uint32_t;

enum class PointerType {
    Unknown,
    Mouse,
    Touch,
    Pen,
    Eraser
};

struct PointerContact {
    PointerId id{};
    PointerType type{PointerType::Unknown};
    float pressure{std::numeric_limits<float>::quiet_NaN()};
    Size contact_size{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    bool primary{};
    bool coalesced{};
    bool predicted{};

    [[nodiscard]] constexpr bool tracked() const noexcept { return id != 0U; }
    [[nodiscard]] constexpr bool hover_capable() const noexcept {
        return type == PointerType::Mouse ||
               (type == PointerType::Unknown && !tracked());
    }
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
    // pointer press. Routing does not move keyboard focus or start a new capture;
    // an existing capture is cancelled before the request is delivered.
    ContextMenu,
    // Appended after ContextMenu so every pre-existing public enumerator keeps
    // its numeric value. Normalized zoom gesture: macOS trackpad pinch
    // (`magnifyWithEvent:`) delivers one event per continuous delta, and other
    // platforms normalize ctrl/cmd+scroll (the OS-synthesized pinch and the
    // browser ctrlKey wheel) into the same event. `magnification` carries the
    // relative factor (e.g. 0.04 = +4% scale) and position anchors zoom at
    // the pointer. Delivery mirrors PointerWheel: pointer hit target, no
    // keyboard focus move, no capture establishment, and PointerWheel never
    // carries ctrl/gui after normalization.
    // A capture request for this pointer is ignored, including a request
    // through an outer borrowed InputContext during reentrant delivery.
    Magnify
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

/// Why a pointer gesture is ending. Only Native represents a user/platform
/// cancellation that may restore an interaction's starting value. Retained
/// policy and teardown signals must stop activity without writing old values.
enum class PointerCancelReason { Native, Replaced, Unavailable, Removed, Teardown };

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
    // Appended to preserve the field order of all pre-existing aggregate
    // initializers. Legacy pointer events leave this at its id-0 default.
    PointerContact pointer{};
    PointerCancelReason cancel_reason{PointerCancelReason::Native};
    // InputType::Magnify: relative scale factor of the trackpad pinch event
    // (e.g. 0.04 = zoom in by 4% this event). Zero for every other event type.
    float magnification{};

    [[nodiscard]] bool primary_shortcut() const noexcept { return primary; }
    [[nodiscard]] bool offers_drop_type(std::string_view requested_type) const noexcept {
        for (const auto& offered : drop_types) {
            if (offered == requested_type) return true;
        }
        return false;
    }
};

[[nodiscard]] constexpr Command command_from_shortcut(const InputEvent& event) noexcept {
    if (event.type != InputType::KeyDown) return Command::None;
    if (event.key == Key::F3) return event.shift ? Command::FindPrevious : Command::FindNext;
    if (!event.primary_shortcut()) return Command::None;
    switch (event.key) {
        case Key::A: return Command::SelectAll;
        case Key::C: return Command::Copy;
        case Key::X: return Command::Cut;
        case Key::V: return Command::Paste;
        case Key::Z: return event.shift ? Command::Redo : Command::Undo;
        case Key::Y: return Command::Redo;
        case Key::G: return event.shift ? Command::FindPrevious : Command::FindNext;
        default: return Command::None;
    }
}

namespace detail {

inline constexpr std::array<Key, 26> kAsciiLetterKeys{
    Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G,
    Key::H, Key::I, Key::J, Key::K, Key::L, Key::M, Key::N,
    Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T, Key::U,
    Key::V, Key::W, Key::X, Key::Y, Key::Z};

inline constexpr std::array<Key, 10> kAsciiDigitKeys{
    Key::Digit0, Key::Digit1, Key::Digit2, Key::Digit3, Key::Digit4, Key::Digit5, Key::Digit6, Key::Digit7, Key::Digit8, Key::Digit9};

/// Map the Pugl unshifted printable-key identity to a symbolic Key. Pugl
/// uses the character corresponding to the key on the active layout; never
/// assume a US layout. Committed Unicode/IME text uses TextInput instead.
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
    if (key >= static_cast<std::uint32_t>('a') &&
        key <= static_cast<std::uint32_t>('z')) {
        return kAsciiLetterKeys[static_cast<std::size_t>(
            key - static_cast<std::uint32_t>('a'))];
    }
    if (key >= static_cast<std::uint32_t>('0') &&
        key <= static_cast<std::uint32_t>('9')) {
        return kAsciiDigitKeys[static_cast<std::size_t>(
            key - static_cast<std::uint32_t>('0'))];
    }
    switch (key) {
    case static_cast<std::uint32_t>('!'): return Key::Exclamation;
    case static_cast<std::uint32_t>('"'): return Key::DoubleQuote;
    case static_cast<std::uint32_t>('#'): return Key::Hash;
    case static_cast<std::uint32_t>('$'): return Key::Dollar;
    case static_cast<std::uint32_t>('%'): return Key::Percent;
    case static_cast<std::uint32_t>('&'): return Key::Ampersand;
    case static_cast<std::uint32_t>('\''): return Key::Apostrophe;
    case static_cast<std::uint32_t>('('): return Key::LeftParen;
    case static_cast<std::uint32_t>(')'): return Key::RightParen;
    case static_cast<std::uint32_t>('*'): return Key::Asterisk;
    case static_cast<std::uint32_t>('+'): return Key::Plus;
    case static_cast<std::uint32_t>(','): return Key::Comma;
    case static_cast<std::uint32_t>('-'): return Key::Minus;
    case static_cast<std::uint32_t>('.'): return Key::Period;
    case static_cast<std::uint32_t>('/'): return Key::Slash;
    case static_cast<std::uint32_t>(':'): return Key::Colon;
    case static_cast<std::uint32_t>(';'): return Key::Semicolon;
    case static_cast<std::uint32_t>('<'): return Key::LessThan;
    case static_cast<std::uint32_t>('='): return Key::Equal;
    case static_cast<std::uint32_t>('>'): return Key::GreaterThan;
    case static_cast<std::uint32_t>('?'): return Key::Question;
    case static_cast<std::uint32_t>('@'): return Key::At;
    case static_cast<std::uint32_t>('['): return Key::LeftBracket;
    case static_cast<std::uint32_t>('\\'): return Key::Backslash;
    case static_cast<std::uint32_t>(']'): return Key::RightBracket;
    case static_cast<std::uint32_t>('^'): return Key::Caret;
    case static_cast<std::uint32_t>('_'): return Key::Underscore;
    case static_cast<std::uint32_t>('`'): return Key::Grave;
    case static_cast<std::uint32_t>('{'): return Key::LeftBrace;
    case static_cast<std::uint32_t>('|'): return Key::Pipe;
    case static_cast<std::uint32_t>('}'): return Key::RightBrace;
    case static_cast<std::uint32_t>('~'): return Key::Tilde;
    default: return Key::None;
    }
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
