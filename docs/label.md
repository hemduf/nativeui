# Label

**Status: existing — extraction required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Display an immutable string, without editing or actions. The main uses are control labels and short status text.

Available in [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc): `Label(std::string)`, alias `TextLabel`, and the public `LabelComponent` in [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). Measurement uses `TextService`; painting is vertically centered with horizontal alignment.

MyGo: `ui/widgets.go`, `Text` and `Textf`, at the same reference version. MyGo automatically wraps lines to the available width; the current Label measures a string without paragraph layout. [RichText](rich_text.md) defines the new paragraph contract.

## 2. Public API and composition

Existing API to preserve exactly: constructor by value; rvalue fluent methods `size(float)`, `color(Color)`, `align(TextAlign)`, `weight(FontWeight)`, `bold(bool = true)`, `slant(FontSlant)`, `italic(bool = true)`, `family(std::string)`, `fallback_families(std::vector<std::string>)`, `style(TextStyle)`, and `spec() &&`.

Verified example using the existing API:

```cpp
auto caption = ui::Label{"Level"}
    .size(14.0f)
    .bold()
    .align(ui::TextAlign::Left)
    .spec();
```

No text Binding is added in this extraction. Reactive text is rebuilt through existing application composition; do not invent an implicit observer on a copied string.

## 3. State, ownership, and notifications

The builder owns the string and `TextStyle`, including the family and fallbacks. Their values are moved into the component; no application `string_view` is retained.

Text is immutable after mounting. There is no change callback and no write to an application model.

Identity belongs to the retained node. Rebuilding content must follow the existing reconciliation mechanism and must not change the parent key by default.

## 4. Interactions

No focus, capture, active hover, dragging, or selection. Pointer, wheel, keyboard, and text events are ignored to allow routing to parents.

Confirmation and cancellation do not apply. A label rendered inside a button does not replace the button's activation target.

There are no accessibility actions. Disabled mode does not install additional interactions.

## 5. Measurement and layout

All dimensions are logical. `TextService::measure(text, style)` supplies width and height; painting uses exactly the same UTF-8 repair and fonts.

Alignment moves the anchor within the allocated space without changing intrinsic measurement. The parent constraint and clipping still determine the result when text is too long.

Preserve current behavior without silently adding wrapping or ellipsis. Zero height or reduced width must not produce non-finite geometry.

## 6. Presentation and invalidation

`TextStyle` remains the public style. `size` retains its clamp to zero; `bold(false)` and `italic(false)` restore Regular/Upright values.

An explicit style color takes precedence; do not promise a Theme subscription absent from the current component. New theme properties await a separate enhancement.

Headless rendering: choose a deterministic test font, use the same text as measurement, and place the vertical anchor at the center of the allocated space. No timer or animation.

## 7. Accessibility

Target contract: `SemanticRole::Text`, with owned text exposed as a readable value; no action or focus. Repeated decorations may be excluded by composition.

The `Component::semantics` hook and role exist; the current component does not guarantee their publication. Extraction must add or preserve publication according to the toolkit's semantic contract.

Native VoiceOver/UIA/AT-SPI bridges are deferred to T068; inspecting a headless snapshot does not prove they work.

## 8. Lifecycle and recovery

UI/main thread only; immutable text requires no per-instance registration.

A failed allocation or font measurement does not publish a partial component. Any measurement caches are private and do not become a global mutable registry.

Destruction is no-throw; no application callback. Do not retain PaintContext or Painter outside paint; font resources follow their existing ownership.

## 9. Dependencies and edge cases

Depends on the existing Component, TextService, and Painter; no additional file loader or native text API.

Empty string: zero width and text height according to TextService. Invalid UTF-8: the same U+FFFD replacements in measurement and painting, without changing application input.

Missing family: use the existing fallback. Zero size, long strings, combining accents, and non-Latin characters are explicit scenarios.

## 10. Files and compatibility

Target: `include/nativeui/label.hpp` and `src/label.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_builders.inc` and `widgets_basic.inc`. Preserve `TextLabel` and the public `LabelComponent` class, its signature, and current style methods.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `label_utf8_measure_paint`: measurement and pixels agree for valid and invalid Unicode.
- `label_alignment`: all three alignments preserve intrinsic size.
- `label_empty_zero`: empty text and zero size cause neither errors nor invalid geometry.
- `label_legacy_alias`: TextLabel and all existing fluent methods compile.

Create `examples/features/label.cpp` and target `nativeui_example_label`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Extend the existing evidence in `tests/label_tests.cpp` and `examples/features/t026_label.cpp` without removing their assertions.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.