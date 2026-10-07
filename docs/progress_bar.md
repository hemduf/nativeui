# ProgressBar

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Display determinate progress or, as an extension, indeterminate activity. The control never modifies the value and does not represent a domain-specific level with thresholds; that role belongs to Meter.

Available in [widgets_progress_meter.inc](../include/nativeui/detail/widgets_progress_meter.inc), ProgressBar and detail::BoundedDisplayComponent. Finite min<max domain; clamped view value, non-finite=min; horizontal/vertical directions and optional formatter. Styles in [progress_style.hpp](../include/nativeui/progress_style.hpp).

MyGo: `ui/widgets.go`, `Progress`, range 0..1, negative indicates indeterminate, and Reverse allows RTL. The target preserves an explicit float range and uses a separate indeterminate mode to avoid repurposing a valid negative value.

## 2. Public API and composition

Verified existing API: `explicit ProgressBar(State<float>&, float minimum=0, float maximum=1)`, `orientation(ProgressOrientation) &&`, `formatter(Formatter) &&` with std::function<std::string(float)>, `style(ProgressBarStyle) &&`, `spec() &&`.

Verified example using the existing API:

```cpp
ui::State<float> progress{0.4f};
auto bar = ui::ProgressBar{progress, 0.0f, 1.0f}
    .orientation(ui::ProgressOrientation::Horizontal)
    .formatter([](float value) { return std::to_string(value); })
    .spec();
```

Target additions: constructor `ProgressBar(Binding<float>, float minimum=0, float maximum=1)` and rvalue fluent `indeterminate(bool=true)`, `reversed(bool=true)`, `reduced_motion(bool=true)`. Preserve historical float and State signatures; no on_change callback or negative sentinel.

## 3. State, ownership, and notifications

Current source directly borrows State<float>&; it must stay alive until unmounting. Enhanced contract: immediately convert the State overload to Binding, with the same source API but safer runtime ownership.

Binding in the new API retains the last control block. Source destroyed: valid=false, last value readable, set never called, observe inactive without destruction notification. Revalidate to disarm activity at the next access; invent no user event.

Effective view value is clamped without writeback, with fraction calculated in double for wide float ranges. Indeterminate mode ignores the value for geometry but does not remove the model value. Observations invalidate only paint/semantics.

## 4. Interactions

Display only: no focus, capture, active hover, wheel, dragging, keyboard, or text input. Inputs return Ignored and do not use formatter as an action.

ReadOnly adds no function; Disabled retains disabled styling and a descriptive value. Indeterminate mode is not a confirmation or operation that Escape cancels.

User writes are impossible, including through SemanticAction::SetValue. Cancelling a process remains a separate application Button.

## 5. Measurement and layout

The historical path uses horizontal/vertical and formatted preferred sizes according to style; formatter is not called for measurement. Preserve space when text changes.

Horizontal fills from the left; vertical from the bottom. reversed reverses the fill origin; fraction 0/1 gives empty/full. All rectangles are logical and bounded to the track.

Indeterminate: a segment covering 30% of length traverses the track in 1.4 s, clipped at edges; reduced_motion/no timing uses a static centered 30% segment without measurement changes. Zero dimensions request neither animation nor pixels.

## 6. Presentation and invalidation

Preserve existing ProgressBarStyle/ProgressStylePatch in progress_style.hpp, with theme, then explicit style, then availability precedence. Do not promise unknown Theme slots.

Determinate: formatter receives the effective float value. Indeterminate: do not call the numeric formatter; the track is sufficient, with activity text composed through a neighboring Label. Reversed does not change formatter.

Animation uses existing AnimationContext, a linear 0→1 tween, and generation-protected cycle restart. Explicit reduced_motion bool defaults to false; no claim that OS detection is delivered. Stop when Hidden/Collapsed/unmounted or the source is invalid.

## 7. Accessibility

Target contract: ProgressBar, effective range/numeric_value for determinate mode; indeterminate omits numeric_value and value_range and provides an “indeterminate progress” description. No invented -1 value within a range.

No mutating action or focus. Semantics updates follow data/mode rather than every animation frame; readers must not be flooded with phase ValueChanged events.

Hooks/role exist, but the current override is not proof of publication. Native T068 bridges are deferred; no IME.

## 8. Lifecycle and recovery

UI/main thread; per-instance RAII subscriptions/animation handles, stopped on unmounting/visual disabling. A stale invalidator is a lifetime-safe no-op.

An owned formatter may throw/reenter: no synchronous structural mutation during paint; restore scopes/guards before C++ propagation. A started invocation is never automatically replayed.

Without a valid DispatcherProvider, use a static centered phase; no thread/sleep or synchronous fallback for a deferred callback. Arming failure removes the unpublished handle, retains static rendering, and permits recovery at the next actual access. Destruction is no-throw.

## 9. Dependencies and edge cases

Reuses existing Binding/Theme/Painter/AnimationContext and DispatcherProvider. No audio adapter or parameter changes.

Non-finite/reversed/equal domains: invalid_argument when the component is instantiated, as currently. External NaN/inf=min visually; model intact. Out-of-range value=clamped view only.

Returning from indeterminate to determinate immediately shows the last value. A hidden external update must appear on remounting. Empty formatting is allowed; a formatter mutating UI must defer changes according to CODE_REVIEW.

## 10. Files and compatibility

Target: `include/nativeui/progress_bar.hpp` and `src/progress_bar.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_progress_meter.inc` and progress_style.hpp. Preserve ProgressOrientation, Formatter, and style entry points. Extract real ProgressBar and Meter cores into their respective .cpp files; domain/style helpers may stay privately shared without a fictitious .cpp.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `progress_bar_legacy_range`: valid/wide domains and invalid_argument preserve their results.
- `progress_bar_formatter_effective`: formatter receives clamp/minimum for non-finite values without writeback.
- `progress_bar_orientation_reverse`: all four directions fill their track exactly.
- `progress_bar_indeterminate_clock`: 1.4 s cycle, clipping, and transition to determinate under a manual clock.
- `progress_bar_reduced_hidden`: reduced motion, absent timing, and Hidden stop wakes with a static phase.
- `progress_bar_formatter_throw`: a formatter exception does not prevent the next frame or sibling rendering.

Create `examples/features/progress_bar.cpp` and target `nativeui_example_progress_bar`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Reuse `examples/features/t033_progress_meter.cpp` and existing Progress/Meter tests; validate the Binding overload and indeterminate activity separately.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.