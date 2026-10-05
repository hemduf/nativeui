# Autocomplete

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit free text with optional suggestions; input without a suggestion remains a valid value. This
differs from EditableComboBox's closed selection.

NativeUI has TextInput and popups, but no Autocomplete. The new editor reuses the private suggestion
engine without duplicating Overlay/Focus.

MyGo: `ui/combobox.go`, `Autocomplete`, `comboboxBase`, `matching`. It presents suggestions containing
the typed text, prefixes first, excludes a complete equivalent, and does not preselect on the first
character.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class Autocomplete {
public:
  using SuggestionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view candidate, std::string_view query)>;
  Autocomplete(std::string label, Binding<std::string> value,
               std::vector<std::string> suggestions);
  Autocomplete(std::string label, State<std::string>& value,
               std::vector<std::string> suggestions);
  Autocomplete(std::string label, Binding<std::string> value, SuggestionsProvider suggestions);
  Autocomplete(std::string label, State<std::string>& value, SuggestionsProvider suggestions);
  Autocomplete&& filter(Filter value) &&;
  Autocomplete&& placeholder(std::string value) &&;
  Autocomplete&& on_submit(std::function<void(const std::string&)> callback) &&;
  Autocomplete&& style(AutocompleteStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<std::string> city{""};
auto city_input = ui::Autocomplete("City", city,
    std::vector<std::string>{"Paris", "Pau", "Lyon"})
    .on_submit([](const std::string&) {}).spec();
```

The default filter and provider follow [EditableComboBox](editable_combo_box.md): ASCII case folding,
stable prefixes then substrings; exact Unicode by default, with an injected extended policy.

Target AutocompleteStyle: TextInputStyle, MenuItemStyle, and popup metrics; the Binding's freeform
string is not constrained to a suggestion.

## 3. State, ownership, and notifications

Binding<string> is live text: all valid edits by the text engine publish, with or without a match.
Choosing a suggested value is a normal write to the same Binding.

There is initially no highlight after typing; an arrow chooses a preview target without set. Taking
a suggestion copies its entire text, moves the caret to the end, and closes the popup.

An external write changes the editor, closes old suggestions, and does not produce submit. An
identical value from its own commit preserves the caret.

Escape cancels only the popup and retains live text without reverting to an earlier choice. A
second Escape with the popup closed follows TextInput's snapshot policy.

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

Typing a nonblank query opens matches, excluding suggestions equivalent to the text under the
default comparison; an empty/blank query shows no panel.

Down/Up open/target the first/last option, then navigate without wrapping. Enter with a highlight
takes the suggestion without on_submit; Enter without a target submits live text. Clicking a
suggestion validates on release.

Escape closes preview; Tab closes and exits without choosing; loss of focus closes it. The editor
retains caret/focus, unlike the ComboBox selection popup.

The panel wheel scrolls; a closed field selects nothing. ReadOnly retains selection/copying but
allows no suggestion or commit; Disabled follows availability.

Composition Update does not call the provider; query Commit starts exactly one generation. Native
preedit/candidate bridging remains separate.

## 5. Measurement and layout

Stable TextInputStyle size; popup width at least the field width, height up to eight rows, then a
scroll viewport. The parent does not grow with the list.

Long suggestions clip/ellipsize within rows; paint and hit-testing use the same MenuItemStyle height.
The overlay service tracks the logical anchor.

No selectable empty panel: no suggestions or complete equivalent text closes the panel. Resize
preserves the target by stable text if it is still present.

## 6. Presentation and invalidation

Field focus ring; row highlight does not conflate with the value already in the Binding. The
placeholder remains for empty text.

Compute the filter outside paint, then publish an owned snapshot. Query/list metric changes
remeasure the popup; highlight colors require only paint.

The suggestion provider may be expensive but remains synchronous on the UI thread; no implicit
debounce/networking. Remote search requires an application using safe snapshots, beyond this v1.

## 7. Accessibility

Target: editable ComboBox with expanded, text_value, and a suggestion description; named ListItem
options. SetValue/Select actions are available when mutable.

Announce highlighted through the snapshot's backend-neutral identity, never a Node*. A free value
without matches remains valid text_value, without a semantic error.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Filtering/provider prepares a complete snapshot tied to the query generation. An exception retains
live text, closes the stale panel, and leaves the caret usable.

Choosing ends popup/capture before Binding.set; submit copies value/callback before invocation. A
throwing observer or submit is not replayed; the published model remains.

Removal of a suggestion or an invalid State cancels the old generation's target; no deferred
clipboard/provider response after destruction can modify the replacement.

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

Dependencies: [TextInput](text_input.md), [EditableComboBox](editable_combo_box.md), the suggestion
core, Overlay/Focus/State.

Deduplicate exact suggestions, retaining the first occurrence; a blank query gets no suggestions
but its value is preserved. Empty suggestions never prevent free submission.

No global learning of choices or history between instances; options/string callbacks belong to the
component or an explicit application model.

## 10. Files and compatibility

Target: `include/nativeui/autocomplete.hpp` and `src/autocomplete.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

AutocompleteStyle and provider/filter remain in the component header; autocomplete.cpp contains
freeform/submit behavior, runtime, layout, and painting, sharing only the private suggestion engine.

Register `src/autocomplete.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`autocomplete_freeform`: text outside the list publishes and submits normally; no forced reversion
to a suggestion.

`autocomplete_highlight`: the first character does not preselect; Down then Enter takes one suggestion.

`autocomplete_submit_choice`: Enter on a target does not submit; Enter without a target submits
exact text.

`autocomplete_escape`: closing the popup preserves live text; a second Escape follows the
TextInput baseline.

`autocomplete_filter_empty`: empty/blank/equivalent closes the panel; stable deduplication and order.

`autocomplete_external_failure`: external write/provider/filter failure close stale content without
losing text.

`autocomplete_remove_throw`: a removed suggestion/callback removing or throwing causes no stale mutation.

Add `examples/features/autocomplete.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
