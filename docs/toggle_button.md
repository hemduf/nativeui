# ToggleButton

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A button that retains a pressed value: bold, italic, or a toolbar option. It is distinct from Toggle, the existing sliding switch.

NativeUI provides [Button](button.md), [Toggle](toggle.md), Binding, and style patches; no public ToggleButton.

MyGo: `ui/toggle.go`, `ToggleBase`, `Toggle`, `pressedColor`. The port adds a name without colliding with Switch<T>, which remains conditional composition.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class ToggleButton {
public:
  ToggleButton(std::string label, Binding<bool> pressed);
  ToggleButton(std::string label, State<bool>& pressed);
  ToggleButton&& style(ToggleButtonStyle value) &&;
  ToggleButton&& content(Spec value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<bool> bold{false};
auto control = ui::ToggleButton("Bold", bold).spec();
```

Target ToggleButtonStyle: base, hovered, pressed, selected, focused, disabled, read_only, with ButtonStyle metrics. selected describes the persistent value; pressed describes only the current press.

content replaces the visual label and preserves its semantic name. Notifications use Binding<bool>; no second on_change callback is needed.

## 3. State, ownership, and notifications

Binding<bool> is the sole persistent source of truth. Selected value and momentary press are never conflated; activation computes !pressed.get() at commit time.

An external write during a press updates appearance; on release, toggle the current value rather than the value captured at the start. This avoids overwriting a recent application command.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

An inside click toggles once on release. Moving outside, PointerCancel, and loss of focus do not write; no toggle at PointerDown, unlike existing Toggle.

Space: arm at KeyDown, toggle at KeyUp; Enter: first KeyDown, with repeats suppressed. Wheel has no effect; dragging outside the button cancels.

ReadOnly remains focusable and exposes the value but consumes mutating input. Disabled follows inherited availability and clears armed gestures.

Within ToggleGroup/Toolbar, arrows move focus without changing this value; only activation selects or clears the independent option.

## 5. Measurement and layout

Measurement follows Button: label or content, horizontal padding, control height, and minimum width. The bool value does not change dimensions.

Clip icon content under narrow constraints. Group and toolbar arrange the control; the button does not read neighbor widths from paint.

Resizing during capture uses current bounds; the release point is in logical coordinates.

## 6. Presentation and invalidation

selected applies a persistent depressed face. hovered/pressed may change colors, then focus keeps its ring; apply disabled and read_only priority clearly before active affordances.

Segmented styling comes locally from ToggleGroup. It does not mutate global Theme state or styles of buttons outside the group.

Binding change: paint only unless the selected patch contains a metric, in which case layout. Reserve ring space to avoid shifts on focus.

## 7. Accessibility

Target: Button with Checked/Unchecked checked state and Toggle action; SemanticRole::Toggle is reserved here for the existing visual switch. A dedicated ToggleButton role may be added separately.

Announce the persistent value even without focus; the name remains the label for icon-only content. ReadOnly removes mutating actions.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Activation disarms, releases capture, and copies the Binding before set(). An observer removing the button leaves no subsequent component access.

Do not roll back a published value if an observer throws; the next interaction starts from the current value.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [button](button.md), [toggle_group](toggle_group.md), availability, ThemeBinding, State, and the activation state machine.

Empty label with an icon: supply an accessible name; an external callback/observer may replace State but must never retain an input context.

An exclusive group must use SegmentedControl rather than observing several ToggleButtons to impose implicit exclusivity.

## 10. Files and compatibility

Target: `include/nativeui/toggle_button.hpp` and `src/toggle_button.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

ToggleButtonStyle and content variants remain in this file pair; the core shares Button's activation state machine without importing widgets_basic.inc as implementation.

Register `src/toggle_button.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`toggle_button_persistent`: selected value remains after release; temporary pressed state disappears.

`toggle_button_cancel`: leaving bounds and PointerCancel do not toggle; wheel changes nothing.

`toggle_button_external_during_press`: an external write followed by activation toggles the latest value.

`toggle_button_read_only`: focus/reading allowed; keyboard and pointer publish no value.

`toggle_button_remove_observer`: an observer removes the button or throws; the next interaction remains usable.

`toggle_button_group_independent`: arrows move only focus; several values may remain true.

Add `examples/features/toggle_button.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.