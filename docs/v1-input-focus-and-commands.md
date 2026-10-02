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
- the Binding overload stores an owned Binding handle; the State overload converts to Binding immediately and therefore does **not** retain a raw `State<bool>*`;
- destroying the originating State leaves the retained Binding at its documented last value and stops future source notifications rather than dangling the scope;
- while mounted, active-state notifications synchronously invalidate focus structure and retained presentation on the UI thread.

The builder owns exactly one child Spec. `spec() &&` consumes that builder and transfers the child, Binding and policy into declarative retained state; it performs no focus transition by itself. Tree materialization/mounting may allocate or throw through ordinary component lifecycle machinery.

By default `trap(true)` prevents Tab/Shift+Tab traversal from escaping once focus is inside an active scope. The trap flag does not make an inactive scope eligible and does not change pointer/layout visibility.

`default_focus(index)` chooses a **zero-based available focusable-descendant index** when the scope activates. It is not a direct-child index. If that index is unavailable/out of range, NativeUI falls back to the first available descendant. When the scope deactivates, NativeUI restores the previously focused eligible node when possible, otherwise normal retained fallback rules apply.

`FocusScopeComponent` is a transparent one-child layout wrapper: preferred/minimum size comes from the first child and layout grants that child the complete logical bounds. It paints no pixels. These methods and focus callbacks belong to the UI/main-thread domain and are not audio/DSP real-time operations.

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

A `CommandScope` does not take focus and does not intercept raw pointer/key/text events. It owns its `std::function` callback and one child Spec. The callback receives `Command` by value, so there is no argument lifetime to extend; captured objects follow normal `std::function` ownership rules.

Invocation is synchronous on the owning UI thread. Returning `Ignored` is intentional when a parent/global handler should remain eligible. An empty callback also behaves as `Ignored`. Exceptions are not translated into an EventResult by CommandScope; they propagate through the Tree/UI dispatch boundary after its bookkeeping rules are applied. Callback-triggered State changes or retained structural work may therefore re-enter UI logic and are reconciled at the same safe checkpoints as other dispatch callbacks. Command callbacks are not audio/DSP real-time entry points.

Like FocusScope, `CommandScopeComponent` is a transparent one-child layout wrapper: it forwards preferred/minimum size, grants the child the full logical bounds and paints no pixels. Construction/spec creation does not invoke the callback; the callback is transferred into the retained component when the Spec materializes.

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

[`t017_commands.cpp`](../examples/features/t017_commands.cpp) demonstrates TextInput owning Copy/Paste, a scoped Undo handler and global fallback.

## Generic edit sessions

[`edit.hpp`](../include/nativeui/edit.hpp) defines `EditSession<T>`, a UI-thread edit-lifetime helper around an ordinary `Binding<T>`. It gives retained controls explicit begin/change/end-or-cancel semantics without turning NativeUI into a plug-in automation layer: `EditSource` is callback metadata only and does not imply host automation, normalization, or audio-thread transport.

### Ownership and callback order

The session owns its Binding handle and `EditCallbacks<T>` by value. Callback captures follow normal C++ lifetime rules; objects captured by reference/pointer are not kept alive by NativeUI. The originating `State<T>` remains the logical value owner, and direct State/Binding writes do not synthesize edit callbacks.

A successful edit has this ordering:

```text
begin
  -> zero or more [State observers -> change]
  -> exactly one end | cancel
```

The source passed to `begin(source)` remains fixed until the terminal callback. `change` receives a borrowed `const T&` valid only for that callback. State observers run first, so change sees the final committed value after synchronous observer-side normalization. If that final value compares equal to the value present before the update, change is suppressed.

Cancellation is a lifetime/interaction result, not rollback: the latest committed State value remains in place. If the originating State expires, later mutation is rejected and an active edit becomes cancellation when a session operation observes the invalid Binding. `active()` only reads the session's local flag and deliberately does not trigger this validation/cancellation itself.

### Reentrancy, failures and return values

While begin/change dispatch is active, `begin()`, `update()`, `set()` and reentrant `finish()` do not create a nested transaction. Reentrant `end()`/`cancel()` are deferred until the callback returns; cancellation wins over a competing deferred end.

