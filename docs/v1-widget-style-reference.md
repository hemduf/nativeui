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

`style.hpp` defines `VisualState`, the shared interaction selector, and the Button/Checkbox/Radio recipes. These are backend-neutral value types: detached recipes contain no UI/tree/native ownership and resolver functions do not invoke callbacks, traverse scopes or mutate retained state.

The shared interaction branch is deterministic:

```text
disabled > pressed > hovered > normal
```

`focused`, `read_only`, `checked` and `selected` remain orthogonal flags. Family resolvers apply layers in a fixed order, and within each layer the inherited recipe is applied before the component-local recipe. An explicit/local field therefore wins at the same layer.

Resolvers perform **no implicit Theme lookup**. Callers normally provide `default_*_style(theme)` as the fully populated inherited recipe. If a field is absent from both inherited and local recipes, the corresponding `Resolved*Style` default remains in place (numeric/color fields can therefore remain zero/default). Returned font-family strings and fallback vectors are owned copies, not borrows.

Numeric patch values are stored verbatim; the style value layer does not clamp negative/non-finite geometry. Geometry and text sizes use logical UI units. Control dimensions, padding/insets, intrinsic geometry and typography can affect measurement; colors and decorative geometry are usually paint-only. The live widget's invalidation classifier remains authoritative.

Default-style creation and resolution can allocate while copying strings/vectors. Allocation failure propagates; there is no fallback/error-code path. The pure resolvers themselves have no reentrancy path because they invoke no callbacks. They are not audio-real-time operations. Applying/changing style on retained widgets remains UI/main-thread work.

## Button

`ButtonStylePatch` covers fill/border/text plus logical-unit border width, corner radius, minimum width, control height, per-side horizontal text padding and typography. Current Button measurement depends on `minimum_width`, `control_height`, `horizontal_padding` and typography; border width and corner radius are paint geometry.

Resolution order is:

```text
base -> interaction -> read_only -> focused
```

Within each layer inherited fields are applied before local fields. Focus is therefore the final overlapping override. `default_button_style(theme)` returns an owned complete recipe; `resolve_button_style()` borrows its input recipes only for the call and returns an owned resolved value.

## Checkbox

`CheckboxStylePatch` covers box/checkmark/label colors, box and checkmark geometry, minimum/control size, leading inset, label gap and typography. Current measurement depends on `box_size`, `minimum_width`, `control_height`, `leading_padding`, `label_gap` and typography; corner/border/checkmark widths are paint geometry.

Resolution order is:

```text
base -> checked -> interaction -> read_only -> focused
```

This ordering is intentional: for checked+disabled, the disabled interaction patch can override the checked patch; read-only and focus apply later. The checked state still controls whether the retained widget paints its checkmark.

## RadioButton

`RadioStylePatch` covers outer/inner/selected-mark colors and radii, minimum/control size, leading inset, label gap and typography. `outer_radius` affects measurement because it determines the circular control diameter; `inner_radius` and `mark_radius` are paint geometry.

Resolution order is:

```text
base -> selected -> interaction -> read_only -> focused
```

A selected+disabled radio receives the selected patch first and disabled overrides second where fields overlap.

```cpp
auto inherited = ui::default_checkbox_style(theme);

ui::CheckboxStyle local;
local.checked.box_fill = ui::Color{0.15f, 0.75f, 0.35f, 1.0f};
local.focused.box_border_width = 3.0f;

ui::VisualState visual;
visual.checked = true;
visual.focused = true;

ui::ResolvedCheckboxStyle resolved =
    ui::resolve_checkbox_style(inherited, local, visual);
```

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

`ListViewStylePatch` is a partial owned override. A disengaged optional inherits the value already produced by the previous layer; an engaged optional replaces it. Surface border width/radius, row insets/radius/accent geometry and separator width/inset are expressed in logical UI pixels and are stored verbatim. The style layer does not clamp negative or non-finite values.

`ListViewStyle` resolves in this exact order:

