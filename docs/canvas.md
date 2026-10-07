# Canvas

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A surface for custom drawing and events in local coordinates. The toolkit remains responsible for routing, clipping, focus, and the backend; Canvas is not a native window.

Builder and public `CanvasComponent` in [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc) and [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). The CanvasContext2D and CanvasInputContext facade is in [component_base.hpp](../include/nativeui/component_base.hpp).

MyGo: `Element.Draw`/`DrawOver` and Painter in `ui/element.go`/`ui/paint.go` are the corresponding primitives, rather than a standalone catalog Canvas. Do not port a Go window/render loop.

## 2. Public API and composition

Existing API to preserve: `Canvas(Size, DrawCallback)` and `Canvas(float width, float height, DrawCallback)`. `DrawCallback = std::function<void(CanvasContext2D&)>`; `InputCallback = std::function<EventResult(const InputEvent&, CanvasInputContext&)>`.

The rvalue template `on_input(Callback&&)` accepts EventResult or void. void is adapted to Handled; any other value is rejected at compile time. `on_input` makes the component focusable; `focusable(bool = true)` can subsequently disable focus. `spec() &&` preserves these decisions.

Verified example using the existing API:

```cpp
auto surface = ui::Canvas{240.0f, 100.0f,
    [](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, g.width(), g.height()},
                    ui::Color{0.1f, 0.2f, 0.3f, 1.0f});
    }}.focusable(false).spec();
```

## 3. State, ownership, and notifications

The builder owns the std::function callbacks and size. Application captures are their owner's responsibility; a reference must live at least until unmounting.

Canvas owns no implicit State and emits no on_change. A mutating input callback chooses its state and explicitly requests invalidate through CanvasInputContext.

The application model is confined to the UI/main thread. A Canvas sharing a Binding with another widget must subscribe within a valid owner; Canvas does not infer this dependency from the lambda.

## 4. Interactions

PointerDown/Move/Up/Wheel and DropOffer/Data are translated by subtracting the bounds origin. Keys and non-positional events pass through unchanged; preserve this current detail.

Without an input callback: Ignored, no automatic focus. With a callback: propagate its result; capture/release, clipboard, and invalidation use the existing CanvasInputContext.

No implicit drag, zoom, scroll, or button. The callback implements and tests its PointerCancel; application confirmation/cancellation belongs to the application. Focusable false removes Canvas from Tab traversal.

## 5. Measurement and layout

Current preferred size: width/height bounded to a minimum of 1 when constructing the component. Preserve this behavior for ordinary values and do not change the float signature.

The parent assigns actual bounds. `g.width()/height()/size()` describe these actual local bounds rather than the requested size. Paint translates to local origin (0,0) and clips the surface.

Fixed enhanced contract: NaN size or size <=1 retains the historical fallback of 1; +inf is rejected with invalid_argument before publication. No invalid transform contaminates a sibling. Measurement does not depend on the draw callback.

## 6. Presentation and invalidation

The lambda composes Painter through CanvasContext2D: existing primitives, text, images, and SVG. No Canvas Theme slot is assumed; the application chooses its colors.

The draw callback must not change the retained structure during paint. Deferred invalidation returns to the normal checkpoint; no recursive rendering.

Wrap clipping and transforms in RAII scopes to balance them if draw throws. The enhancement fixes restoration after exceptions; extraction must preserve pixels on the normal path. These fixes are targets and are not described as already delivered.

## 7. Accessibility

Default target contract: Custom, with name/description supplied by composition; decorative Canvas may remain None. No value or action is assumed.

An application drawing a control must also expose its backend-neutral semantic actions; pixels do not describe its function.

No promise that the T068 bridge is delivered. Drawn text does not open an IME; custom input remains limited to existing committed paths and the DESIGN17.4 preedit dependency.

## 8. Lifecycle and recovery

UI/main thread. PaintContext/InputContext/CanvasContext2D/CanvasInputContext are borrowed only during the call; do not capture them in deferred work.

Callbacks are owned and copied into a snapshot before invocation if a reentrant mutation can replace them. After an exception, restore clip/transform and dispatch guards; a callback that has started is not replayed.

Unmounting cancels capture/focus through Tree without running draw. Destruction is no-throw. A subtree may be removed at the checkpoint; top-level destruction is deferred rather than performed on the active stack.

## 9. Dependencies and edge cases

Depends on existing Component, Painter/CanvasContext2D, Input, and routing. No SkCanvas/Pugl object or new render thread in the public surface.

Empty draw callback: empty surface. Empty input callback: events ignored. Zero/negative Size: historical clamp. Out-of-bounds input may arrive after capture and must keep its unclamped local coordinates.

Drop does not implicitly read files; bytes/MIME are delivered through existing services. Application timers must be cancellable and capture only lifetime-safe identities.

## 10. Files and compatibility

Target: `include/nativeui/canvas.hpp` and `src/canvas.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_builders.inc`, `widgets_basic.inc`, `component_base.hpp`. Preserve public CanvasComponent and the shared context facade through its historical includes; move only Canvas behavior.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `canvas_local_coordinates`: positions and dimensions after parent translation are exact.
- `canvas_input_adapter`: void/Handled/Ignored and explicit focusable preserve their contract.
- `canvas_throw_balanced`: a throwing draw leaves clip and transform intact for a sibling.
- `canvas_capture_teardown`: unmounting during drag makes subsequent events harmless.
- `canvas_headless_primitives`: primitives, images, and SVG remain renderer-independent.

Create `examples/features/canvas.cpp` and target `nativeui_example_canvas`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Reuse `tests/canvas_tests.cpp` and existing headless scenes as a compatibility oracle, then add restoration cases.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.