`begin()` can therefore return false even after its begin callback ran: if that callback requested end/cancel, the deferred terminal notification has already made the session inactive. `update(value)` returns true only when the final committed State value differs from the value seen before the update.

`finish(value)` performs the update and then requests end while retaining internal control state, so removing the owning subtree from an observer/callback does not invalidate the in-flight method. An equal final value still ends an active session and returns false. A reentrant finish returns false immediately and leaves the current transaction unchanged.

`set(value, source)` is the discrete one-shot form. It runs begin -> update -> end only for a live, valid, inactive, non-reentrant session whose value differs from the current State. Equal/rejected calls emit no edit callbacks and do not interrupt an existing edit.

Exceptions from begin/change/State observers cancel the edit once and then rethrow the original exception. Explicit terminal callback exceptions propagate after the session is already inactive and are never retried. Destructor-triggered cancellation suppresses terminal exceptions to preserve the noexcept destructor.

```cpp
ui::State<float> gain{0.5f};

ui::EditSession<float> edit{
    gain.binding(),
    {
        .begin = [](ui::EditSource source) { begin_ui_edit(source); },
        .change = [](const float& value, ui::EditSource) { preview_gain(value); },
        .end = [](ui::EditSource) { commit_ui_edit(); },
        .cancel = [](ui::EditSource) { abandon_ui_edit(); },
    }};

if (edit.begin(ui::EditSource::Pointer)) {
    edit.update(0.65f);
    edit.finish(0.7f);
}
```

The helper may allocate and invoke arbitrary application callbacks. It belongs to the owning UI/main-thread domain and is not an audio/DSP real-time primitive or cross-thread synchronization object.

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

### State inspection and terminal edge cases

`active()`, `dragging()`, `phase()`, `origin()`, `current()` and `threshold()` are allocation-free value queries. Origin/current retain the last stored sample after end/cancel; phase/active are the liveness indicators. Calling `begin()` while already active restarts at the new origin and does not synthesize an end/cancel for the abandoned gesture.

Idle `move()`, `end()` and `cancel()` return a zero/default `DragUpdate`. `end(position)` samples the final position with the same threshold logic as `move()` before click-versus-drag classification. With a zero threshold, that final sample crosses the threshold even with zero displacement, so it terminates as a drag rather than a click. `cancel()` does not sample a new position: returned delta is zero and total reflects the last current() sample.

The gesture owns only value state, invokes no callbacks, performs no allocation or retained-tree/platform work, and has no internal synchronization. NativeUI controls normally mutate one instance from their owning UI thread. Pointer capture remains a separate `InputContext` responsibility.

### Mapping drag distance to values

`drag_axis_delta()` returns exactly the x or y component of the supplied `Point`. `drag_value_delta()` multiplies that component by a caller-provided application-value-units-per-coordinate-unit scale (normally value per logical pixel) and optionally negates the result.

Neither helper clamps to an application range, validates finite inputs, retains state, or invokes callbacks. The control/application remains responsible for its final value domain.

## TextEditModel

[`text_edit.hpp`](../include/nativeui/text_edit.hpp) provides NativeUI's platform-independent UTF-8 boundary/navigation helpers plus the owned mutable `TextEditModel`.

### Storage, units and ownership

The model owns its base text, selection endpoints, active IME/pre-edit bytes and bounded undo/redo snapshots. It does not retain caller string views, platform editor objects, clipboard handles, painters or retained components.

Cursor, anchor, selection and composition positions are **byte offsets**. Helpers in `ui::text` recognize UTF-8 continuation-byte boundaries, but deliberately do not perform full Unicode validation, normalization or grapheme-cluster segmentation. `TextMotion::Codepoint` is therefore one continuation-boundary-delimited unit, not one user-perceived grapheme. `Word` uses the coarse `CharClass` policy; `Document` targets the complete buffer boundary.

Borrowed and owned results have different lifetimes:

- `text()` and `composition_text()` return references to model-owned strings;
- `selected_text()` returns a view into `text()`;
- relevant mutation, assignment or model destruction can invalidate those borrows;
- `snapshot()` returns an owned independent text/cursor/anchor copy and intentionally excludes transient composition plus undo/redo history.

