# FindBar

Status: **new — implementation required**.

[Component catalog](widgets.md)

Sources studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A document search bar with a query field, count, previous/next, and closing. The application
computes matches and applies movement; the widget traverses no text.

NativeUI has CommandScope and TextInput but no FindBar. The new component assembles controls and
observations without a search engine or global index.

MyGo: `ui/feedback.go`, `FindBar`. External open/query, count/current, circular navigation,
Enter/Shift+Enter, Cmd+G on macOS and F3 elsewhere; opening focuses and selects the query.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class FindBar {
public:
  FindBar(Binding<bool> open, Binding<std::string> query,
          Binding<std::size_t> matches, Binding<std::optional<std::size_t>> current);
  FindBar(State<bool>& open, State<std::string>& query,
          State<std::size_t>& matches, State<std::optional<std::size_t>>& current);
  FindBar&& label(std::string value) &&;
  FindBar&& on_navigate(std::function<void(std::size_t)> callback) &&;
  FindBar&& style(FindBarStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<bool> open{true};
ui::State<std::string> query{"gain"};
ui::State<std::size_t> matches{3};
ui::State<std::optional<std::size_t>> current{std::optional<std::size_t>{0}};
auto find = ui::FindBar(open, query, matches, current).label("Find")
    .on_navigate([](std::size_t) {}).spec();
```

matches is read/observed but never written by the widget. Optional current expresses the absence of
a choice; replace MyGo's artificial index 0 when there are no matches with an explicit contract.

Target FindBarStyle: SearchFieldStyle/TextInputStyle, navigation/close ButtonStyle, status TextStyle,
gap/padding, and maximum field width; default label “Find”.

on_navigate receives the target index after current publication, even if unchanged with one match;
the application can recenter the view. No additional query on_change.

## 3. State, ownership, and notifications

open/query/current are mutable bindings; matches is an external source used read-only. Opened/closed
phases and the previous focus ID are local to the bar.

Query editing publishes live; the application updates matches/current. During rendering, an unknown
or out-of-range current is not rewritten: use a clamped effective index for status/navigation, with
absence when matches=0.

The first navigation without current chooses 0 for next and count-1 for previous; otherwise use
modulo count. A match count change cancels an armed action if its old generation target becomes
invalid.

Closing publishes open=false and preserves query/current; reopening retains these values and
selects the query for replacement. The widget does not reset defaults on opening.

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

On a false-to-true transition, request field focus and select query. Enter/Shift+Enter move
next/previous; buttons use the same actions, circular at the ends.

Navigation shortcuts: CommandScope receives new portable FindNext/FindPrevious commands; append
the enumerators to Command and translate Cmd+G/Shift+Cmd+G or F3/Shift+F3 in the private input layer.

Key::F3 does not currently exist: append it to the enum without renumbering existing values, with
mapping tests. The component neither tests OS types nor registers a process-wide keyboard hook.

Escape closes the bar from the field (after canceling active composition), as does the Done button.
Tab traverses field, previous, next, done; directions are disabled when count=0.

On closing, restore previous focus if valid and if focus still belongs to the bar; otherwise leave
application focus alone. The wheel does not navigate; ReadOnly prevents query/current mutations,
while closing remains a visibility action.

## 5. Measurement and layout

Closed: no painted child or focus stop, zero measurement. Open: Row with flexible field,
reserved-width status, navigation/done buttons; control height plus padding.

Narrow width prioritizes field and done; status may clip before shrinking action areas; buttons
retain minimum size. No implicit overflow or popup.

Status text uses tabular digits if the service supports them, without changing the match count.
Logical coordinates and clipping follow the parent.

## 6. Presentation and invalidation

Surface background and separating border, muted status. An empty query shows empty status;
a nonempty query/count0 shows “No results”; otherwise “n of count” with n=effective+1.

An invalid current value does not appear as an impossible index; mark absence/invalid external value
in the description without implicit set.

Query/count/current changes require paint; an open transition or metric style requires
layout/structure. No timer or automatic search recalculation loop.

## 7. Accessibility

Target: named Group (Toolbar is absent from SemanticRole), TextInput field, Text status, and
Previous/Next/Done buttons with real actions.

SemanticRole has neither Status nor a live region; provide a description/current text snapshot and
plan a separate announcement extension if required by a native bridge. No native AT result is claimed.

Closed removes accessible descendants. Expose query, count, and current as immutable backend-neutral
data, never invoking search callbacks from native reads.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role
specified here is a target contract: its presence in the enum does not prove that the current
component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI result is claimed; verify the
headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Navigation prepares the target from a count/current snapshot, then publishes current and calls a
copied on_navigate if the owner/generation still live and Bindings are valid. A started callback is
never replayed.

The current observer may remove the bar or replace count; recheck token/generation before
on_navigate, canceling an obsolete action rather than sending a wrong index.

open false closes text input/composition and capture before publishing hidden descendants.
Destruction calls neither the close action nor navigate; previous focus is a safe handle, not Node*.

Failure of a focus/deferred command request remains pending at the checkpoint without a synchronous
fallback; a throwing observer retains already-committed values and reusable dispatch.

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

Dependencies: [TextInput](text_input.md), [SearchField](search_field.md), [Button](button.md),
[CommandScope](command_scope.md), Focus/State. Search/replace/regex options belong to the application.

count=0: no navigation/callback and current is preserved; count=1: navigation may call back with the
same index. Count changes during a gesture use generation to cancel a removed target.

Invalid bindings: the last snapshot may be displayed, but no further mutation/callback; an invalid
open hides the bar at the checkpoint. Extremely long queries and large counts do not overflow
modulo arithmetic.

## 10. Files and compatibility

Target: `include/nativeui/find_bar.hpp` and `src/find_bar.cpp`. The header exposes public
declarations and only the necessary template adapters; the .cpp must contain a real retained core,
interactions, measurement, and rendering, never an empty file.

FindBarStyle and all models/status remain in the pair; find_bar.cpp owns phases, observations,
commands, navigation, and layout.

New commands and keys are added to the shared input model and normalized in the private backend;
enum compatibility tests are a prerequisite for component implementation.

Register `src/find_bar.cpp` in NativeUI::Core during implementation. Preserve historical aggregate
includes as compatible entry points; no Pugl, Skia, OS, or plugin types belong in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed by
this documentation batch.

## 11. Tests and acceptance criteria

Tests are required during implementation; this documentation reports no execution results.

`find_bar_opening`: open true requests focus/query selection; closed measures zero with no focusable
descendants.

`find_bar_navigate_wrap`: forward/back/modulo and absent current give the expected indices; a
one-match callback is possible.

`find_bar_count_change`: count0 ignores navigation and removes directions; a target removed during
a press is canceled.

`find_bar_shortcuts`: Enter/Shift+Enter and normalized macOS/other commands reach the same action,
with stable enums.

`find_bar_closing`: Escape/Done retain query; focus is restored only if still owned.

`find_bar_reentrant`: the current observer replaces count/removes the bar/throws: no wrong navigation
or retry.

`find_bar_invalid_state`: removing a State retains the last safe snapshot and stops actions/callbacks.

Add `examples/features/find_bar.cpp`, compilable by a public consumer, with a `--self-test` mode
verifying the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared with stable geometry,
two independent instances work, historical includes compile, and new sources are warning-free.
