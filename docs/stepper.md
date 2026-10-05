# Stepper

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A compact two-arrow control that increments/decrements a number, particularly beside NumberInput. Stepper itself has no text-entry area.

NativeUI offers float sliders and private domains but no Stepper. Reuse [state.hpp](../include/nativeui/state.hpp), input, and availability; create a private double domain for new numeric controls.

MyGo: `ui/indicators.go`, `Stepper`. Arrows, Home/End, and hold repeat (400 ms initial, 80 ms repeat) are the behavior to port.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class Stepper {
public:
  explicit Stepper(Binding<double> value);
  explicit Stepper(State<double>& value);
  Stepper&& label(std::string value) &&;
  Stepper&& range(double minimum, double maximum) &&;
  Stepper&& step(double value) &&;
  Stepper&& style(StepperStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<double> copies{1.0};
auto arrows = ui::Stepper(copies).label("Copies").range(1.0, 99.0).step(1.0).spec();
```

Defaults: range[0,100], step=1, vertical arrows; finite min<=max and finite step>0. StepperStyle contains width/height, arrows/border/backgrounds for normal/hovered/pressed/focused/disabled/read_only.

No timer configured by an application callback: 400 ms delay and 80 ms cadence are internal constants with a clock injected in tests. label names the control without occupying a third cell.

## 3. State, ownership, and notifications

Binding<double> is the state. A non-finite external value displays/uses minimum; out-of-range values are clamped for affordances without writing.

Interaction increases/decreases the effective value, then snaps to a minimum-anchored grid and clamps. Min==max is a constant control with no action; an identical set does not notify.

Each repeated tick rereads the current value to respect an external write during hold. Arrows disabled at a bound request no repeat.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

PointerDown on an active arrow increments immediately, captures, and arms repetition. After 400 ms, increment every 80 ms while the pointer remains in the same arrow.

Leaving the arrow suspends repeat; returning restarts the delay without another immediate increment. PointerUp/Cancel, focus loss, hidden/disabled/read_only stop timer and capture.

Up/Right=plus, Down/Left=minus, Home=min, End=max; keyboard repeat follows normalized KeyDown events without doubling pointer repeat. Escape cancels hold without rollback.

Tab gives one stop on the control rather than one per arrow; wheel ignored. ReadOnly permits focus/reading but removes all mutations.

## 5. Measurement and layout

Two equal-height vertical cells, a thin separator, and compact dimensions explicitly defined by StepperStyle. The NumberInput parent aligns the control with field height.

Hit testing of both halves follows arranged bounds in logical coordinates. Zero height or width prevents arming; assign the separator region to the upper half for a deterministic result.

Value does not change measurement; only geometry/arrows/metric style require layout.

## 6. Presentation and invalidation

Arrows at bounds are locally visually disabled; the control itself remains focusable if one direction is possible. Focus ring around both arrows.

During hold, pressed applies only to the active arrow; value changes invalidate paint to reevaluate direction availability.

No frame request after reaching a bound, closing, or unmounting; tick/animation runtime is per instance.

## 7. Accessibility

SemanticRole has no Stepper: use Custom with numeric_value, value_range, Increment/Decrement/SetValue actions, and label name.

Arrows are not separate Tab stops, but their actions are accessible through the control. A SpinButton role is a separate schema extension rather than an available role today.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Before each repeated set, prepare the new value and check owner generation. A callback removing Stepper immediately stops scheduling; no subsequent iteration on a destroyed node.

After an observer exception, stop hold, restore capture/timer scheduling state, and preserve the already committed value. Do not catch up or replay past ticks; the next press starts normally.

Late clock: produce at most one increment per delivered Tick, then set the next deadline from now; avoid an unbounded application burst after suspension.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: Binding, input/clock commands, availability, and [number_input](number_input.md) for composition. Private double domain shared with NumberInput without changing float SliderDomain.

Invalid domain/step: invalid_argument before mount. Step larger than span allowed, with reachable bounds; avoid value+step overflow by clamping intermediates and checking finiteness.

Min==max, NaN value, and complete unavailability never create a permanent timer. Two instances share neither deadline nor active arrow.

## 10. Files and compatibility

Target: `include/nativeui/stepper.hpp` and `src/stepper.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

The header exposes Stepper/StepperStyle; the .cpp implements cells, domain, clock, repeat, and painting. The common numeric engine is private rather than an additional parent component file.

Register `src/stepper.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`stepper_domain`: min/max/step validated; clamp/snap and constant domain without writes.

`stepper_repeat_clock`: Down increments immediately, nothing before 400 ms, then an 80 ms cadence with a controlled clock.

`stepper_pause_cancel`: leaving/returning restarts delay; Up/Cancel/hidden stop all ticks.

`stepper_external_held`: an external write during hold is the basis for the next increment.

`stepper_late_tick`: long suspension creates no notification burst.

`stepper_throw_remove`: observer removes/throws: capture/timer stop and the next interaction recovers.

Add `examples/features/stepper.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.