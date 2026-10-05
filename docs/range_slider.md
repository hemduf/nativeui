# RangeSlider

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit a low/high interval within a domain: frequency cutoffs or a range. One RangeValue publication preserves low<=high for a gesture.

NativeUI: [widgets_range_slider.inc](../include/nativeui/detail/widgets_range_slider.inc), RangeValue {float low, float high}, RangeSlider, and active thumb. The domain is defined in [slider_value.hpp](../include/nativeui/detail/slider_value.hpp).

MyGo: `ui/indicators.go`, `RangeSlider`; MyGo uses two float64 pointers. NativeUI already has an aggregate binding; preserve this difference, which avoids two separate notifications. Target: complete keyboard access to both handles.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
struct RangeValue { float low{}; float high{1.0f}; };
explicit RangeSlider(Binding<RangeValue> state);
explicit RangeSlider(State<RangeValue>& state);
RangeSlider&& range(float minimum, float maximum) &&;
RangeSlider&& step(float value) &&;
RangeSlider&& orientation(SliderOrientation value) &&;
RangeSlider&& style(SliderStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<ui::RangeValue> band{ui::RangeValue{0.2f, 0.8f}};
auto range = ui::RangeSlider(band).range(0.0f, 1.0f).step(0.05f).spec();
```

Existing RangeValue::operator== remains unchanged. The keyboard variant adds Enter to change the active handle without writing Binding; it adds neither a second public component nor changes to constructors.

Preserved defaults: range[0,1], step=0, horizontal orientation; public float numerics remain stable.

## 3. State, ownership, and notifications

Binding<RangeValue> owns both bounds; active handle is local state. Non-finite external low/high use minimum/maximum respectively, then clamp and swap for geometry alone, without set.

An interaction normalizes the chosen handle; lower does not exceed effective high, upper does not fall below effective low. set publishes an entire RangeValue only when different.

Pointer choice uses distance to the continuous target before snapping; a tie chooses the previous active handle, otherwise lower. Do not use the step grid to determine handle identity.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

PointerDown chooses the nearest handle and publishes live; Move/Up follow that handle with capture. Handles do not cross: they stop against the other bound.

Existing keyboard behavior: arrows and Home/End change the active handle value without changing identity (lower by default); Home lower=min/upper=low, End lower=high/upper=max. Increments follow Slider (step or span/100, Shift span/1000).

Target extension: Enter toggles the active handle and filters auto-repeat until KeyUp without changing the value; announce the active handle. Tab remains one stop on RangeSlider, then exits.

Cancel ends dragging without rolling back the live value. ReadOnly blocks setters but allows handle choice/focus; wheel ignored. Disabled cancels interaction without inventing an initial interval.

## 5. Measurement and layout

Same SliderTrackAxis and insets for paint/hit testing; vertical orientation reverses the fraction like Slider. Paint the selected segment between effective low/high.

Both thumbs retain fixed size and may overlap when low==high. Current source paints lower then upper and draws a focus ring on both; enhanced target: paint the active handle last and reserve its active indication so it remains readable. The next tied click preserves active identity.

Zero-length axis constraints are handled without division by zero. Clipping bounds and pointer calculations remain in logical coordinates during resize and scale changes.

## 6. Presentation and invalidation

Shared SliderStyle, with visual_state for the control and active-handle/focus accent. RangeValue changes require only repaint when metrics are identical.

Handle selection through Enter repaints the active thumb ring/indicator rather than layout; handle sizes do not differ.

No double Binding or audio callbacks; the view always reflects an accepted pair snapshot.

## 7. Accessibility

Target: Group with two RangeSliderHandle children named “Minimum”/“Maximum”, numeric_value, range adjusted by the other bound, and Increment/Decrement/SetValue actions.

Semantic children have stable identities throughout RangeSlider lifetime; Focus/Select chooses the handle. Current source publishes neither these semantics nor two children: an enhancement is required.

Special ListView virtualization is not reused for two handles: create two internal retained subparts within the same range_slider.hpp/cpp pair, with simple backend-neutral semantic nodes and no thumb pointers. No VirtualSemanticChildren extension is needed for this control.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare the pair and end capture before State.set; if an observer removes the widget, do not reread handle/component after notification.

After an observer exception, the published pair remains authoritative and the active handle remains valid or resets if unmounted; no partial restoration of low alone.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [slider](slider.md), SliderDomain, slider_track_axis, State/ThemeBinding; semantic support in shared services.

Range min<max finite, step>=0 finite, as in Slider. Reversed/NaN/out-of-range external pair: safe rendering without automatically correcting the application.

A new external write during drag is reread as the current pair; the chosen handle keeps its identity. If the other bound moves, the next value is constrained against it without a hidden gesture.

## 10. Files and compatibility

Target: `include/nativeui/range_slider.hpp` and `src/range_slider.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Preserve RangeValue, RangeSlider, SliderStyle/Orientation, and historical includes; declarations in range_slider.hpp, non-template core/painting/input in range_slider.cpp.

Keyboard additions and semantic children neither reshape public State nor create one .cpp per handle.

Register `src/range_slider.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`range_slider_pair`: every write preserves low<=high; a single pair notification.

`range_slider_nearest`: continuous distance chooses the handle; ties retain active identity before snapping.

`range_slider_keyboard_handles`: Enter changes active handle, arrows/Home/End edit correct bounds; Tab exits.

`range_slider_external_invalid`: swap/clamp/render external values without set at mount/paint.

`range_slider_cross_cancel`: handles do not cross; Cancel does not roll back the live pair.

`range_slider_semantic_ids`: two stable IDs and actions adjusted to the other bound.

`range_slider_remove_throw`: a reentrant/throwing observer leaves no capture or stale handle.

Add `examples/features/range_slider.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.