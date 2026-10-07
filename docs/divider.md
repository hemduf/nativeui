# Divider

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A horizontal or vertical visual separator, distinct from SplitView handles. It contributes thickness to layout and stretches along its main axis.

No Divider builder in NativeUI; [Header](header.md) currently draws an internal line. Line/rectangle primitives exist in [component_base.hpp](../include/nativeui/component_base.hpp).

MyGo: `ui/widgets.go`, `Divider`, infers orientation from the parent row and sets width or height to 1 DIP. The port chooses an explicit, deterministic orientation without hidden parent inspection.

## 2. Public API and composition

Proposed target API:

```cpp
enum class DividerOrientation { Horizontal, Vertical };
class Divider {
public:
    explicit Divider(DividerOrientation value = DividerOrientation::Horizontal);
    Divider&& thickness(double value) &&;
    Divider&& color(Color value) &&;
    Spec spec() &&;
};
```

Default thickness is 1 logical unit; default color is the theme border color. thickness requires a finite value >= 0; invalid input causes invalid_argument before publication.

Proposed target example: `ui::Divider{ui::DividerOrientation::Vertical}.thickness(1.0).spec()`. The application must choose vertical orientation inside a Row.

## 3. State, ownership, and notifications

Owned options; no mutable application state, Binding, or callback. Identity follows the retained node.

Explicit color is distinct from the themed default color. Only the instance's Theme subscription may update an implicit color.

The separator communicates neither a value nor a drag position. No change event.

## 4. Interactions

No focus, capture, hover, wheel, drag, keyboard, or text; all inputs are Ignored.

Confirmation/cancellation do not apply. Vertical orientation does not provide resize behavior.

An overlap with a Button leaves activation to the Button; Divider must not obscure an interactive input target.

## 5. Measurement and layout

Horizontal: preferred {0, thickness}, with minimum thickness bounded by the constraint. Vertical: preferred {thickness, 0}. The parent supplies the final length.

The main axis stretches within the allocated space; maximum thickness is bounded to the available cross-axis dimension. Zero thickness paints nothing and must not require an artificial size.

Draw in logical coordinates without rounding retained space to DPR. Never paint beyond bounds in a tiny window.

## 6. Presentation and invalidation

Border-colored rectangle/line; no animation, focus ring, hover, or implicit accent. Zero alpha preserves geometry without pixels.

An implicit Theme change invalidates paint; changing thickness or orientation through reconstruction invalidates layout and paint.

No mandatory DividerStyle for three properties; if extended, the type belongs to the Divider file pair and must not invent an existing Theme slot.

## 7. Accessibility

Decorative separator: SemanticRole::None. Do not invent Role::Separator, which is absent from the current contract.

If the application needs to indicate a functional separation, name its Groups around the Divider; the line does not replace this structure.

Native T068 bridges are deferred. No text/IME or action.

## 8. Lifecycle and recovery

UI/main thread; private RAII Theme subscription for implicit color. It ends at unmounting.

Failed construction leaves no subscription. A stale invalidator is lifetime-safe; no lambda captures a raw node.

Destruction is no-throw, with no application notification. Two instances may choose independent thicknesses/colors.

## 9. Dependencies and edge cases

Reuses Component and Painter. No dependency on Pugl, public Skia, or the parent Row.

Zero dimensions, unbounded constraints, and transparent alpha are allowed. NaN/inf or negative thickness are rejected by the new API.

Several successive Dividers are valid; no line collapsing or implicit gap.

## 10. Files and compatibility

Target: `include/nativeui/divider.hpp` and `src/divider.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: existing Painter primitives; new behavior. Header preserves its historical separator or explicitly reuses this core without circular component dependencies.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `divider_orientation`: horizontal/vertical sizes and pixels are consistent.
- `divider_zero_narrow`: zero thickness and insufficient space do not overflow.
- `divider_pass_through`: no input or focus is captured.
- `divider_theme_isolation`: implicit color changes only within its own UI.

Create `examples/features/divider.cpp` and target `nativeui_example_divider`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.