# Knob

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A continuous rotary float-value control for any interface. A musical application may use it; automation and audio gestures remain in the application adapter.

Available in [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), `Knob(label, Binding<float>)` and a State overload. Public `KnobComponent` in [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). It accepts a range, vertical dragging, and keyboard arrows.

MyGo has no standalone Knob among its 54 families; `Slider` in `ui/widgets.go` provides comparable continuous behavior with a different presentation. Preserve the NativeUI extension.

## 2. Public API and composition

Exact existing API: `Knob(std::string, Binding<float>)`, `Knob(std::string, State<float>&)`, rvalue fluent `range(float minimum, float maximum)`, and `spec() &&`. No `on_change`, formatter, or style currently exists.

Verified example using the existing API:

```cpp
ui::State<float> amount{0.5f};
auto control = ui::Knob{"Amount", amount}
    .range(0.0f, 1.0f)
    .spec();
```

The State overload is immediately converted to Binding; after its source is destroyed, valid()==false, get() supplies the last value, set is ignored, and observe is inactive. No automatic destruction notification; events revalidate valid(). The public KnobComponent constructor preserves label/Binding/range; no float-to-double migration in this extraction.

## 3. State, ownership, and notifications

Owned Binding<float> and local visual value. Subscribe per instance at mounting; each external write updates the clamped value and invalidates rendering.

Current source: value_ receives state_.get() without clamping in the constructor; observe notifications clamp afterward. The arc normalizes during rendering, but text may initially show an out-of-range value. Enhanced contract: initialization and external updates use the same clamped effective value without writeback; user mutations write a clamped value.

Dragging retains its initial value and total displacement. Enhanced contract: an external update during drag cancels the gesture to avoid rewriting from an old starting point. Observations confirming the Knob's expected write do not count as external; this recognition is scoped and restored even if set/observer throws, and a different reentrant value cancels. Test this distinction during enhancement.

## 4. Interactions

PointerDown starts DragGesture and capture. Upward vertical movement increases the value; current sensitivity equals range/180 logical pixels. PointerUp ends the gesture and releases capture.

Left/down arrows decrement and right/up arrows increment, with a step of 1% of the range; Shift uses 0.2%. Other keys/wheel are Ignored; no implicit double-click reset.

ReadOnly consumes mutating events without writing and cancels the current drag. PointerCancel/disabling/unmounting release capture and preserve the last published value, without rollback or added notification.

## 5. Measurement and layout

Historical measurement is 176 × 182. The parent may constrain it; verify clipping and knob geometry in a small space without changing its requested size.

Current rendering reserves space for the title, arcs, and a value with two decimal places. Preserve this normal presentation as an oracle; string measurement never triggers mutation.

Logical coordinates; DPR belongs to the renderer. Normalization must remain finite for every supported range and must never pass NaN to arc/line.

## 6. Presentation and invalidation

Current colors and geometry come from colors: panel, border, accent, knob, and knobInner. Focus replaces the border; no existing KnobStyle.

External/internal value and focus trigger repaint. Range and label are immutable after creation. Extraction does not promise a new animation or hardware effect.

Range maximum <= minimum: preserve maximum = minimum + 1 for ordinary values. Enhanced contract: reject non-finite bounds with invalid_argument; if minimum+1 does not advance in float, use nextafter(minimum,+inf), rejecting a non-finite result. A non-finite source value is displayed as minimum and never produces invalid geometry.

## 7. Accessibility

Target contract: Slider, name = label, numeric_value = effective value, value_range = range with a 1% step, actions Increment/Decrement/SetValue/Focus.

ReadOnly preserves name/value and removes semantic mutations. Actions use the same clamp as the keyboard; no privileged out-of-range path.

The current Knob does not demonstrate these overrides. Verify backend-neutral publication; native T068 bridges are deferred, without claiming native support.

## 8. Lifecycle and recovery

RAII subscriptions are cancelled at unmounting; add an explicit unmount if needed rather than depending solely on destruction. Disabling and exceptions do not leave DragGesture active.

Binding::set may be reentrant: prepare the value and gesture state, write, then revalidate the lifetime token before any component access. A notification that has started is never replayed after an exception.

Destruction is no-throw; do not invent an application end-of-gesture callback. No globals, locks, or audio calls; two simultaneous Knobs remain independent.

## 9. Dependencies and edge cases

Reuses existing Binding/State, DragGesture, and focus/capture. No plugin parameter controller is added to the toolkit.

A wide range may overflow float subtraction: calculate span, delta, and internal normalization in double, then convert to float after clamping, without changing the float API. NaN/inf value: effective minimum presentation with no model correction.

A reentrant State/Binding that removes the node stops every gesture; a new instance does not resume the old drag. Reset/reopen must be able to observe again without retaining the old invalidator.

## 10. Files and compatibility

Target: `include/nativeui/knob.hpp` and `src/knob.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_basic.inc` and `widgets_builders.inc`. Preserve KnobComponent and both builder constructors; put the gesture/rendering core in knob.cpp without an empty wrapper file.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `knob_legacy_delta`: a 180-unit drag and arrows/Shift reproduce the increments.
- `knob_external_drag`: an external write cancels dragging before further movement.
- `knob_readonly_cancel`: a ReadOnly transition and PointerCancel release capture without writing.
- `knob_wide_nonfinite`: extreme inputs produce no non-finite geometry.
- `knob_observer_throw`: after a reentrant observer throws, a new drag remains possible.

Create `examples/features/knob.cpp` and target `nativeui_example_knob`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.