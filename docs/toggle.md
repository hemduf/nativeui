# Toggle

**Status: existing — extraction required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A boolean switch with a visual sliding thumb; enable/disable an option. This name preserves its current NativeUI meaning.

NativeUI: [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc), public ToggleComponent; [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), Toggle(label, State/Binding). Style in [toggle_style.hpp](../include/nativeui/toggle_style.hpp).

MyGo: `ui/widgets.go`, `Switch`; `ui/base.go`, `SwitchBase`. Its `Toggle` denotes a pressed button ported separately as ToggleButton; NativeUI Switch<T> remains conditional composition.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
Toggle(std::string label, Binding<bool> state);
Toggle(std::string label, State<bool>& state);
Toggle&& style(ToggleStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<bool> enabled{true};
auto control = ui::Toggle("Active", enabled).spec();
```

Preserve label-then-state order, public ToggleComponent, and ToggleStyle. No Switch alias for Toggle or bool type change.

Value notifications use Binding; no audio start/end gesture API or automation callback in the control.

## 3. State, ownership, and notifications

Binding<bool> is the only persistent state. The thumb represents state.get(); space_pressed_/enter_pressed_ bools filter repeats and are not the value.

External values are observed and refresh rendering. An external write during a press remains authoritative; no final PointerUp commit overwrites its change.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

Current behavior to preserve: PointerDown toggles immediately and then captures; PointerUp ends the press without a second set. PointerCancel does not restore the already published value.

Space and Enter toggle on the first KeyDown; suppress auto-repeat until KeyUp. Loss of focus/disabling clears key flags.

ReadOnly blocks mutating pointer input and keys; if applied during capture, end arming without publication. Disabled follows inherited availability. Wheel ignored.

The thumb does not move proportionally to drag: this is a bool rather than a slider. Any new “drag to choose” interaction would be a separate extension rather than a consequence of extraction.

## 5. Measurement and layout

Track, thumb, and label use ToggleStyle and text measurement according to the existing core. The on/off position moves only the thumb, not the control's width.

Keep source padding, gap, and minimum; logical bounds serve hit testing even at high scale. A long label clips under parent constraints.

Resizing during a press does not call state.set again. Metric changes to track/thumb/font imply layout; position/color changes imply paint.

## 6. Presentation and invalidation

Preserve base/checked/hovered/pressed/focused/disabled/read_only style and current patches. Display checked position and distinct focus; do not replace the switch with a button face.

The source promises no automatic time-based thumb animation; extraction creates no timer. A future animation must respect reduced motion and pause when unavailable.

The transition classifier compares metrics and presentation, including patches dependent on checked. Theme remains per instance/scope.

## 7. Accessibility

Target: Toggle, Checked/Unchecked checked state, Toggle action when mutable; label name. Read bool after immediate commit without waiting for PointerUp.

ToggleComponent currently publishes no semantics in the reviewed file. Add the override during extraction without claiming a delivered native bridge.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Every context/capture must be consistent before immediate publication at PointerDown. Copy Binding if needed to avoid rereading this component after set removes its subtree.

An observer exception preserves the committed value; restore keyboard flags/capture for the next interaction. Unmounting never resets the option to false.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: ThemeBinding, State, PressActivationState, and TextService. [toggle_button](toggle_button.md) shares bool logic but explicitly has different commit timing.

Two switches bound to the same bool display the same value but retain distinct capture/focus. Expired State follows Binding; no raw reference.

The widget directly triggers no system service, OS preference, automation, or DSP processing. The application observer performs the effect.

## 10. Files and compatibility

Target: `include/nativeui/toggle.hpp` and `src/toggle.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Move public ToggleComponent and the builder out of the .inc files; the header preserves their public declarations and signatures. The .cpp contains observer/input/layout/paint and style classification.

toggle_style.hpp remains compatible; existing widgets.hpp/nativeui.hpp includes continue exporting Toggle. Never rename this component Switch.

Register `src/toggle.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`toggle_down_commit`: PointerDown writes once; PointerUp/Cancel neither undo nor rewrite.

`toggle_key_repeat`: Space/Enter only at the first KeyDown; KeyUp permits the next activation.

`toggle_read_only`: a ReadOnly transition during capture ends the gesture and preserves the value.

`toggle_external_change`: an external value between Down and Up is not overwritten on release.

`toggle_style`: checked changes thumb and exact invalidation; geometry preserved during extraction.

`toggle_publish_remove_throw`: an observer removes/throws after commit; captures and the next Toggle recover.

Add `examples/features/toggle.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.