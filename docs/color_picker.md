# ColorPicker

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

An sRGB picker with a saturation/value square, hue, alpha, hexadecimal field, preview, and named palette. The public model remains ui::Color; HSV is only an internal UI representation.

Absent from NativeUI; [geometry.hpp](../include/nativeui/geometry.hpp) defines Color, Painter has gradients, and Slider/TextInput and Binding exist.

MyGo: `ui/colorpicker.go`, `ColorPicker`, `toHSVA`, `hsva.color`, `hexOf`, `pickerState`, `channelSlider`, `checkers`. Hue retention on local grays, external synchronization, alpha/hex, and swatches. MyGo uses uint8; the target retains ui::Color floating-point components.

## 2. Public API and composition

Proposed target API:

```cpp
struct ColorSwatch { std::string id; std::string name; Color value; };
class ColorPicker {
public:
    ColorPicker(std::string label, Binding<Color> value);
    ColorPicker(std::string label, State<Color>& value);
    ColorPicker&& alpha_enabled(bool value = true) &&;
    ColorPicker&& swatches(std::vector<ColorSwatch> value) &&;
    ColorPicker&& on_change(std::function<void(Color)> callback) &&;
    ColorPicker&& style(ColorPickerStyle value) &&;
    Spec spec() &&;
};
```

Defaults: alpha enabled, empty swatches to avoid imposing a palette, SV square then hue/alpha/hex, preferred width 280 logical units. ColorPickerStyle covers square, thumb, checker, text, gaps/padding, and swatch size.

Proposed target example: `ui::ColorPicker{"Accent", accent}.alpha_enabled().spec()` with application-owned State<Color>. All new option/style numbers are double; Color retains its existing type.

## 3. State, ownership, and notifications

External Binding<Color> and immediately converted State overload. Source destruction: valid=false, get returns the last color, set ignored/observe inactive without automatic notification; revalidate before edit and do not notify on_change. Private double HSV, last effective Color, hex draft, and capture belong to the instance. Nonlinear sRGB channels [0,1]; linear alpha [0,1].

HSV is calculated on these sRGB channels, not after gamma conversion. A user transition to gray retains the last useful hue; an external gray color also retains this instance's hue without writing Color.

Each user mutation changes effective Color, then on_change once if distinct; observations of the expected write, recognized by a recoverable scope, do not reset hue. A different external value updates HSV/preview, cancels the gesture, and replaces the draft without a callback. No 8-bit quantization except hex entry/formatting.

## 4. Interactions

SV square: captured click/drag, x=S, y=1-V, clamped to bounds. Right/left arrows ±0.01 S and up/down ±0.01 V; Shift divides the step by 10. Hue/alpha use slider keyboard behavior.

Swatch activated by click/Enter/Space; the hex field accepts exactly #RRGGBB or #RRGGBBAA. Enter/loss of focus commit only if parsing is valid; invalid syntax remains local without modifying Color; Escape restores the last valid hex.

PointerCancel retains the last published color and releases capture. An external change during dragging cancels drag before synchronization. ReadOnly removes mutations but retains reading/navigation; no implicit wheel behavior on the square.

## 5. Measurement and layout

Measurement: 1:1 SV square, two slider rows if alpha is enabled, preview/hex, and swatches in a grid with bounded columns. The parent constrains width; minimum size guarantees usable areas or clipping without overlap.

Calculate SV only with strictly positive width/height; zero forbids mutation rather than dividing. Swatch rectangles and pointer hits follow the calculated grid.

Logical coordinates; alpha checker clips to preview and alpha slider. No exposed framebuffer dimension or screen access.

## 6. Presentation and invalidation

New ColorPickerStyle for surfaces, labels, and focus; the selected color does not become a global theme variable. A light/dark checker makes alpha visible and retains a textual description.

Color changes repaint dependent surfaces and update text/semantics; hue changes gradients without relayout. Swatches/metric style change layout/structure.

Hex formatting = lowercase, #rrggbb if alpha is exactly 1, otherwise #rrggbbaa; byte rounding = round(clamp(channel)*255). Hex formatting must not rewrite the model or lose its fractional float values.

## 7. Accessibility

Target contract: named Group; Custom square describing S and V, Slider sliders for H/alpha with ranges; named Button swatches, TextInput field named “Hexadecimal”. No existing Role::ColorPicker.

Provide descriptive percentage and hex values, not only visual color. Mutating semantic actions share validation/clamping and respect ReadOnly.

T068 bridges are deferred. The hex field uses committed text; IME preedit/candidate rectangles remain a DESIGN17.4 dependency and are not claimed.

## 8. Lifecycle and recovery

UI/main thread; RAII subscription, SV/slider capture owned by the input system. Each update uses a source color snapshot without retaining references to its channels.

Prepare conversion/parsing before writing; Binding/on_change reentrancy may hide/unmount, so do not touch the context or instance after a callback without a valid token.

Exceptions restore capture/guards, then propagate in C++; a started callback is never replayed. No-throw destruction cancels edits/capture and subscriptions without on_change. No OS color service or global mutable state.

## 9. Dependencies and edge cases

Depends on [Slider](slider.md), [TextInput](text_input.md), Button, Painter/gradients, and Binding. The palette is supplied as owned values and stable IDs.

Nonfinite external Color: display invalid with effective fallback color {0,0,0,1}, without writeback; external channels outside [0,1] clamp only for the view. A new gesture writes a finite normalized color.

alpha_enabled=false hides alpha and preserves the source channel during HSV/#RRGGBB changes; #RRGGBBAA is rejected in this mode. Empty/duplicate swatch IDs = invalid_argument before publication; removal of a pressed swatch cancels its action.

## 10. Files and compatibility

Target: `include/nativeui/color_picker.hpp` and `src/color_picker.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Origin to extract or reuse: existing Color/Painter/Binding. ColorSwatch and ColorPickerStyle remain in the parent header; HSV and parser are private in color_picker.cpp, with no Skia or system API.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. No Pugl, Skia, OS, or plugin SDK types belong in this API.

This delivery is documentation only: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests required during implementation:

- `color_picker_srgb_hsv`: primaries and sRGB/HSV round-trip respect float tolerance.
- `color_picker_gray_hue`: loss and restoration of saturation retain hue per instance.
- `color_picker_hex_validation`: exact forms, errors, and hex rounding without writeback.
- `color_picker_alpha_disabled`: the alpha channel retains precision and rejects alpha hex.
- `color_picker_external_drag`: an external update cancels capture without stale rewriting.
- `color_picker_remove_swatch`: a palette changed during a press does not choose another swatch.
- `color_picker_throw_recover`: a throwing callback is not replayed and the next color remains possible.

Create `examples/features/color_picker.cpp` and the `nativeui_example_color_picker` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery after the faults described above under ASan/UBSan where lifetime is involved.

Acceptance: the named tests pass, no capture/registration remains after unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive tests executed.
