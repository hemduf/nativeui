# ColorWell

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A compact color sample opening a portable anchored ColorPicker. Public sRGB ui::Color value; no system color dialog or global service.

Absent from NativeUI; Color, Button, and [Overlay](../include/nativeui/overlay.hpp) exist. [ColorPicker](color_picker.md) and [Popover](popover.md) are target dependencies.

MyGo: `ui/colorpicker.go`, `ColorWell`, preview on checker, hex value, Enter/Space/click opening, and Escape/outside closing. The picker color is published while open.

## 2. Public API and composition

Proposed target API:

```cpp
class ColorWell {
public:
    ColorWell(std::string label, Binding<Color> value);
    ColorWell(std::string label, State<Color>& value);
    ColorWell&& alpha_enabled(bool value = true) &&;
    ColorWell&& swatches(std::vector<ColorSwatch> value) &&;
    ColorWell&& on_change(std::function<void(Color)> callback) &&;
    ColorWell&& style(ColorWellStyle value) &&;
    Spec spec() &&;
};
```

Defaults: alpha enabled, empty palette, ColorPicker popup natural width 280 DIP and preview 36 × 20 DIP plus padding. ColorWellStyle covers preview/chrome/focus/size, rather than global picker preferences.

Proposed target example: `ui::ColorWell{"Track color", trackColor}.spec()`. ColorSwatch is defined in color_picker.hpp and reused, not redefined.

## 3. State, ownership, and notifications

One Binding<Color> shared between preview and ColorPicker; private open/handle. The State overload is converted to Binding. Source destruction: valid false, get returns the last value, set ignored and observe inactive without automatic notification; revalidate before opening/edit and do not emit on_change for an ignored set.

Picker mutations are notified once through ColorWell.on_change; no second subscription reemits every update. External changes repaint preview/picker but do not notify on_change.

Values publish continuously; closing the popup does not restore the initial color. Escape cancellation concerns opening/the hex draft, not a Color transaction rollback.

## 4. Interactions

Completed primary click, Enter, and Space toggle the popup once per press. Focus goes to the picker's first control; normal closing returns focus to the still-available anchor.

Escape/outside click close it. PointerCancel cancels a pressed trigger; drag/wheel on the preview are ignored. Dragging in the picker follows its capture rules.

ReadOnly/Disabled prevent mutating opening; a ReadOnly/Disabled/Hidden transition while open closes it and cancels capture. No implicit double-click reset or color drag/drop.

## 5. Measurement and layout

Rectangular preview in chrome with style dimensions and preserved ratio. Rounded clipping includes checker and fill; zero bounds accept no input.

Popup anchored by NodeId, Overlay service Auto and clamping, natural picker size independent of preview. The parent moves/scrolls the anchor without OS coordinates.

Viewport smaller than picker: bound/clip its content under Popover/Calendar conventions without enlarging the window or overflowing interactions.

## 6. Presentation and invalidation

ColorWellStyle: border, focus, padding/radius; preview draws checker then view-normalized Color. Alpha does not reduce border/chrome color.

Color update = repaint/semantics; metric style = layout; open/close = Overlay structure. No default-color flash before the first Binding read.

An invalid external color is signaled textually and rendered with the picker's fallback; preserve the source. No mandatory animation or new theme palette.

## 7. Accessibility

Target contract: Button named by label, color hex description/value, expanded, Activate/Focus. SemanticRole::ColorWell is absent; do not claim it is delivered.

Open content carries the field name and channel names; semantic actions use picker rules. Preview does not generate an unnecessary second Image.

T068 bridges are deferred. The only text entry is the picker's hex field: current committed text, future native preedit under DESIGN17.4.

## 8. Lifecycle and recovery

UI/main thread, RAII subscription, popup callback protected by token/generation. Unmounting removes only its own overlay and does not close another ColorWell's overlay.

Show/panel failure leaves open false and the next click usable. Idempotent closing; focus restoration only if the anchor is valid.

Reentrant on_change may reopen another popup; exact handle/generation prevents closing the new one. A started callback is never replayed; no-throw destruction, no improvised application cancellation callback.

## 9. Dependencies and edge cases

Depends on ColorPicker/Popover/Binding and Button chrome. No audio, filesystem, or OS color-panel calls.

Disabled alpha preserves the source channel under ColorPicker rules. An external change during dragging cancels the picker's gesture; popup closing with invalid hex discards it without a new color.

Empty palette valid; duplicate IDs rejected by the picker. A removed Binding source becomes nonmutating at the next revalidation; removed anchor = closing, not relocation to the center.

## 10. Files and compatibility

Target: `include/nativeui/color_well.hpp` and `src/color_well.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Origin to extract or reuse: existing Color/Overlay and target ColorPicker. Import ColorSwatch from its parent; the well core owns opening/preview, while HSV/parser remain exclusive to the picker.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. No Pugl, Skia, OS, or plugin SDK types belong in this API.

This delivery is documentation only: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests required during implementation:

- `color_well_live_color`: preview and picker share one value with a single notification.
- `color_well_escape_keeps`: Escape closing preserves the last validated color.
- `color_well_focus_anchor`: open/close and removed anchor never focus a stale node.
- `color_well_alpha_preview`: zero/partial alpha leaves the checker visible.
- `color_well_two_instances`: two simultaneous wells isolate handles, hue, and subscriptions.
- `color_well_show_failure`: opening failure then a new click work.

Create `examples/features/color_well.cpp` and the `nativeui_example_color_well` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery after the faults described above under ASan/UBSan where lifetime is involved.

Acceptance: the named tests pass, no capture/registration remains after unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive tests executed.
