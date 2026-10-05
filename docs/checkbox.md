# Checkbox

**Status: existing — extraction required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Edit an independent boolean option, with mark and label clickable as a single target.

NativeUI: [widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc), Checkbox, CheckboxStyle, and detail::CheckboxComponent core. It observes Binding to classify layout/paint precisely and uses PressActivationState.

MyGo: `ui/widgets.go`, `Checkbox`; `ui/base.go`, `CheckboxBase`. The simple bool is already covered; mixed state belongs to CheckboxGroup and does not replace Checkbox's bool signature.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
Checkbox(Binding<bool> state, std::string label);
Checkbox(State<bool>& state, std::string label);
Checkbox&& style(CheckboxStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
ui::State<bool> notifications{false};
auto checkbox = ui::Checkbox(notifications, "Notifications").spec();
```

Respect the current state-then-label order, unlike Toggle(label, state). Do not introduce a second internal value that could diverge from Binding.

Existing CheckboxStyle and CheckboxStylePatch remain available through style.hpp and widgets.hpp; extraction does not impose a new options model.

## 3. State, ownership, and notifications

Binding<bool> owns the choice's source of truth; checked state is reread from Binding. The pointer/Space session retains only arming, not a bool snapshot.

At commit, copy Binding and publish !state.get(). An external write during the press is therefore taken into account; release does not restore an old value.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

PointerDown arms and captures; PointerUp inside toggles once; leaving and releasing outside, and PointerCancel, publish no mutation. The label is part of the target.

Space toggles at KeyUp after the first KeyDown; repeats do not multiply it. Enter is not a selection key in the current API: let the event follow parent routing.

Tab and availability use shared focus; ReadOnly remains focusable but consumes mutating input and cancels already armed capture. Disabled receives no activation.

Wheel has no effect; no automatic drag-reorder or context menu. A group of checkboxes does not impose exclusivity.

## 5. Measurement and layout

Current measurement: max(minimum_width, text + leading_padding + box_size + label_gap + 5), height control_height. Preserve this geometry during extraction.

Box vertically centered; label placed after box_size and label_gap. Use logical component bounds for hit testing and text clipping.

A checked box does not grow unless a checked patch explicitly changes a metric. Narrow parent: preserve a readable indicator and clip text without affecting the value.

## 6. Presentation and invalidation

Preserve CheckboxStyle: base/checked/hovered/pressed/focused/disabled/read_only and resolution order. Checkmark visible only when state is true and metrics/color permit painting.

Invalidation classification accounts for a checked change revealing the mark even when styles are identical. A checked patch changing box_size must also remeasure.

The ring remains visible for keyboard use; local style does not mutate Theme. Two boxes sharing a Binding can be checked together without sharing hover/capture.

## 7. Accessibility

Target: Checkbox, name=label, checked Checked/Unchecked, Toggle and Focus actions when available. ReadOnly removes Toggle but preserves checked.

In the reviewed file, detail::CheckboxComponent currently provides no semantics override; publishing this contract through Component hooks is required for complete extraction.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Observation already uses an invalidation object detached from the component with an active flag; preserve this protection against a preceding observer removing the node during a pass.

unmount disables classification and then removes the subscription. Activation completes context operations before state.set; no this access after publication.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: State, TextService, ThemeBinding, and PressActivationState; [checkbox_group](checkbox_group.md) composes independent bindings without changing Checkbox.

Binding with an expired State: preserve existing Binding safety rather than turning the control into a raw pointer. Empty label allowed with a semantic name supplied by composition.

One external bool value, with no invented mixed state. Form content must not reset the value at mounting or through focus.

## 10. Files and compatibility

Target: `include/nativeui/checkbox.hpp` and `src/checkbox.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Move the bool implementation out of widgets_checkbox_radio.inc; preserve Checkbox and styles, then import checkbox.hpp through historical headers.

detail::CheckboxComponent remains private; no predefined template instantiation or replacement of State<bool> is required.

Register `src/checkbox.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`checkbox_release`: clicks and Space write once on release; Enter leaves bool unchanged.

`checkbox_cancel_read_only`: cancellation or ReadOnly during a press publishes nothing; capture released.

`checkbox_external_value`: an external value changed during a press becomes the commit basis.

`checkbox_style_classifier`: mark, color, and box_size produce appropriate invalidations.

`checkbox_remove_listener`: a preceding observer removes the node; detached classification remains safe.

`checkbox_observer_throw`: State recovers after an exception; a later click is possible, with no retry.

Add `examples/features/checkbox.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.