# TextArea

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Enter multiple lines and keep the cursor and selection visible in a local viewport. TextArea is a
plain text editor, rather than RichText or FindBar.

NativeUI: [widgets_text_area.inc](../include/nativeui/detail/widgets_text_area.inc), public
TextAreaComponent; builder in
[widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), multiline TextEditModel,
and TextAreaStyle.

MyGo: `ui/editor.go`, `TextArea`, `textInput`; `ui/base.go`, `TextAreaBase`. Multiline editing has
already been ported; enhancements are required for a separate core, semantics, and robust callback
lifetimes.

## 2. Public API and composition

Preserve the current API; the following declarations are in `namespace ui`.

```cpp
TextArea(std::string label, Binding<std::string> state);
TextArea(std::string label, State<std::string>& state);
TextArea&& placeholder(std::string value) &&;
TextArea&& max_length(std::size_t value) &&;
TextArea&& style(TextAreaStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<std::string> notes{"First line\nSecond line"};
auto editor = ui::TextArea("Notes", notes).placeholder("Write notes")
    .max_length(4096).spec();
```

The default max_length is 0, meaning unlimited; unlike TextInput, there is currently no public
on_submit. Enter inserts a new line without an implicit validation callback.

Preserve the public TextAreaComponent and existing TextAreaStyle. Do not add automatic wrapping,
Markdown formatting, or syntax highlighting under the name of extraction.

## 3. State, ownership, and notifications

Binding<string> contains plain text; the local model holds caret/anchor/history, a line cache, and
scroll_x/y. A user change commits a complete string live.

A different external observation replaces the model, runs rebuild_lines, and resets scrolling and
history as in the current source. An identical value does not disrupt the caret.

focus_snapshot is set on entry; Escape can restore this snapshot with a Binding write; Enter does
not validate submission. Preserve this historical policy, including after external replacement.

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

Click/double-click/triple-click and selection dragging follow multiline text. Up/Down retain the
preferred column; Left/Right move by character/word, Home/End to line start/end, and the primary
modifier to document start/end.

Enter inserts LF; insertion/paste normalize CR or CRLF to LF, preserve LF/Tab, and remove other
controls. Copy/Cut/Paste/SelectAll/Undo/Redo are shared.

Shift extends selection across lines; Backspace/Delete cross separators. Escape restores the
snapshot. ReadOnly retains selection/copying and navigation while blocking modifications.

The current source ignores PointerWheel. Target enhancement: the wheel scrolls the local viewport
vertically, or horizontally with Shift, without changing Binding/caret; at a boundary with no
movement the event bubbles to the parent. This extension is documented separately from extraction.

Headless Composition Start/Update/Commit/Cancel and commit deduplication are present; native
preedit/candidate integration remains a platform contract to verify separately under DESIGN §17.4.

## 5. Measurement and layout

Fixed preferred size comes from TextAreaStyle control_width/control_height. Line-aware layout and
local viewport clipping; long lines scroll horizontally without implicit soft wrapping.

rebuild_lines produces consistent lines and offsets after CRLF normalization; keep the caret
visible after vertical movement, selection, and resize.

Text, cross-line selection, caret, and placeholder use the same TextService font in logical
coordinates; malformed measurement/rendering use the common repair path.

## 6. Presentation and invalidation

TextAreaStyle defines fonts, fill/border, selection/caret, and composition underline. Shared
background and ring, with a single focus target for the viewport.

Text changes or font metrics invalidate the line cache; value/caret/selection require painting,
while dimensions/style metrics require layout.

Caret ticking occurs only while focused/visible; no loop runs while unfocused. A paint error keeps
the clip stack balanced and leaves the last committed frame intact.

## 7. Accessibility

Target: TextArea, name=label, plain text text_value, read_only/enabled, and Focus/SetValue if allowed.
The line structure does not become N accessible text fields.

The studied core currently has no semantics override; a selection model and native text range APIs
are not claimed as implemented.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

The current subscription captures this; extraction uses a detached runtime/token and keeps the
observation inert after removal during a pass.

Prepare the line cache and text before consistent publication. Allocation/rebuild failure must not
publish a caret pointing into an old buffer with a new string.

Deferred clipboard delivery carries a generation/weak token; ignore the response after removal,
a switch to ReadOnly, or an invalid Binding. Unmounting ends dragging, composition, and text input
without a destructive commit.

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

Dependencies: [TextInput](text_input.md), TextEditModel, TextService, clipboard, ThemeBinding, and
focus. RichText/FindBar are separate components.

Empty text, a trailing empty line, a very long line, many lines, emojis/malformed text, reaching the
maximum on multiline paste, and external replacement during dragging must remain deterministic.

No filesystem I/O, system document editing, or audio callbacks. The application computes search
results and handles saving.

## 10. Files and compatibility

Target: `include/nativeui/text_area.hpp` and `src/text_area.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

TextArea and TextAreaComponent declarations remain public in text_area.hpp; text_area.cpp contains
the retained model, line cache, scrolling, handlers, and painting.

Preserve text_area_style.hpp and aggregate includes; shared editing helpers remain private or in
the current text_edit.hpp without duplicating the engine.

Register `src/text_area.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`text_area_line_normalize`: CR/CRLF become LF; Tab is preserved; Enter inserts LF without submit.

`text_area_vertical_selection`: Up/Down retain the column, Home/End navigate line/document, and
Shift selects across lines.

`text_area_history_escape`: undo/redo and the focus snapshot restore the expected text.

`text_area_viewport`: caret visibility with long lines/resize; balanced, clipped selection painting.

`text_area_composition_clipboard`: synthetic composition without duplicate commit; stale clipboard
responses rejected.

`text_area_replace_throw`: failures in an observer, rebuild, or paint recover without partial
cache/text.

`text_area_public_component`: old signatures and direct TextAreaComponent use still compile.

Add `examples/features/text_area.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
