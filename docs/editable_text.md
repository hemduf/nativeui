# EditableText

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Display text, then edit it on request, such as a filename. The draft is private until acceptance;
Escape restores the display of the latest application value.

NativeUI provides Label/TextInput and retained collections, but no EditableText. Row integration
uses stable keys and explicit commands without finding a Node by label.

MyGo: `ui/editable.go`, `EditableText`, `editState`, and renameDelay=500 ms. Standalone uses
double-click/Enter; in a list, request through F2/Enter or a slow click on the selected row;
Enter/blur commit and Escape cancels.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class EditableTextController {
public:
  void begin();
  void accept();
  void cancel();
  bool editing() const noexcept;
};
class EditableText {
public:
  using Validator = std::function<std::optional<std::string>(std::string_view)>;
  EditableText(std::string label, Binding<std::string> value);
  EditableText(std::string label, State<std::string>& value);
  EditableText&& controller(std::shared_ptr<EditableTextController> value) &&;
  EditableText&& select_stem(bool value = true) &&;
  EditableText&& validator(Validator value) &&;
  EditableText&& style(EditableTextStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<std::string> file{"Preset.oreto"};
auto rename = ui::EditableText("Preset name", file).select_stem().spec();
```

The optional controller is owned through shared_ptr and bound to one mounted instance; simultaneous
reuse is rejected. Without a controller, the widget owns a private one. begin/accept/cancel are UI
requests deferred to a safe checkpoint; editing indicates the committed phase.

Defaults: select_stem=false (generic text), no validator, label used as the accessible name.
select_stem(true) selects before the last noninitial dot, as MyGo does for files.

Validator returns nullopt for valid input, otherwise an owned message. EditableTextStyle contains
reading TextStyle, editing TextInputStyle, and an invalid message. No separate rename component.

## 3. State, ownership, and notifications

Binding<string> is validated text; draft, editing, just_started, and error are local. begin snapshots
the current value and requests focus/selection; typing does not set the value.

Accept validates the draft and publishes only if it differs; Cancel discards the draft and shows
the current value without set. An external value while editing wins: replace draft/baseline and
preserve or reapply stem selection.

Invalid Enter validation retains editing/error; invalid blur cancels the draft and returns to
reading without stealing focus. Valid blur commits but does not restore editor focus; validation
is never called during destruction.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction,
Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and
observe() remains inactive. There is no implicit destruction notification: check valid at every
dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not
extend the lifetime of application models captured by a closure.

An external observation invalidates presentation without simulating a user gesture. State
notifications are synchronous: a stable snapshot, additions on the next pass, skipped removals,
and coalesced recursive writes. After an exception, the published value remains, notifications
for the rest of the pass stop, and dispatch must remain reusable.

## 4. Interactions

Standalone: double-click or Enter starts editing; a single click provides focus. During editing,
Enter accepts, Escape cancels, and valid Tab/blur accept before continuing focus navigation.

In a collection: controller begin from a command on the selected key; integration allows a slow
500 ms click on already-selected text, canceled by a second click/double-click, different selection,
scrolling, or removal.

F2 is a targeted input extension: Key does not currently contain it; append it to the enum and
translate it in the platform layer without changing existing values. The collection calls
controller begin with its own row ID.

Initially select_stem selects UTF-8 before the last “.” if it is not initial; a file without a suffix
selects all. Clipboard/selection editing follows TextInput, but the local Enter/Escape/blur policy
replaces snapshot restoration.

ReadOnly blocks begin/accept mutation while showing text; Disabled blocks focus. PointerCancel
cancels the slow trigger. The wheel does not edit and cancels the rename timer on actual parent scroll.

## 5. Measurement and layout

While reading, text size follows parent constraints; while editing, the editor visually replaces
text in the same area with compensated border/padding, without suddenly moving the row.

A long draft uses TextInput horizontal scrolling; the collection cell width takes precedence over
the new name's length. Common minimum height for both phases.

The error sits in a reserved area/description without arbitrarily enlarging every row of a table.
The edited key's logical coordinates are recalculated at the checkpoint.

## 6. Presentation and invalidation

Reading TextStyle; accent border and caret/selection while editing. The ring is visible on focused
text while reading, then on the editor while editing, never two visible focuses.

Phase changes invalidate layout if effective height differs, otherwise paint/structure; an accepted
new name remeasures text. The slow timer stops requesting frames as soon as canceled.

Validator and actions are not called during paint; the error snapshot is owned. The parent does not
remount the entire table on every draft keystroke.

## 7. Accessibility

Reading target: Text, focusable with an Activate “Edit” action when mutable; editing phase uses
TextInput and draft text_value. The name remains the label, rather than only the filename.

Announce phase/error through a backend-neutral description snapshot. The controller is not a
semantic node; no bridge retains a collection pointer.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

The controller holds a weak owner token and monotonic identity; a command after unmounting becomes
a no-op and does not keep the widget alive.

Before validator, take a draft/callback snapshot with no borrowed this surviving a reentrant call;
if the generation changes or the widget is removed, ignore the result. A validator exception retains
the editing draft with flags restored, allowing the next accept.

After successful validation, reading phase/focus policy are consistent before Binding.set; an
observer may remove the row. Destruction neither commits the draft nor replays validation.

Rejected controller enqueue retains a durable request for the owner's checkpoint without risky
synchronous execution. A controller reattached to a replacement does not resume old requests.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released
per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before
publishing a value or calling the application. A callback that has started and throws is never
replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback
must go through a deferred safe point; synchronous owner destruction safety is not promised.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a
monotonic identity; after removal they become inert, without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [Label](label.md), [TextInput](text_input.md), Focus/Dispatcher, and keyed collections
([ListView](list_view.md), [TableView](table_view.md)). Integration does not import MyGo Router.

Controller requests require the active owner's Dispatcher to execute begin/accept/cancel. Without a
Dispatcher, they remain pending and may be posted on a subsequent activation that provides this
capability; no synchronous fallback is allowed. A rejected post retains the request until the next
retained checkpoint, which retries only enqueue. Deactivation cancels accepted requests from the
old activation; they cannot execute on a new Dispatcher owner.

An invalid Binding stops all requests and remains in reading mode with the last snapshot; an empty
name is allowed if validator permits it. Initial dots in “.profile”, multiple extensions, and UTF-8
must not break stem selection.

A row removed/reordered during the timer: look up stable key and generation, cancel if different.
Double-click remains the collection's submit action when integrated: start neither rename nor two
callbacks.

## 10. Files and compatibility

Target: `include/nativeui/editable_text.hpp` and `src/editable_text.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

EditableTextController, Validator, and EditableTextStyle are described/declared in the same header;
editable_text.cpp owns phases, timer, commands, validation, and reading/editing cores.

Controller requests pass through a private backend-neutral interface; the Key F2 extension respects
the ABI of existing enumerators.

Register `src/editable_text.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`editable_text_phases`: begin creates a draft without set; valid Enter/blur commit once; Escape
preserves the current model.

`editable_text_validator`: invalid Return stays editing, invalid blur cancels; a reentrant/throwing
validator recovers.

`editable_text_stem`: suffix, no dot, initial dot, multiple dots, and UTF-8 produce exact offsets.

`editable_text_external`: an external write during editing wins and becomes the new text.

`editable_text_row_delay`: controlled 500 ms; double-click/reorder/removal/scroll cancel stale rename.

`editable_text_controller`: stale command is a no-op and rejected enqueue does not execute
synchronously; one controller per instance.

`editable_text_destroy_draft`: unmount/destruction produce no commit/validation; two isolated instances.

Add `examples/features/editable_text.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
