# Tooltip

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Text decorator for an anchor, presented while the pointer is idle or on focus without capturing input/focus. No interactive content; an interactive panel uses Popover.

Present in [tooltip.hpp](../include/nativeui/tooltip.hpp): the Tooltip builder, detail::TooltipController and TooltipComponent. The controller owns a Dispatcher timer and a NonModal/Ignore/Auto overlay.

MyGo: `ui/widgets.go`, `Element.Tooltip`. A 600 ms delay, panel near the pointer, accessible description and innermost tooltip. NativeUI already has its own 500 ms delay, NodeId anchor and focus trigger; retain these choices rather than copying Go's global timing.

## 2. Public API and composition

Exact existing API: `template<class Child> Tooltip(std::string text, Child&& child)`, `delay(milliseconds) &&`, `delay(milliseconds) &`, getter `delay() const noexcept`, getter `text() const noexcept`, `spec() &&`, constants kDefaultDelay=500ms and kDefaultMaxWidth.

Verified existing example:

```cpp
auto help = ui::Tooltip{"Save the document",
    ui::Button{"Save", [] {}}}
    .delay(std::chrono::milliseconds{500})
    .spec();
```

Proposed target addition: `Tooltip&& style(TooltipStyle value) &&`; a new type with background/text/border/radius/padding/max_width and TextStyle. Preserve both ref-qualified delay overloads and their clamping of negative values to zero.

## 3. State, ownership and notifications

Owned text/child Spec/style, without a text Binding in this version. The controller owns hover/focus/available/suppressed/pointer_active and the timer; one specific overlay handle per anchor.

Eligibility = an available anchor and (hover or focus) and no pointer interaction. Visible and pending remain distinct; the delay starts only at a real eligibility transition.

PointerDown or loss of availability suppresses a continuous presentation: regaining availability under a stationary pointer does not restart it. A false then true transition starts the full delay again. No shown/hidden user callback is added.

## 4. Interactions

Tooltip observes anchor hover/focus without managing activation. Tab/keyboard belong to the child; the Ignore panel is neither targetable nor focusable.

PointerDown dismisses the tooltip; any gesture in the tree suppresses hover eligibility. Ending the gesture alone does not restart it under a stationary pointer. Focus alone may arm the delay under the current controller.

The source sets dismiss_on_escape=false and dismiss_on_outside_pointer_down=false: Tooltip does not intercept Escape, which follows anchor routing. PointerDown dismissal uses its controller rather than a second outside policy. No dragging, wheel interaction or validation in the panel.

## 5. Measurement and layout

The decorator measures and places its child as before. The text panel wraps through `wrap_tooltip_text`, with padding and maximum width; sizes use logical coordinates.

Overlay Auto from NodeId; placement/clamping are handled entirely by the existing service. No reserved space or copied OS screen coordinates.

Very long text without spaces: wrap at UTF-8 boundaries according to the helper; v1 does not claim multi-run layout. A smaller viewport clips through the services without producing negative size.

## 6. Presentation and invalidation

The new TooltipStyle exposes fields currently private to TooltipSurfaceComponent; defaults preserve the exact palette and metrics. No new Theme slot is claimed to have shipped.

Metric style/text width = panel layout; colors = paint. The anchor does not remount on theme changes or tooltip closure.

Empty text: no presentation. No default animation/fade or process-global warmup/current-tooltip. One Tooltip's style does not affect subsequent instances.

## 7. Accessibility

Target contract: populate the anchor description from the text if its explicit description is empty; do not overwrite the application's description. The decorative panel creates neither focus nor duplicate reading.

SemanticRole::Tooltip is absent; use Group/Text in the snapshot if publishing the panel, or the anchor description alone under the existing exclusion contract. The default chosen here is the anchor description with the panel excluded.

Native T068 bridges are deferred. No IME/editing. Text displayed in a tooltip remains owned even after the overlay disappears visually.

## 8. Lifecycle and recovery

UI/main thread; availability subscription/observation and timer are canceled on unmount. Controller and handlers are per instance and weakly protected; a stale deferred invocation is a no-op.

Failed schedule_after never presents synchronously. Failed show restores visible=false; failed hide preserves the handle/closure-recovery state under the current transaction.

No-throw destruction, shutdown without application callbacks. Busy/suppressed scopes remain consistent after exceptions; a started integration callback is not replayed. The next false/true cycle remains usable.

## 9. Dependencies and edge cases

Depends on existing DispatcherProvider/Dispatcher, Overlay and retained focus/hover. No new timing service, OS tooltip or overlay stack.

Without a valid Dispatcher, preserve a normal anchor and no tooltip; no thread/sleep fallback. Negative delay clamps to zero; empty text is ineligible.

Hidden/Collapsed/Disabled anchors remove pending/visible presentations; read-only remains eligible for information. UI deactivation closes and leaves observation consistent. Nested tooltips do not share a global “current” variable.

## 10. Files and compatibility

Target: `include/nativeui/tooltip.hpp` and `src/tooltip.cpp`. The header contains public declarations; the `.cpp` contains a real retained kernel, measurement, layout, applicable events and rendering.

Source to extract or reuse: existing `tooltip.hpp`. Preserve constants/getters/ref-qualifiers/the child template and historical includes; the actual controller and surface belong in tooltip.cpp. Keep the controller test seam or adapt its visibility without breaking consumers.

Essential template adapters stay in the header and delegate to the non-template kernel. Preserve historical includes through their collective headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. This API exposes no Pugl, Skia, OS or plugin SDK types.

This delivery is documentation: no extraction or CMake change is performed in this documentation phase.

## 11. Tests and acceptance criteria

Tests to implement with the component:

- `tooltip_legacy_delay`: the 500 ms default and both delay overloads retain their results.
- `tooltip_pointer_suppression`: PointerDown/release under a stationary pointer does not rearm.
- `tooltip_availability`: Hidden/Disabled followed by return under a stationary pointer does not rearm.
- `tooltip_style_wrap`: maximum width/padding/wrapping affect measurement and pixels together.
- `tooltip_description`: preserve an explicit description and avoid duplicate reading.
- `tooltip_timer_fault`: timer/show/hide failure followed by the next cycle remains recoverable.

Create `examples/features/tooltip.cpp` and the `nativeui_example_tooltip` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, works headlessly and returns a nonzero code on the first failure.

Reuse `tests/t062_tooltip_tests.cpp` and `examples/features/t062_tooltip.cpp`; enhance style/semantics without rewriting current suppression scenarios.

Acceptance: all named tests pass, no capture/registration survives unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive test was executed for this specification.
