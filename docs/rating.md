# Rating

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Assign a star rating from zero to the maximum count. One numeric value, one Tab stop, hover preview without writing.

NativeUI provides no Rating in [widgets.hpp](../include/nativeui/widgets.hpp); PaintContext/Path and sliders supply rendering and value primitives.

MyGo: `ui/indicators.go`, `Rating`, `starPath`. Integer rating, with another click on the selected rating resetting to zero; the target preserves this default and offers optional double granularity.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class Rating {
public:
  Rating(std::string label, Binding<double> value, std::size_t stars = 5);
  Rating(std::string label, State<double>& value, std::size_t stars = 5);
  Rating&& step(double value) &&;
  Rating&& clearable(bool value = true) &&;
  Rating&& style(RatingStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<double> score{3.0};
auto rating = ui::Rating("Rating", score, 5).step(1.0).clearable().spec();
```

Defaults: five stars, step=1, clearable=true. Accept only step=1 or step=0.5 for defined fractional geometry; max=stars is the full count.

Target RatingStyle: star size/gap, empty/full/preview/disabled colors, outline, and ring. Stars are painted control subparts rather than public components with their own files.

## 3. State, ownership, and notifications

Binding<double> holds the rating; stars/step/style are owned. Effective rendering value is clamp[0,stars], NaN/inf=0, with no silent mount-time rewrite.

User rating is quantized to the chosen step: clicking a star chooses its integer count; with step=0.5, its left half chooses i+0.5 and right half i+1.

hover_value is local and temporary; PointerUp publishes only the targeted value. clearable=true and a target exactly equal to effective rating reset to zero. External writes do not turn the rating into preview.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

PointerMove shows preview; Down arms the target and captures; Up inside confirms the current target; Cancel/release outside remove preview without mutation.

Right/Up increase by step, Left/Down decrease; Home=0, End=stars. Tab has one stop; internal star shapes never request focus.

ReadOnly allows reading/focus without a preview promising mutation or an action. Disabled receives no editing. Wheel ignored; no continuing drag outside.

Escape cancels a press/preview without rollback; the last value published by a keyboard command remains authoritative.

## 5. Measurement and layout

Width=stars*star_size + max(stars-1,0)*gap; height is star_size plus ring. Each star's hit region is its rectangle rather than only its opaque path.

Width larger than intrinsic size leaves stars aligned at the start; a narrower parent clips without redefining their values according to the viewport.

stars=0 produces an empty non-focusable indicator without actions. Size/gap must be finite>=0; all coordinates are logical.

## 6. Presentation and invalidation

Immutable reusable star path, filled according to effective value; half-stars use balanced local horizontal clipping.

Preview uses a distinct color and does not destroy checked value in the model. Focus ring around the actually drawn star group.

Value/hover change paint alone. stars/size/gap invalidate layout. No shader/raster requirement in the public API or per-star path allocation on each event.

## 7. Accessibility

Target: Slider, name=label, numeric_value, range[0,stars] with step, Increment/Decrement/SetValue actions if mutable; optional “3 out of 5” text computed in the UI snapshot.

Individual stars have Role None: avoid N duplicate focus/actions. ReadOnly preserves value and removes actions; stars=0 does not advertise an adjustable slider.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Commit ends capture and clears preview before Binding.set. If an observer removes Rating, reread no path/instance afterward.

Callback/observer exception: committed value remains, preview cancelled, next interaction available. Unmounting invokes no clear action.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: Binding, PaintContext/Path, focus, and theme. Star count belongs to Rating; no separate Star component.

Invalid step rejects Spec before publication; an out-of-range external value renders at a bound without correction. A stars change after reconstruction does not reset State to zero.

Preserve a readable palette in dark themes; zero-size stars do not become editing targets.

## 10. Files and compatibility

Target: `include/nativeui/rating.hpp` and `src/rating.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

RatingStyle and internal preview value belong to the Rating file pair; rating.cpp contains quantization/hit testing, capture, geometry, and a shared immutable star path.

Register `src/rating.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`rating_click_clear`: a click chooses a rating; a second identical choice resets to zero if clearable.

`rating_half`: step=0.5 distinguishes left/right halves; step=1 produces only integer ratings.

`rating_preview`: hover/Cancel publish no value; resuming displays the current model.

`rating_keys`: arrows/Home/End respect bounds and step, with one Tab stop.

`rating_external_zero`: NaN/out-of-range render without writing; stars=0 is non-editable.

`rating_remove_throw`: observer removes/throws and half-star clipping recovers; next input/frame valid.

Add `examples/features/rating.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.