# NativeUI 1.0 widget style reference

NativeUI standard-widget styles are typed C++ values. Every family follows three layers: an optional-field patch, a recipe containing base/state patches, and a resolved style containing concrete values consumed by measurement and painting.

Public families:

- [`slider_style.hpp`](../include/nativeui/slider_style.hpp)
- [`progress_style.hpp`](../include/nativeui/progress_style.hpp)
- [`toggle_style.hpp`](../include/nativeui/toggle_style.hpp)
- [`scrollbar_style.hpp`](../include/nativeui/scrollbar_style.hpp)
- [`list_tabs_style.hpp`](../include/nativeui/list_tabs_style.hpp)
- [`combo_popup_style.hpp`](../include/nativeui/combo_popup_style.hpp)
- [`text_input_style.hpp`](../include/nativeui/text_input_style.hpp)
- [`text_area_style.hpp`](../include/nativeui/text_area_style.hpp)

## Common resolution

Inherited/theme-derived base values resolve first, then component-local base overrides, then the active state patches. The shared interaction branch is deterministic:

```text
disabled > pressed > hovered > normal
```

Checked/selected, read-only and focused states remain orthogonal where exposed. Only fields present in a patch replace previous values.

A style field can affect measurement. Control dimensions, padding/insets, intrinsic geometry and typography can require layout; colors/highlights are usually paint-only. The widget's style invalidation classifier remains authoritative.

## Slider / RangeSlider

`SliderStylePatch` controls track, active-range, thumb, focus-ring and formatter-text colors plus track thickness, thumb diameter and focus-ring width. `SliderStyle` supplies base/hovered/pressed/disabled/read-only/focused patches; `ResolvedSliderStyle` is concrete.

Thumb diameter and focus-ring width currently affect intrinsic cross-axis measurement; track thickness is paint geometry inside the measured bounds.

## ProgressBar / Meter

`ProgressStylePatch` contains track/fill/border/text colors, border/corner geometry, text size, and horizontal/vertical intrinsic sizes including formatted variants.

`ProgressBarStyle` and `MeterStyle` use separate defaults; Meter has a tighter default fill radius. Resolve with `resolve_progress_bar_style()` or `resolve_meter_style()`.

## Toggle

`ToggleStylePatch` covers surface/border/text, switch track/thumb, control geometry and typography. Checked state is orthogonal to interaction state, so checked+hovered/pressed/disabled/read-only/focused combinations resolve deterministically.

## Scrollbar

`ScrollbarStylePatch` controls track/thumb color, thickness, minimum thumb length and corner radius. The ScrollView scrollbar is pointer-targetable but not keyboard-focusable, so its recipe uses interaction plus read-only variants without a focused patch.

## ListView / Tabs

`ListViewStylePatch` controls surface/row/accent/separator colors and their border/radius/inset geometry. Selection is orthogonal to interaction.

`TabsStylePatch` controls header/panel/tab/text/separator/underline presentation plus header/panel/tab dimensions and text size.

## ComboBox / MenuItem

`ComboBoxStylePatch` controls anchor fill/border/text, minimum width, control height, padding and typography.

`MenuItemStylePatch` controls popup row fill/text/separator, row/separator geometry, padding, radius and typography. Selected state remains orthogonal to interaction.

## TextInput

`TextInputStylePatch` covers field/border, label/text/placeholder, selection/caret/composition underline, control/field geometry, padding/insets and typography. All float geometry is in logical UI units. `control_width` / `control_height` are the preferred measured size; `field_top` and `field_height` locate the editable field. `horizontal_padding` and `content_vertical_inset` define the clipped editable-content rectangle. Selection, caret and IME underline widths/insets are paint geometry inside that field.

Patch values are stored verbatim rather than sanitized by the style layer. `default_text_input_style(theme)` returns owned recipe data. `resolve_text_input_style()` applies inherited base, explicit base, one interaction branch (`disabled > pressed > hovered > normal`), read-only, then focused; explicit fields win inside each layer. The returned `ResolvedTextInputStyle` owns its font-family/fallback strings and borrows nothing from the input recipes.

## TextArea

`TextAreaStylePatch` is the multiline counterpart. `control_width` / `control_height` are preferred measured bounds; `field_top` begins the field and `minimum_field_height` prevents it from collapsing below the configured height. `horizontal_padding` / `vertical_padding` define the multiline content viewport, `line_height` is the per-line vertical advance, and `newline_selection_width` extends a selection highlight when the selected range includes a line break.

TextArea patch values are likewise stored verbatim. `resolve_text_area_style()` uses the same inherited/explicit and interaction/read-only/focused precedence and returns a fully owned `ResolvedTextAreaStyle`. Both text-style resolvers may allocate while copying font-family/fallback data and are not audio-real-time operations.

## Example

```cpp
auto inherited = ui::default_slider_style(theme);

ui::SliderStyle local;
local.base.active = ui::Color{0.2f, 0.8f, 0.4f, 1.0f};
local.focused.focus_ring_width = 3.0f;

const auto resolved =
    ui::resolve_slider_style(inherited, local, visual_state);
```

Style recipes and resolved values are ordinary per-instance value data. They contain no native/backend widget objects and provide no cross-thread synchronization; mutation belongs to the owning UI/main-thread domain.
