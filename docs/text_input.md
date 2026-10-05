# TextInput

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Enter a single UTF-8 line with a cursor, selection, clipboard support, and submission validation.
The Binding value is updated during editing; Enter is a separate submit event.

NativeUI: [widgets_text_input.inc](../include/nativeui/detail/widgets_text_input.inc), public
TextInputComponent; builder in
[widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), model in
[text_edit.hpp](../include/nativeui/text_edit.hpp). Selection, undo/redo, a codepoint limit,
horizontal scrolling, and an Escape snapshot are present.

MyGo: `ui/editor.go`, `TextInput`, `textInput`; `ui/base.go`, `TextInputBase`. The essentials already
exist. Target enhancements: core extraction, semantic publication, and safe reentrant callbacks
and observations.

## 2. Public API and composition

Preserve the current API; the following declarations are in `namespace ui`.

```cpp
using SubmitCallback = std::function<void(const std::string&)>;
TextInput(std::string label, Binding<std::string> state);
TextInput(std::string label, State<std::string>& state);
TextInput&& placeholder(std::string value) &&;
TextInput&& max_length(std::size_t value) &&;
TextInput&& on_submit(SubmitCallback callback) &&;
TextInput&& style(TextInputStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<std::string> name{"Oreto"};
auto input = ui::TextInput("Name", name).placeholder("Your name")
    .max_length(128).on_submit([](const std::string&) {}).spec();
```

The default max_length is 256 codepoints; max_length(0) means unlimited in TextEditModel. The label
is distinct from the placeholder; do not present password entry or native IME control as current
features.

Preserve the public TextInputComponent, its SubmitCallback, and TextInputStyle; extensions needed
for specialized composition must remain private to the core without changing these signatures.

## 3. State, ownership, and notifications

Binding<string> is the persistent source of truth; TextEditModel holds local text, cursor, anchor,
and history. Offsets align with UTF-8 codepoint boundaries, rather than arbitrary byte indices.

Each actual edit commits a new string; observing a different value replaces the model and resets
scroll_x and history as it does today. An identical external value does not reposition the cursor.

focus_snapshot is taken on entry; Enter updates it before submit; Escape restores this snapshot
through the Binding if the text differs. Preserve this historical behavior even if an external
value has arrived since entry, and document it clearly.

New derived components should use their own draft/conflict policy rather than silently changing
TextInput's historical restoration behavior.

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

Pointer: clicking places the caret; double-click selects a word, triple-click selects all; Shift
extends selection; dragging selects with capture and scrolls to keep the cursor visible.

Keyboard: Left/Right move by character or word depending on modifiers, Home/Up to the beginning
and End/Down to the end; Shift extends selection; Backspace/Delete remove text; normalized
Copy/Cut/Paste/SelectAll/Undo/Redo commands.

Enter calls SubmitCallback and updates the snapshot; Escape restores it. ReadOnly permits
selection, copying, and focus but blocks insert/cut/paste/undo and mutating restoration; disabled
follows availability.

TextInput converts CR/LF/Tab to single-line separators and removes non-printable controls on
insertion. The wheel does not change text; there is no automatic application drag/drop.

The core handles Composition Start/Update/Commit/Cancel and deduplicates the TextInput following a
commit. These headless handlers do not demonstrate native preedit/candidate coverage; advanced
platform delivery is separate (DESIGN §17.4).

## 5. Measurement and layout

Preferred size comes from TextInputStyle control_width/control_height, independently of text;
the label and text area retain their source partitioning. Horizontal scrolling keeps the caret
inside the content area.

Measurement and hit-testing use TextService and the same font style as painting; content clipping
bounds the selection and caret to the field.

Resize and font replacement recalculate offsets/scroll in logical units. Maximum length applies to
codepoints without cutting a UTF-8 sequence; there is no implicit auto-grow.

## 6. Presentation and invalidation

Preserve TextInputStyle with base/hovered/pressed/focused/disabled/read_only states; a distinct
placeholder, caret/selection, and composition underline are in the existing core.

Caret ticking is active only while focused and visible; loss of focus/unmounting stops text input
and composition. Color changes require only painting; dimensions/fonts require layout and then
scrolling recomputation.

External input does not write a default value or call submit. The common renderer repairs invalid
UTF-8 bytes for measurement and painting without automatically changing the Binding.

## 7. Accessibility

Target: TextInput, label as name, model text_value, read_only/enabled, and appropriate Focus/SetValue
actions. The placeholder is neither the accessible name nor the value.

The current source does not publish semantics; textual selection/caret require a separate enriched
semantic model if a bridge needs them. Do not claim a delivered native text range API.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

The source currently observes with a this capture; extraction must replace this hazardous
dependency with a detached token/runtime to tolerate removal by an earlier observer.

Current submit continues model operations after on_submit: the target copies text/callback,
finishes selection/invalidation before the call, and never reads the component afterward.

Clipboard delivery is deferred: the response retains a weak instance token and request generation.
Ignore a response after removal, an invalid State, a switch to ReadOnly, or a newer request without
inserting text.

Insert/clipboard/submit failures restore all captures and dispatch flags. Destruction cancels
composition without commit or submit and is no-throw.

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

Dependencies: TextEditModel, TextService, PlatformServices clipboard/text input, ThemeBinding, and
focus; [TextArea](text_area.md) shares the editing engine.

Empty text, externally supplied text exceeding the maximum, multibyte/malformed UTF-8, and external
replacement during selection follow the existing model; test value round-tripping rather than
inventing content validation.

No full grapheme navigation or linguistic word behavior is promised: current editing uses
codepoints and the model's word classes. The native IME limitation is explicit and does not block
committed text.

## 10. Files and compatibility

Target: `include/nativeui/text_input.hpp` and `src/text_input.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

Move the public TextInputComponent and builder to text_input.hpp/text_input.cpp, preserving
signatures, maximum defaults, and the historical style header.

The .cpp contains safe callbacks, the detached subscription, selection/clipboard/scroll/caret/paint;
the header retains only adapters needed by other builders, without a permanent behavior .inc.

Register `src/text_input.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`text_input_utf8_max`: multibyte insertion respects the codepoint limit and normalizes single-line
text without cutting bytes.

`text_input_selection_history`: clicks/drag/word/Shift, Copy/Cut/Paste, and undo/redo produce the
expected results.

`text_input_submit_escape`: Enter snapshots then submits; Escape restores the historical baseline
with an external change.

`text_input_composition`: synthetic preedit does not commit; Commit followed by identical text does
not duplicate it.

`text_input_stale_clipboard`: responses after removal/ReadOnly/an invalid model are ignored.

`text_input_remove_throw`: an observer or submit removes/throws; caret/capture/dispatch remain
reusable.

`text_input_headless_clip`: caret/selection/placeholder stay clipped across several fonts/scales.

Add `examples/features/text_input.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