A non-zero max length applies to **future insertion/composition commits in code-point-like units**; zero means unlimited. Construction and `set_text()` do not truncate a complete supplied string, so owned text may already exceed the insertion budget.

### Mutation, failures and history

`TextEditModel` invokes no application callback and has no callback-driven reentrancy path. Noexcept movement/selection APIs only update already-owned scalar/index state. Operations that copy/truncate/grow strings or push snapshots may allocate and propagate ordinary C++ exceptions. Boolean editing results describe whether the requested text mutation was accepted; they are not a generic error channel and do not translate allocation failures.

Undo/redo retains at most 64 owned snapshots per direction. A successful normal edit checkpoints the prior text/cursor/anchor state and clears redo history.

| API | Contract |
| --- | --- |
| `insert(view)` | borrows input for the call, cancels active composition before a non-empty normal insertion, truncates to remaining max-length budget and checkpoints accepted mutation |
| `erase_selection()` | false for empty selection; otherwise cancels composition and checkpoints before deletion |
| `backspace()/delete_forward()` | cancel composition, then delete selection or Codepoint/Word/Document range |
| `undo()/redo()` | cancel composition first; false only when the respective history is empty; successful transfer copies an owned snapshot to the opposite history |
| `set_text(value,...)` | consumes complete replacement text without enforcing max length; optionally preserves/clamps byte endpoints and clears history |

A false edit result does not necessarily mean every transient field stayed unchanged. For example, composition cancellation/restoration can occur before a later insertion/deletion rejection. Allocation failures are exceptions rather than false and mutating calls are not documented as a transactional rollback boundary for every cursor/composition/history side effect.

```cpp
ui::TextEditModel edit{"one two", 32};
edit.move_left(ui::TextMotion::Word);
edit.select_word_at(edit.cursor());

const std::string_view borrowed = edit.selected_text();
(void)edit.insert("three");
// borrowed must not be used after the mutation.

auto checkpoint = edit.snapshot(); // owned copy; may outlive edit
(void)edit.undo();
```

### IME/pre-edit transaction

`begin_composition()` snapshots current text/cursor/anchor and starts an empty pre-edit transaction. `update_composition(preedit, cursor_byte, selection_bytes)` consumes the new pre-edit string, clamps the cursor to a recognized byte boundary, bounds the selection to the remaining pre-edit bytes and leaves base text/history unchanged. Calling it while inactive starts composition automatically.

`commit_composition(committed)` borrows committed bytes only for the call. For an active attempt, NativeUI ends transient composition state, verifies that base text still matches the composition-start snapshot and truncates the accepted prefix to the remaining max-length budget without splitting a recognized boundary. It returns true only when non-empty bytes are inserted. Inactive/stale composition, exhausted budget or an accepted empty prefix returns false; allocation/history/string failures propagate instead.

`set_text()` does not explicitly cancel active composition. Replacing base text makes the stored composition-start snapshot stale, so a later `commit_composition()` rejects the old range instead of applying it to the new text.

`cancel_composition()` is noexcept. It clears pre-edit storage and, when base text still matches the starting snapshot, restores the captured cursor/anchor. Normal destructive editing operations cancel active composition before mutating base text.

[`t025_text_edit_model.cpp`](../examples/features/t025_text_edit_model.cpp) is the focused model reference; [`t029_ime_composition.cpp`](../examples/features/t029_ime_composition.cpp) exercises retained IME integration.

## Threading and lifetime

Retained input/focus mutation is UI/main-thread work. No process-global focus, pointer-capture or command target exists: ownership belongs to the concrete retained tree/UI instance.

State/Binding objects driving a `FocusScope` follow the lifetime/reentrancy rules in [State and binding](v1-state-and-binding.md). Removing, hiding, collapsing or disabling focused/captured content is reconciled by the retained tree rather than leaving stale callback targets.

For related behavior see:

- [Composition, layout and widgets](v1-composition-layout-and-widgets.md);
- [`t015_pointer_capture.cpp`](../examples/features/t015_pointer_capture.cpp);
- [`t016_gestures.cpp`](../examples/features/t016_gestures.cpp);
- [`t029_ime_composition.cpp`](../examples/features/t029_ime_composition.cpp).
