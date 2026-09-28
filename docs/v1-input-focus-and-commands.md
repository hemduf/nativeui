# NativeUI 1.0 input, focus and commands

This chapter documents NativeUI's public retained interaction surface: normalized input events, pointer metadata/capture, keyboard focus scopes, IME composition and portable editing commands. The primary public headers are [`input.hpp`](../include/nativeui/input.hpp), [`component_base.hpp`](../include/nativeui/component_base.hpp), [`focus.hpp`](../include/nativeui/focus.hpp) and [`command.hpp`](../include/nativeui/command.hpp).

## Event model

NativeUI converts platform events into value-semantic `ui::InputEvent` objects before components see them. Components do not need AppKit, Win32, X11 or Pugl event types.

| API | Contract |
| --- | --- |
| `Key` | Platform-neutral non-text key identity for navigation, shortcuts and widget control. Text entry is separate. |
| `Command` | Semantic editing action: Copy, Cut, Paste, SelectAll, Undo or Redo. |
| `CompositionEvent` | IME composition lifecycle and current text/cursor/selection byte ranges. |
| `PointerContact` | Pointer ID/device metadata, optional pressure/contact footprint and coalesced/predicted flags. |
| `InputType` | Normalized event category for keys, commands, text, composition, pointers, drops, resize and quit/context-menu requests. |
| `EventResult` | `Handled` consumes a routed event; `Ignored` allows bubbling/fallback. |
| `InputEvent` | Owned value payload containing the fields relevant to its `type`. |

### Keys are not text

`KeyDown` and `KeyUp` represent logical control keys. They are appropriate for Tab, arrows, activation keys and shortcut detection, but they are not a character-input protocol.

Printable text arrives through:

- `InputType::TextInput` for committed text;
- `InputType::Composition` plus `CompositionEvent` for IME/pre-edit lifecycle.

Applications must not derive characters by arithmetic on `Key` values. Letter enumerators intentionally preserve historical numeric values and are not contiguous.

### Modifiers and the portable primary shortcut

`InputEvent` exposes `shift`, `ctrl`, `alt`, `gui` and the normalized `primary` modifier. `primary` means Command on macOS and Control on Windows/Linux.

`command_from_shortcut()` maps these KeyDown combinations:

| Shortcut | Command |
| --- | --- |
| Primary+A | `SelectAll` |
| Primary+C | `Copy` |
| Primary+X | `Cut` |
| Primary+V | `Paste` |
| Primary+Z | `Undo` |
| Shift+Primary+Z | `Redo` |
| Primary+Y | `Redo` |

Unrelated keys, non-KeyDown events and events without `primary` return `Command::None`.

## IME composition

`CompositionEvent::type` is one of `Start`, `Update`, `Commit` or `Cancel`. The accompanying `text` is owned by the event value.

`cursor_byte` and `selection_bytes` are byte-based positions/counts into the composition string. They are not code-point or grapheme indices. Editing controls must keep those units distinct from higher-level text-navigation units.

The platform text-input boundary is exposed to focused components through `FocusContext::set_text_input()` and to active input handlers through `InputContext::set_text_input()`. The editor supplies logical geometry; platform integration performs the physical-pixel conversion.

## Pointer contacts and capture

A `PointerContact` carries:

- `id`: `0` for the legacy/untracked pointer identity, non-zero for a tracked contact;
- `type`: Unknown, Mouse, Touch, Pen or Eraser;
- optional `pressure` and `contact_size`; unavailable values are represented by NaN;
- `primary`: whether this is the primary contact;
- `coalesced` and `predicted`: metadata for high-frequency native samples.

`tracked()` is true for non-zero IDs. `hover_capable()` is true for a mouse and for the legacy untracked/unknown pointer.

Pointer capture is requested from the borrowed `InputContext`:

```cpp
ui::EventResult MyComponent::input(
    const ui::InputEvent& event,
    ui::InputContext& context)
{
    if (event.type == ui::InputType::PointerDown) {
        context.capture_pointer();
        return ui::EventResult::Handled;
    }

    if (event.type == ui::InputType::PointerUp) {
        context.release_pointer();
        return ui::EventResult::Handled;
    }

    return ui::EventResult::Ignored;
}
```

Capture applies to the pointer associated with the current callback. NativeUI guards capture generations so a stale outer callback cannot overwrite or release capture created by a newer re-entrant event for the same pointer ID. Capture cannot be newly established from terminal PointerUp, PointerCancel or ContextMenu callbacks.

## Routing and bubbling

NativeUI resolves one authoritative leaf target, then bubbles ignored input through retained ancestors:

1. pointer input selects a hit-tested or captured target;
2. keyboard/text input uses the focused retained target;
3. the target's `Component::input()` runs first;
4. `EventResult::Ignored` continues to the parent;
5. `Handled` stops the route.

Effectively hidden/collapsed/disabled content is excluded from normal interactive targeting by the retained availability model.

[`t013_bubbling.cpp`](../examples/features/t013_bubbling.cpp) is the focused example for leaf-to-parent routing.

## Borrowed callback contexts

`InputContext`, `CanvasInputContext` and `FocusContext` are callback-duration borrows. Do not store them, capture them into delayed work or keep references to them after the callback returns.

### InputContext

`InputContext` provides:

- receiving-component logical bounds;
- text measurement;
- clipboard set/request operations;
- drag/drop accept/reject;
- platform text-input activation/geometry;
- paint-only invalidation;
- layout+paint invalidation;
- capture/release for the current pointer contact.

`CanvasInputContext` is a convenience facade for Canvas handlers. It exposes `size()` and forwards the same interaction services.

### FocusContext

`FocusContext` is delivered to `Component::focus_changed()`. It provides logical bounds, text measurement, text-input activation and paint/layout invalidation. It is also borrowed only for the active callback.

