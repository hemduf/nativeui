# Slider

**Status: existing — extraction required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit a continuous or quantized value along a horizontal/vertical axis. Optionally display a formatted value without carrying plugin automation.

NativeUI: [widgets_slider.inc](../include/nativeui/detail/widgets_slider.inc), Slider, SliderVisualState, and core; [slider_value.hpp](../include/nativeui/detail/slider_value.hpp) defines domain and normalization. Shared style in [slider_style.hpp](../include/nativeui/slider_style.hpp).

MyGo: `ui/widgets.go`, `Slider`; `ui/base.go`, `SliderBase`. MyGo's domain is float64; NativeUI float is an existing API to preserve.

## 2. Public API and composition

`Slider::on_edit(EditCallbacks<float>)` and `wheel_enabled(bool = true)`
are available after integration with main. Wheel editing is disabled by default.
Pointer edits publish begin/change/end or cancel; keyboard and opted-in wheel
commands are atomic edits. Unchanged commands do not begin an edit. Cancellation
keeps the last committed value. Owned sessions and originating-contact checks
protect subtree removal and newer nested gestures. See [value editing](value-editing.md).

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
explicit Slider(Binding<float> state);
explicit Slider(State<float>& state);
Slider&& range(float minimum, float maximum) &&;
Slider&& step(float value) &&;
Slider&& orientation(SliderOrientation value) &&;
Slider&& formatter(Formatter value) &&;
Slider&& style(SliderStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<float> amount{0.5f};
auto slider = ui::Slider(amount).range(0.0f, 1.0f).step(0.01f)
    .orientation(ui::SliderOrientation::Horizontal).spec();
```

Formatter retains the core's std::function<std::string(float)>; builder defaults: range [0,1], step=0, horizontal. step=0 means continuous movement.

Constructors retain State<float>/Binding<float>. Porting a MyGo double algorithm does not justify implicit migration of the public type.

## 3. State, ownership, and notifications

Binding<float> is authoritative; external values are never snapped/clamped and rewritten at mount/paint. For geometry alone: non-finite becomes minimum, out-of-range is clamped.

User interaction: quantize minimum + round((value-minimum)/step)*step when step>0, then clamp; use double intermediates to avoid float-span overflow.

Dragging publishes live at Down/Move/Up only when the value changes. An external write during drag is reread at the next target; PointerCancel does not restore the old snapshot.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

PointerDown chooses the target along the axis and captures; Move follows outside bounds with a bounded value; Up finalizes the target and releases; PointerCancel stops without rollback.

Left/Down decrease, Right/Up increase; Home=min, End=max. step>0 uses step increments; continuous uses span/100, Shift uses span/1000.

Vertical orientation: maximum at top and minimum at bottom according to existing SliderTrackAxis. ReadOnly consumes mutations and stops capture without set; Disabled receives no editing.

Wheel and Escape do not currently edit the slider; preserve their ignored behavior. Tab and focus follow runtime, with one focusable control.

## 5. Measurement and layout

Preserve existing metrics: control length, track thickness, thumb insets, and formatter space define the same axis for painting and mapping.

Formatter reserves a display band horizontally and vertically; its presence must be identical in slider_track_axis for pointer and paint.

When available axis length is zero, mapping retains a safe fraction without division by zero. All positions are logical; scale alters neither values nor the quantization threshold.

## 6. Presentation and invalidation

SliderStyle and SliderVisualState Normal/Hover/Pressed/Focused/Disabled/ReadOnly remain public. Resolve face and ring through Theme and existing patches.

Binding change invalidates paint; metric changes to track/width/minimum/formatter invalidate layout. Formatter is a rendering callback: call it only under the UI contract, without model mutation.

A throwing formatter must leave the previous frame valid and save/clip operations balanced. No timer or automatic animation is added during extraction.

## 7. Accessibility

Target: Slider, effective numeric_value, value_range {minimum,maximum,step}, Increment/Decrement/SetValue and Focus actions when mutable.

The current widget source publishes no semantics; adding the target snapshot preserves clamp rules without writes. Formatter text may supplement text_value but must not be called from immutable native reads.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Determine target and finish capture/context operations before publishing State; an observer removing the slider allows no subsequent core access.

Remove Binding subscription at unmounting; cancel capture and clear local focus. Observer or formatter exceptions restore flags without re-emitting the last set.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: immutable SliderDomain, slider_track_axis, ThemeBinding/State, and TextService formatter. [range_slider](range_slider.md) reuses the domain.

minimum/max must be finite with min<max; step finite>=0. Throw invalid_argument during core creation before publication; preserve current contractual timing and message if tested.

Step larger than span is allowed, with the maximum still reachable; external NaN/inf values render without state corruption. Reject a degenerate domain rather than displaying a misleading empty slider.

## 10. Files and compatibility

Target: `include/nativeui/slider.hpp` and `src/slider.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Move the non-template core out of widgets_slider.inc. Preserve already visible public SliderVisualState, SliderOrientation, SliderStyle, Formatter, and functions; slider_style.hpp remains compatible.

SliderDomain stays private and shared with RangeSlider; no dependency of the public header on the Skia runtime.

Register `src/slider.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`slider_domain`: invalid min/max/step rejected, step=0 continuous, and large steps correct.

`slider_external`: mount/paint of NaN/inf/out-of-range does not rewrite State.

`slider_axis`: horizontal/vertical and formatter give the same position for paint and hit testing.

`slider_keys`: arrows/Home/End and continuous Shift produce exact increments.

`slider_cancel`: cancellation keeps the last live value; ReadOnly stops capture without writing.

`slider_throw_remove`: an observer removes/throws and a formatter throws; next input/frame recover.

Add `examples/features/slider.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.