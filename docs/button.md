# Button

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Trigger a one-time action: saving, confirmation, or a toolbar command. Pressed state is not a persistent value.

NativeUI provides Button, ButtonStyle, and ButtonVisualState in [widgets_button.inc](../include/nativeui/detail/widgets_button.inc). The shared state machine in [widgets_activation.inc](../include/nativeui/detail/widgets_activation.inc) ends the interaction before invoking the callback.

MyGo: `ui/widgets.go`, functions `Button`, `PrimaryButton`, `styleButton`. Its primary variant uses the accent color and accepts children; NativeUI currently draws a label. Target: a primary variant and composed content within the same component.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
Button(std::string label, ActivateCallback on_activate);
Button&& style(ButtonStyle value) &&;
Spec spec() &&;
```

Example using the current API:

```cpp
auto save = ui::Button("Save", [] {}).spec();
```

Target extensions: `enum class ButtonVariant { Standard, Primary, Toolbar };`, `Button&& variant(ButtonVariant) &&`, and `Button&& content(Spec) &&`. Content replaces the visual label; the label remains the accessible name.

ActivateCallback remains std::function<void()>. An absent action leaves a button that can be displayed and activated without effect; no dependency on an audio command or native service.

Composed content children do not add activation targets: the button is a single control; a focusable child is rejected during content validation.

## 3. State, ownership, and notifications

The label, style, content, and callback are owned by the Spec and then the component. No Binding<bool> for a momentary button.

Hover, capture, and keyboard press state are local to the instance. The primary variant does not automatically choose a default action for the entire window.

An action may change application state or remove the subtree. The widget no longer accesses itself after its callback; refreshing belongs to the bindings of affected components.

## 4. Interactions

Pointer: arm at PointerDown and capture; moving outside bounds clears pressed state; PointerUp inside activates once. PointerCancel, disappearance, or loss of focus cancel arming without an action.

Space arms on the first KeyDown and activates at KeyUp; repeats do not multiply the action. Enter activates on the first KeyDown and remains suppressed until KeyUp, following existing behavior.

The wheel and external dragging change nothing. Disabled blocks activation and focus according to inherited availability; ReadOnly preserves existing action semantics because no value is edited.

Toolbar mode is a local presentation; group navigation belongs to Toolbar, which invokes the same callback.

## 5. Measurement and layout

Current measurement: the larger of minimum_width and text measurement plus two horizontal_padding values; height is control_height. Preserve these results for identical styles.

Target content supplies its intrinsic measurement surrounded by button padding; the parent distributes constraints. Space below the minimum clips content without drawing outside the allocated region.

All dimensions are logical; framebuffer scale changes neither hit testing nor the arming threshold. Resizing during capture uses current bounds on release.

## 6. Presentation and invalidation

Preserve ButtonStyle base/hovered/pressed/focused/disabled/read_only and the current resolution order. Primary uses an accent background and a readable theme color; Toolbar is transparent at rest.

Visible focus and pressed state are independent; the ring does not disappear under a pressed patch. Font metrics, padding, and minimum require layout; colors alone require paint.

Custom content uses shared painting services and inherited availability; no permanent animation or Tick loop is added.

## 7. Accessibility

Target: SemanticRole::Button, name from label, Activate action when enabled; a momentary button has no checked state.

ButtonComponent does not currently publish a semantics override in its source; test this enhancement, including icon content that replaces the visual label.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Closing the overflow menu or a Dialog that invokes Button must commit its terminal state before the action. Cancellation and destruction never trigger Activate.

If the action throws after changing a State, the button remains disarmed and the next click works; do not attempt application rollback or automatic retry.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: Component, ThemeBinding, TextService, styles, and PressActivationState. Reuse the latter for Link/ToggleButton; do not create a second focus manager.

An empty label is allowed for icon content, but require a non-empty semantic name in the accessible example. Very long text, invalid UTF-8, and font substitution follow TextService rules.

If content(Spec) fails during mounting, no partial child remains published; retain the previous button until the reconciliation checkpoint.

## 10. Files and compatibility

Target: `include/nativeui/button.hpp` and `src/button.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Extract the currently inline core from widgets_button.inc; ButtonVisualState and ActivateCallback remain available through historical includes.

The new variant/content builders extend signatures without changing Button(label, callback) or ButtonStyle patches. MyGo's PrimaryButton variant does not create an additional C++ file.

Register `src/button.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`button_activation`: an inside click and a Space pair produce exactly one action; repeated Enter is filtered.

`button_cancel`: leaving bounds, PointerCancel, and disabling trigger nothing; a new activation succeeds.

`button_layout_style`: color alone invalidates paint, font/padding invalidate layout; content remains clipped.

`button_reentrant_throw`: an action removes its button or throws, then the next click on another button works.

`button_primary_semantics`: Primary preserves name, Activate, and contrast; icon content remains a single control.

Add `examples/features/button.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.