## Focus scopes

[`FocusScope`](../include/nativeui/focus.hpp) creates a focus-only boundary around one child subtree.

Important distinctions:

- it does **not** hide, disable, collapse, mount or unmount its child;
- `active=false` only removes descendants from focus targeting;
- an active scope observes its `Binding<bool>` while mounted;
- focus structure is reconciled when that binding changes.

By default `trap(true)` prevents Tab/Shift+Tab traversal from escaping once focus is inside an active scope.

`default_focus(index)` chooses a **zero-based available focusable-descendant index** when the scope activates. If that index is unavailable/out of range, NativeUI falls back to the first available descendant. When the scope deactivates, NativeUI restores the previously focused eligible node when possible, otherwise normal retained fallback rules apply.

Example:

```cpp
ui::State<bool> dialog_active{true};

auto content = ui::FocusScope{
    dialog_active,
    ui::Column{
        ui::Button{"Cancel", [] {}},
        ui::Button{"Confirm", [] {}}
    }
}
.trap(true)
.default_focus(1);
```

[`t014_focus_scopes.cpp`](../examples/features/t014_focus_scopes.cpp) demonstrates default focus and trapped forward/reverse traversal.

## Portable command routing

Commands separate semantic editing intent from the native key combination.

When NativeUI sees a supported primary-modifier shortcut, it converts the key event to `InputType::Command`, then:

1. routes the command from the focused leaf through retained ancestors;
2. gives focused widgets first opportunity to handle their own editing command;
3. lets [`CommandScope`](../include/nativeui/command.hpp) handlers on the ancestor route handle or ignore it;
4. if still ignored, invokes the tree/UI global command handler.

A `CommandScope` does not take focus and does not intercept raw pointer/key/text events:

```cpp
auto scoped = ui::CommandScope{
    [](ui::Command command) {
        if (command == ui::Command::Undo) {
            // Apply scoped undo.
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    },
    child
};
```

Returning `Ignored` is intentional when a parent/global handler should remain eligible. [`t017_commands.cpp`](../examples/features/t017_commands.cpp) demonstrates TextInput owning Copy/Paste, a scoped Undo handler and global fallback.

## Reusable click/drag gestures

[`gesture.hpp`](../include/nativeui/gesture.hpp) provides `DragGesture`, a small synchronous value-only state machine for controls that need click-versus-drag classification without owning another event subsystem.

The helper allocates no heap memory and owns no platform or retained-tree object. The caller is responsible for starting it from pointer input, forwarding moves, ending or cancelling it, and requesting pointer capture when the interaction must continue outside the original hit-test bounds.

### Threshold semantics

The constructor threshold is expressed in the same logical coordinate units as the `Point` values supplied to `begin()`, `move()` and `end()`. In ordinary NativeUI input code that means logical pixels.

A gesture transitions:

```text
Idle --begin--> Pressed --distance >= threshold--> Dragging
  ^                    \--end before threshold--> click
  |                                         |
  +-------------------- end/cancel ---------+
```

The threshold check uses Euclidean distance from the original press point, not accumulated per-event distance.

`DragUpdate` reports two displacement forms:

- `delta`: movement since the previous sample;
- `total`: movement from the original press point.

`drag_started` is true only on the update that crosses the threshold. `clicked` appears only on `end()` if the gesture remained Pressed. `cancel()` returns the final total displacement and marks the update cancelled without producing a click.

### Typical retained input use

```cpp
ui::DragGesture drag{5.0f};

ui::EventResult input(
    const ui::InputEvent& event,
    ui::InputContext& context)
{
    switch (event.type) {
    case ui::InputType::PointerDown:
        drag.begin(event.position);
        context.capture_pointer();
        return ui::EventResult::Handled;

    case ui::InputType::PointerMove:
        if (drag.active()) {
            const auto update = drag.move(event.position);
            if (update.dragging) {
                // Apply update.total or update.delta.
            }
            return ui::EventResult::Handled;
        }
        break;

    case ui::InputType::PointerUp:
        if (drag.active()) {
            const auto update = drag.end(event.position);
            context.release_pointer();
            if (update.clicked) {
                // Handle click.
            }
            return ui::EventResult::Handled;
        }
        break;

    case ui::InputType::PointerCancel:
        if (drag.active()) {
            (void)drag.cancel();
            return ui::EventResult::Handled;
        }
        break;

    default:
        break;
    }

    return ui::EventResult::Ignored;
}
```

[`t016_gestures.cpp`](../examples/features/t016_gestures.cpp) demonstrates this pattern with pointer capture and a horizontal drag value.

### Mapping drag distance to values

`drag_axis_delta()` projects a `Point` onto Horizontal or Vertical. `drag_value_delta()` additionally multiplies by a caller-provided value-per-logical-pixel scale and optionally inverts the sign.

These helpers deliberately do not clamp application values. The control/application retains responsibility for its value domain.

## Threading and lifetime

Retained input/focus mutation is UI/main-thread work. No process-global focus, pointer-capture or command target exists: ownership belongs to the concrete retained tree/UI instance.

State/Binding objects driving a `FocusScope` follow the lifetime/reentrancy rules in [State and binding](v1-state-and-binding.md). Removing, hiding, collapsing or disabling focused/captured content is reconciled by the retained tree rather than leaving stale callback targets.

For related behavior see:

- [Composition, layout and widgets](v1-composition-layout-and-widgets.md);
- [`t015_pointer_capture.cpp`](../examples/features/t015_pointer_capture.cpp);
- [`t016_gestures.cpp`](../examples/features/t016_gestures.cpp);
- [`t029_ime_composition.cpp`](../examples/features/t029_ime_composition.cpp).
