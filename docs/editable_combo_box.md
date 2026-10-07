# EditableComboBox

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Choose a string from a closed list by typing a filter. Draft text is not the persistent selection
until a valid option is chosen.

NativeUI ComboBox<T> corresponds to Select, without an editor; TextInput/overlays provide the
foundations. There is no public EditableComboBox.

MyGo: `ui/combobox.go`, `Combobox`, `ComboboxBase`, `comboboxBase`, `matching`. Filtering, the opening
chevron, choosing, and reverting to selected on blur are the source contract carried over.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class EditableComboBox {
public:
  using OptionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view candidate, std::string_view query)>;
  EditableComboBox(std::string label, Binding<std::string> selected,
                   std::vector<std::string> options);
  EditableComboBox(std::string label, State<std::string>& selected,
                   std::vector<std::string> options);
  EditableComboBox(std::string label, Binding<std::string> selected, OptionsProvider options);
  EditableComboBox(std::string label, State<std::string>& selected, OptionsProvider options);
  EditableComboBox&& filter(Filter value) &&;
  EditableComboBox&& placeholder(std::string value) &&;
  EditableComboBox&& style(EditableComboBoxStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<std::string> font{"Inter"};
auto selector = ui::EditableComboBox("Font", font,
    std::vector<std::string>{"Inter", "Georgia", "Menlo"}).spec();
```

Target style: TextInputStyle, ComboBoxStyle for frame/chevron, and popup MenuItemStyle. v1 uses
strings as MyGo does; selecting a typed object remains ComboBox<T>.

Default filter: trim ASCII whitespace from query, compare ASCII case-insensitively, preserve
non-ASCII UTF-8 exactly; prefixes first, then substrings, with stable source order. This explicitly
differs from MyGo's Unicode strings.ToLower; an injected filter allows application Unicode policy.

A custom Filter decides inclusion and retains provider order; no ICU or asynchronous filtering is
promised. Call the provider on opening and after each committed edit that must refresh the list,
never during paint.

## 3. State, ownership, and notifications

Binding<string> is the selection; draft, typed/filtering, highlighted, and the overlay session are
local. Display the selection at rest, even if the string was removed from the list.

Typing changes only the draft and suggestions. The first match is highlighted; navigation does not
publish. Choosing a match replaces selected, formats the draft, and selects all text.

Blur or Escape discards an unchosen draft and displays the current selection without set. An
external selected write during editing replaces the draft and closes the session; the application
has priority.

The suggestion snapshot is owned and identified by generation. A provider result for an old
generation cannot be committed to the new query.

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

The chevron/click opens the full list if no typed filter exists; typing opens a filtered list.
Down/Up open then navigate without wrapping past the ends; Home/End remain text navigation unless
explicit popup commands are used.

Enter with a highlight chooses; Enter without a match does not create a free value. Clicking an item
chooses on release. Escape closes/restores; Tab closes/restores and continues focus navigation.

The editor keeps focus during popup navigation; the chevron and rows do not add Tab stops.
PointerCancel ends item arming without choosing.

ReadOnly prevents editing/mutating opening and retains reading; disabled blocks interaction. The
wheel scrolls only the open panel; no spontaneous selection occurs on a closed field.

Composition Update neither filters nor selects; Commit triggers one query generation and committed
text deduplication. Advanced native IME bridging is separate.

## 5. Measurement and layout

Flexible editor and fixed chevron in a common frame; stable preferred field size, independent of
match count. A long draft scrolls horizontally.

The popup is anchored to the entire frame, bounded by the viewport through OverlaySpec/OverlayHandle
and the retained overlay service. Height is capped at 8 rows, then scrolling; no virtualization
claim for v1.

Rows have a fixed height from MenuItemStyle and a width at least as large as the frame, expanded for
text within the viewport. Empty results: a nonselectable “No results” panel.

## 6. Presentation and invalidation

Field focus/hover/read_only states plus popup highlighted state. The application's selected value
is not updated for preview; the presentation must not imply a validated choice when only a filter
has been typed.

Query changes recalculate the filter at the UI checkpoint, then paint/layout the popup; highlight
color is paint only. Text handlers remain in the shared core.

Chevrons/primitives require no public specialized shader/resource. No provider/filter call from
immutable native semantics.

## 7. Accessibility

Target: ComboBox with draft text_value, expanded, and label as name; ListItem options,
distinguishing highlighted from selected.

Application selection appears in description/value at rest; ReadOnly removes Expand/Select/SetValue.
The empty message is Text without actions.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare the new filtered list before publishing the generation; if provider/filter throws, close
the panel, keep the draft, remove all activation from the old snapshot, and restore flags before
propagation. The next opening may retry; no old option remains selectable.

Commit closes the popup then prepares an owned Binding and string; set may remove the field. Do not
reread draft/this after publication.

Rejected deferred opening: remain closed with the draft preserved and continue editing; a stale
handle becomes a no-op. Clipboard and filter callbacks carry safe tokens.

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

Dependencies: [TextInput](text_input.md), [ComboBox](combo_box.md), [PopupMenu](popup_menu.md), Overlay,
and State. A private suggestion core is shared with Autocomplete/TokenField.

Deduplicate exact duplicate options on the first pass for stable text identity; an empty string is
allowed as an explicit option, distinct from the placeholder.

An unknown selection or empty options do not change the Binding; no auto-selection on mount. An
option removed between generations cancels its press before the new snapshot; an invalid filter
does not supply a new choice.

## 10. Files and compatibility

Target: `include/nativeui/editable_combo_box.hpp` and `src/editable_combo_box.cpp`. The header exposes
public declarations and only the necessary template adapters; the .cpp must contain a real retained
core, interactions, measurement, and rendering, never an empty file.

Filter/OptionsProvider aliases and EditableComboBoxStyle remain in the pair. The .cpp contains draft,
filtering, snapshot/generation, input, layout, and popup through the shared core.

Do not add these features to ComboBox<T> by silently changing its meaning; old includes and the
selection-only API remain intact.

Register `src/editable_combo_box.cpp` in NativeUI::Core during implementation. Preserve historical
aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the
public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`editable_combo_box_filter`: prefixes before substrings, ASCII case folding/exact Unicode, and
injected filter documented.

`editable_combo_box_draft`: editing does not set selected; Enter with a match chooses, without a
match it makes no choice.

`editable_combo_box_restore_external`: blur/Escape restore the current selected value; an external
write wins and closes the popup.

`editable_combo_box_duplicates_empty`: option deduplication and a nonselectable empty state; no
default value.

`editable_combo_box_composition`: Update does not filter, Commit produces one generation; no
duplicate commit.

`editable_combo_box_stale_failure`: provider/filter/enqueue throw or an option is removed during a
press: no stale choice.

`editable_combo_box_popup_focus`: focus stays in the editor, stable popup resize/scroll, and Tab exits.

Add `examples/features/editable_combo_box.cpp`, compilable by a public consumer, with a `--self-test`
mode verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
