# Value editing and embedded visibility

`<nativeui/edit.hpp>` adds UI-thread edit lifetimes independently of any domain.
`EditSession<T>` accepts a `Binding<T>` and optional `EditCallbacks<T>`:

```cpp
ui::State<double> value{0.5};
ui::EditSession<double> edit{value.binding(), {
    .begin = [](ui::EditSource source) { /* interaction started */ },
    .change = [](const double& value, ui::EditSource source) { /* committed */ },
    .end = [](ui::EditSource source) { /* completed */ },
    .cancel = [](ui::EditSource source) { /* interrupted */ }
}};
edit.begin(ui::EditSource::Pointer);
edit.update(0.75);
edit.end();
```

The source is `Pointer`, `Keyboard`, `Wheel`, or `Accessibility`. The last value
is reserved for semantic action adapters; this change does not implement native
accessibility, VoiceOver, or any OS accessibility actions.

- `begin(source)` returns whether a session became active. Repeated begin is
  ignored. A pointer click without movement may therefore emit begin/end.
- `update(value)` returns whether the effective committed value changed. State
  observers run synchronously before `change(value, source)`, which receives a
  borrowed final value valid only during the callback. Unchanged values emit no
  change. Recursive State writes from its observers are coalesced by State.
- `end()` and `cancel()` close an active session once. Cancel keeps the latest
  value; the application decides whether and how to restore an earlier value.
- `set(value, source)` is an atomic begin/update/end command. Equal values emit
  nothing. `finish(value)` updates and ends a pointer interaction.
- `active()` reports the session state. The session is neither copyable nor
  movable. Its destructor cancels without throwing.
- Direct State/Binding writes remain model synchronization and emit no edit
  callbacks. Sessions sharing one State remain independent.

Reentrant begin/update/set/finish calls during a callback are rejected. Beginning
or updating an edit from inside an already-running notification of its State is
also rejected: State defers such writes, so an edit cannot truthfully report a
synchronous committed value there. Schedule such edits after the notification.
Reentrant end/cancel requests are deferred until the callback returns; cancel
wins. A throwing begin/change/State observer cancels once, restores session
bookkeeping, then rethrows the original error. A throwing terminal callback is
not repeated; destructor cleanup swallows exceptions. Losing the State owner
cancels at the next operation. Destroying the EditSession from its callback is
safe. As elsewhere in NativeUI, destroy a top-level UI/native owner only at its
safe deferred checkpoint, never synchronously inside a native callback.

`Knob`, `Slider`, and `Toggle` expose `.on_edit(EditCallbacks<float/bool>)`.
Callback failures clear widget gesture state and pointer capture before propagating
the original exception, so the next interaction can start normally.
Knob/Slider bracket each pointer drag, including the final release outside their
bounds. Escape, pointer cancellation, focus loss, deactivation, disabled/hidden
availability, and unmount interrupt active drags. Keyboard commands (including
repeats) are atomic; Toggle preserves its one-toggle-per-key-press policy.
Knob/Slider optionally enable atomic wheel commands with `.wheel_enabled()`;
wheel behavior is disabled by default, and clamped no-ops emit no callbacks.
Toggle activation is atomic and retains its existing pointer-down behavior.
Retained availability/reconciliation is applied at the existing UI checkpoint;
a complete atomic command can finish before its observer's deferred availability
change is applied. Lifecycle cancellation applies when an edit is still active.

Custom Components can reuse `EditSession` for their own drawing and interaction
rules. NativeUI has no parameter IDs, normalization, automation, host event queues,
or audio-thread access in this API. Adapters own those policies.

## Hidden embedded construction

Existing constructors remain initially visible. Hosts with separate create/show
phases opt into hidden realization:

```cpp
ui::EmbeddedView view{tree, parent, {640, 420}, {},
    ui::EmbeddedViewOptions{.initially_visible = false}};
view.show();
view.hide();
bool shown = view.visible();
```

All calls, including construction and destruction, run on the platform/UI thread.
`show()`/`hide()` are idempotent and return false after native close. `visible()`
reports the requested local visibility, not occlusion, minimized host windows,
or hidden ancestors. Hidden views retain resources and permit polling, State
updates and size grants. Showing invalidates the retained scene for a fresh
frame. Hiding cancels pointer edits, deactivates focus and stops IME; cancellation
callback errors propagate only after cleanup. `request_close()` stays terminal.
The view borrows the UI; destroy the view before its UI and State owners.

On macOS realization and passive show do not steal the host's first responder.
Show/hide affect the embedded NSView, never the host NSWindow or sibling views.
An explicit click acquires the embedded responder and activates input before
delivering that press. Native responder and key-window changes activate or
deactivate the retained UI; losing focus cancels an active gesture. Hiding the
focused child clears its native responder, and showing it again remains passive.
Window notifications are scoped to that child's current host window and removed
on detach/destruction. NativeUI does not replace the host window delegate.
The pinned Pugl backend lacks that contract; `NativeUIPuglVisibility.cmake`
applies checked source substitutions to a build-private Pugl staging tree.
The CPM cache/source override is unchanged, and installed packages include the
corrected sources. A mismatched Pugl source fails configuration for review instead
of silently applying an uncertain patch. Remove the shim after upstream Pugl
provides the same behavior. Windows/X11/WebAssembly continue to use their
existing Pugl show/hide implementations; this change is validated on macOS.

Public examples: `examples/features/value_edit_sessions.cpp` and
`examples/features/embedded_visibility.cpp`, both with displayless `--self-test`.
The macOS integration test checks NSView visibility, host focus, and real AppKit
mouse/keyboard delivery through `NSWindow::sendEvent`, without manually activating
the retained UI.
