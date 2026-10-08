# SearchField

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A search field with a magnifier, text, and clearing, without a built-in search engine. The Binding
carries the query for application filtering.

NativeUI provides TextInput and icon primitives, but no SearchField. The magnifier and clear button
remain subparts of the same component.

MyGo: `ui/combobox.go`, `SearchField`, `magnifier`. Clearing by button or Escape, Enter Submitted,
the “Search” placeholder, and retaining focus are ported into the retained API.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class SearchField {
public:
  SearchField(std::string label, Binding<std::string> query);
  SearchField(std::string label, State<std::string>& query);
  SearchField&& placeholder(std::string value) &&;
  SearchField&& max_length(std::size_t value) &&;
  SearchField&& on_submit(std::function<void(const std::string&)> callback) &&;
  SearchField&& style(SearchFieldStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<std::string> query{""};
auto search = ui::SearchField("Search presets", query)
    .placeholder("Search").on_submit([](const std::string&) {}).spec();
```

Defaults: placeholder “Search”, max_length=0 meaning unlimited. SearchFieldStyle: TextInputStyle,
magnifier size/gap, clear ButtonStyle, and internal metrics; no new Theme slot is assumed.

Query changes are Binding notifications; on_submit receives a copy of the query on Enter. No hidden
debounce, network request, or application scheduler.

## 3. State, ownership, and notifications

Binding<string> is the live query; caret/selection/undo are local to the TextInput core. An external
write replaces text under the safe editor contract without triggering submit.

Clear sets query to empty once and resets the cleared query's caret/history; it publishes neither
search results nor filter rollback.

TextInput's Escape baseline is replaced by the SearchField policy: Escape on a nonempty query
clears it instead of restoring a historical snapshot. This replacement is limited to the new
component.

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

Clicking the field provides caret/focus; editing and clipboard follow TextInput. The clear button
appears for a nonempty query; clicking triggers it and keeps focus in the editor.

Enter submits the current query unchanged; Escape clears a nonempty query; Escape on an empty query
bubbles to the parent Dialog/FindBar. During composition, the first composition cancellation does
not clear the query.

Clear is not an additional Tab stop; the semantic clearing command remains accessible. The
magnifier is noninteractive. The wheel bubbles to the parent.

ReadOnly blocks typing/clear and retains selection/copying. Disabled blocks focus/actions;
PointerCancel on an armed clear action publishes nothing.

## 5. Measurement and layout

A Row with TextInput height: fixed magnifier, flexible editor, and a clear area with reserved width
even when empty so text does not move.

Preferred width includes padding, magnifier/gaps, editor area, and clear. Text clipping must not
reach the magnifier/button; the core's horizontal scrolling keeps the caret visible.

Logical coordinates and scale through paint; a long query does not grow the entire form.

## 6. Presentation and invalidation

Muted magnifier color, clear as a secondary affordance on hover; focus ring around the complete
field. The placeholder is distinct from an empty query.

A nonempty-to-empty query repaints clear visibility and content; reserved width avoids layout.
Magnifier/gap/font style metrics require layout.

No permanent animation or implicit search spinner. The application can compose a Spinner beside it.

## 7. Accessibility

SemanticRole has no SearchField: use TextInput with a descriptive name and text_value, plus
Focus/SetValue actions. Clear is a Button named “Clear search”, with an Activate action and an
internal retained identity in the component.

The magnifier and decorative icons have Role None. Results and their count are exposed by the
view performing the search, rather than invented by the field.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Copy query/callback after editing and invalidation finish, before submit; an action may remove
SearchField without subsequent access.

Clear ends capture, prepares the empty value, and makes the caret consistent before Binding.set.
An observer exception leaves the query empty and the component recoverable.

Deferred clipboard delivery uses the editor's token/generation; an invalid query, removed component,
or ReadOnly state causes its response to be ignored. Destruction does not clear the Binding.

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

Dependencies: [TextInput](text_input.md), [Button](button.md), icon primitives, and State.
[FindBar](find_bar.md) adds count/navigation; SearchField does not.

Empty query: Enter may submit empty text, clear is absent, Escape bubbles. Spaces are preserved
exactly; no trim/case normalization is imposed on the application engine.

IME: committed Unicode and headless composition are inherited; native preedit/candidate coverage is
separate. An external query change during clear cancels the old action at the checkpoint if its
identity was replaced.

## 10. Files and compatibility

Target: `include/nativeui/search_field.hpp` and `src/search_field.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

SearchFieldStyle and clear/magnifier subparts belong in the same pair; search_field.cpp contains
specialized routing, layout, and text core composition.

Register `src/search_field.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`search_field_live_query`: editing publishes the exact query, preserving spaces/multibyte text.

`search_field_clear_focus`: clear/Escape clear once and retain the caret; no additional Tab stop.

`search_field_escape_empty`: empty Escape bubbles; preedit cancellation precedes clear.

`search_field_submit`: Enter passes an exact snapshot, including empty text, without hidden debounce.

`search_field_layout`: the clear area is reserved without caret movement; correct clipping/scales.

`search_field_throw_stale`: an observer/submit throws/removes and stale clipboard does not alter a
new field.

Add `examples/features/search_field.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
