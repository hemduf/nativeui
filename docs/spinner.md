# Spinner

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A circular activity indicator of unknown duration, without a percentage or interaction. Useful beside a status Label or busy button.

Absent from the toolkit; arc/path primitives and [animation.hpp](../include/nativeui/animation.hpp) exist, with AnimationContext and Dispatcher calculating animations on an injected clock.

MyGo: `ui/indicators.go`, `Spinner`, twelve spokes with a 900 ms cycle and unknown RoleProgress. The target adopts this rhythm and interruptible animation without invoking AnimationFrame in an imported Go loop.

## 2. Public API and composition

Proposed target API:

```cpp
class Spinner {
public:
    explicit Spinner(std::string label = {});
    Spinner&& active(Binding<bool> value) &&;
    Spinner&& active(State<bool>& value) &&;
    Spinner&& size(double value) &&;
    Spinner&& reduced_motion(bool value = true) &&;
    Spinner&& style(SpinnerStyle value) &&;
    Spec spec() &&;
};
```

Defaults: active=true when Binding is absent, size 18 DIP, 900 ms cycle, twelve spokes. SpinnerStyle contains color/thickness_ratio and optional size; explicit `.size` takes precedence over style size. New numerics are double.

Proposed target example:

```cpp
ui::State<bool> working{true};
auto busy = ui::Spinner{"Loading"}
    .active(working)
    .size(18.0)
    .spec();
```

## 3. State, ownership, and notifications

Label/style/options are owned; optional active Binding<bool> is observed through RAII, with State overload converted immediately. Phase and animation handle belong to the instance.

If the Binding source disappears: `valid()` becomes false, get retains the last value but activity is considered inactive on the next revalidation; no callback or automatic destruction notification. Do not write false to the model.

active=false preserves measurement but draws no spokes; phase restarts from zero on reactivation. No on_change/finished or automatic relationship with an application process.

## 4. Interactions

Display only: pointer, dragging, wheel, keyboard, text, and drops are Ignored; non-focusable, with no capture or confirmation/cancellation.

Disabled permits no action; apply disabled color and suspend animation while retaining a static icon when active. ReadOnly does not change busy semantics.

When placed inside Button, the parent handles activation; Spinner has no invisible interactive region or stop command.

## 5. Measurement and layout

Preferred/minimum: a square of the chosen size; extra space centers the spinner without stretching it. Actual diameter = min(width,height,requested size).

Spokes span fractions 0.22..0.46 of diameter, with default thickness 0.09, bounded to the allocated space. Size 0 produces an empty surface without divisions/wakes.

Logical dimensions; no framebuffer rounding of diameters inside the component. Padding belongs to composition, with no hidden external space.

## 6. Presentation and invalidation

Default color=Theme muted_text; owned SpinnerStyle override. Twelve spokes, fading from the lead down to 15% opacity, without rewriting parent opacity.

Cycle uses a linear 0→1 tween with existing AnimationContext, with token/generation-protected rearming. Animated phase affects paint only; no layout or semantic announcement every 16 ms.

Explicit reduced_motion or absent timing means static phase 0 drawing while active. Hidden/collapsed/unmounted stops all wakes; becoming visible restarts from zero. This option does not claim a delivered OS preference.

## 7. Accessibility

Target contract: ProgressBar with label name, no numeric_value/range for unknown activity, and no action. Inactive is omitted as an activity indicator rather than announced as completed.

A decorative Spinner in Button may be excluded by composition; Button describes its application state separately. Do not duplicate reading of a neighboring Label.

Role/hooks are available, target publication is testable headlessly; T068 bridges are deferred. No IME.

## 8. Lifecycle and recovery

UI/main thread; RAII subscription and animation context/handle. Mounting first validates size/options, then acquires timing and token; teardown cancels before releasing the paint target.

Failed timer/tween arming removes the provisional entry and retains static phase. A future inactive→active transition may retry; no poisoned busy guard.

Internal cycle completion checks lifetime/generation before rearming; a stale callback after Hidden/unmounting is a no-op, never run synchronously if enqueue fails. Destruction is no-throw and invokes no callbacks.

## 9. Dependencies and edge cases

Reuses AnimationContext/DispatcherProvider and Theme. No concurrent timer service, thread, or shared wall clock; the existing animation clock supports manual tests.

Non-finite or negative size causes invalid_argument; zero is valid. Non-finite/non-positive thickness_ratio or >0.5 causes invalid_argument before publication. Empty label allowed for decoration.

Active changes during a cycle cancel exactly that cycle and its future restarts. Two Spinners may share a Dispatcher owner but not phases/options/tokens.

## 10. Files and compatibility

Target: `include/nativeui/spinner.hpp` and `src/spinner.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: existing AnimationContext/Painter/Theme. SpinnerStyle stays with the parent; measurement/spokes/subscription/cycle core in spinner.cpp, without an inline scheduler or new native API.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `spinner_cycle`: twelve spokes and a 900 ms cycle follow the manual clock.
- `spinner_active_lifetime`: active false and source destruction suspend at the next access without writeback.
- `spinner_reduced_static`: reduced motion and unavailable dispatcher produce static phase.
- `spinner_zero_hidden`: zero size/Hidden/Collapsed stop wakes and preserve intended space.
- `spinner_timer_fault`: rejected arming or an exception permits the next transition.
- `spinner_multi_instance`: phases/colors/timer teardown do not affect another UI.

Create `examples/features/spinner.cpp` and target `nativeui_example_spinner`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.