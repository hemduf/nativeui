# Header

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Display a panel header with a title, subtitle, and separator. It remains a presentation component with no implicit navigation bar.

Available in [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), `Header(std::string)`; public `HeaderComponent` in [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). It currently measures 640 × 70 and always draws the subtitle `SATURATION / CHARACTER`.

MyGo: no standalone Header component; `Text`, `Column`, and `Divider` in `ui/widgets.go` support this composition. Removing the dependency on an audio-specific label is a NativeUI enhancement.

## 2. Public API and composition

Current API preserved: `Header(std::string title)` and `spec() &&`.

Proposed target API:

```cpp
class Header {
public:
    explicit Header(std::string title);
    Header&& subtitle(std::string value) &&;
    Header&& style(HeaderStyle value) &&;
    Spec spec() &&;
};
```

`HeaderStyle` contains `TextStyle title`, `TextStyle subtitle`, and logical padding, gap, border_color, and border_width. New numeric fields are double; no Theme slot is assumed to exist.

Proposed target example: `ui::Header{"Library"}.subtitle("User presets").spec()`. The historical constructor without a setter preserves the current subtitle; `subtitle("")` explicitly removes it.

## 3. State, ownership, and notifications

Title, subtitle, and style are owned by value. No Binding in this version; updates rebuild the composition.

Omitting the subtitle setter is distinct from an explicitly empty string to maintain historical behavior. Do not reinsert the historical string after the user has chosen an empty string.

No application notification or audio/host data. Identity and lifetime follow the retained node.

## 4. Interactions

Pointer, drag, wheel, and keyboard events are ignored; no focus or text input.

An action such as closing a panel must use a neighboring Button rather than a hidden active region in the Header.

No confirmation/cancellation. The separator is not a resize handle.

## 5. Measurement and layout

Preserve 640 × 70 as the preferred size of the historical path. The path with a custom style/subtitle measures text and spacing, bounded by available constraints.

Title and subtitle occupy two separate lines; an empty subtitle removes its line and gap in the custom path. Widths come from TextService.

At narrow widths, clip within bounds without allowing the separator to overflow. Do not silently reduce font size; insufficient space means deterministic clipping.

## 6. Presentation and invalidation

Customizable style covers title/subtitle/border without imposing a product name. In the new path, the default palette is taken from Theme at mounting; explicit overrides take precedence.

Size/family/padding invalidate layout and paint when rebuilding; color alone invalidates paint.

No animation. Use logical coordinates when drawing the line; pixel adjustment belongs to the renderer.

## 7. Accessibility

Target contract: Group with a Text title and Text subtitle; do not claim a Heading role already exists.

The decorative separator has no independent semantic node. The title must remain readable even when the visual area is clipped.

Backend-neutral hooks are available; verify target publication headlessly. Native T068 bridges are deferred, with no promise of VoiceOver/UIA/AT-SPI reading.

## 8. Lifecycle and recovery

No capture, timer, or subscription. The component owns all content used during paint and measure.

If typography preparation throws, preserve retained invariants; do not attempt rendering with a partially published style.

Destruction is no-throw and invokes no callbacks. No mutable state shared between headers or UIs; no retained measurement/paint context.

## 9. Dependencies and edge cases

Reuses Label/TextService and separator drawing; [Divider](divider.md) shares the visual convention without a second layout loop.

Empty title and subtitle alone are allowed. Transparent colors are allowed. Non-finite/negative style dimensions for new options are rejected before Spec with invalid_argument.

No MyGo domain-specific equivalent to import; no audio or plugin-format access. Unicode follows the common TextService repair.

## 10. Files and compatibility

Target: `include/nativeui/header.hpp` and `src/header.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: `widgets_builders.inc` and `widgets_basic.inc`. Preserve `HeaderComponent(std::string)` and the aggregate include; add an internal overload/options for the enhanced path without changing the historical default.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `header_legacy_presentation`: simple construction preserves historical size and subtitle.
- `header_custom_subtitle`: custom and empty strings use distinct layouts.
- `header_narrow_clip`: long titles and the separator stay within their bounds.
- `header_semantic_text`: title and subtitle appear once in the snapshot.

Create `examples/features/header.cpp` and target `nativeui_example_header`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.