```text
base -> selected -> interaction -> read_only -> focused
```

Interaction is one branch using the shared `disabled > pressed > hovered > normal` precedence. At every layer the inherited recipe is applied before the explicit/component-local recipe, so local fields win when both are present. `ResolvedListViewStyle` is an owned snapshot and borrows nothing from either recipe or the Theme used to create defaults. Equality is exact for geometry; no epsilon/tolerance is applied.

`TabsStylePatch` follows the same absent-means-inherit contract. Header/panel border widths, header height, panel gap, radii, tab inset/radius, underline geometry, separator geometry and text size are logical UI pixels. `TabsStyle` uses the same `base -> selected -> interaction -> read_only -> focused` layering and inherited-before-local rule. `ResolvedTabsStyle` is likewise an independent owned value.

`default_list_view_style(theme)` and `default_tabs_style(theme)` borrow the Theme only for the call and return detached recipes. The resolver functions borrow their recipe/state arguments synchronously, invoke no callbacks, perform no Theme lookup and do not mutate retained state. Applying a resolved/recipe change to a live widget remains UI/main-thread work; these helpers are not intended for audio/DSP real-time callbacks.

```cpp
auto inherited = ui::default_tabs_style(theme);

ui::TabsStyle local;
local.selected.underline_height = 3.0f; // logical pixels
local.focused.header_border_width = 2.0f;

ui::VisualState state;
state.selected = true;
state.focused = true;

ui::ResolvedTabsStyle resolved =
    ui::resolve_tabs_style(inherited, local, state);
```

## ComboBox / MenuItem

`ComboBoxStylePatch` is a detached partial override for the ComboBox anchor. A disengaged optional means “inherit the value already produced by the previous layer”; an engaged optional replaces it. Border width, corner radius, minimum width, control height, horizontal padding and text size are logical UI pixels and are stored verbatim: this style layer does not clamp negative/non-finite values. Font-family strings and fallback vectors are owned.

`ComboBoxStyle` resolves in this exact order:

```text
base -> interaction -> read_only -> focused
```

The interaction layer selects exactly one branch with `disabled > pressed > hovered > normal` precedence. Within every layer, inherited fields are applied before component-local fields, so the local recipe wins on overlap. `ResolvedComboBoxStyle` is an independent owned snapshot. If callers resolve incomplete recipes without first supplying `default_combo_box_style(theme)`, untouched fields retain their zero/default constructed values; resolution does not perform an implicit Theme lookup.

`MenuItemStylePatch` follows the same absent-means-inherit and logical-pixel rules for row/separator geometry, padding, radius and typography. Menu-item resolution deliberately differs in layer order:

```text
base -> interaction -> selected -> read_only -> focused
```

Selection is an orthogonal flag, but its patch is applied **after** the interaction branch. Consequently a selected patch can override an overlapping hovered/pressed/disabled field. Read-only then overrides selection where specified, and focus is final. As with ComboBox, inherited values are applied before local values inside each layer. `ResolvedMenuItemStyle` owns its font strings/vectors and borrows nothing from its recipes or Theme.

`default_combo_box_style(theme)` and `default_menu_item_style(theme)` borrow the Theme only for the call and return detached recipes. The resolver functions synchronously borrow recipe/state inputs, invoke no callbacks, do not mutate retained state, and have no reentrant UI-dispatch path. Copying strings/vectors can allocate and allocation failure propagates; there is no silent fallback. These helpers are therefore not audio/DSP real-time operations. Applying recipe changes to live widgets remains UI/main-thread work.

```cpp
auto inherited = ui::default_menu_item_style(theme);

ui::MenuItemStyle local;
local.selected.fill = ui::Color{0.12f, 0.25f, 0.55f, 1.0f};
local.focused.text = ui::Color{1.0f, 1.0f, 1.0f, 1.0f};

ui::VisualState state;
state.selected = true;
state.focused = true;

ui::ResolvedMenuItemStyle resolved =
    ui::resolve_menu_item_style(inherited, local, state);
```

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
