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

`SliderStylePatch` is a detached partial override for inactive track, active range, thumb, focus ring and formatter text, plus track thickness, thumb diameter and focus-ring width. Geometry is expressed in logical UI pixels. Engaged optionals are stored verbatim; negative or non-finite values are not clamped by the style layer.

Resolution order is:

```text
base -> interaction -> read_only -> focused
```

Interaction selects exactly one branch with `disabled > pressed > hovered > normal` precedence. Within each layer the inherited recipe is applied before the local recipe. `ResolvedSliderStyle` is an owned snapshot; it borrows nothing from its inputs.

Thumb diameter and focus-ring width currently affect intrinsic cross-axis measurement, so changing either can require layout. Track thickness is paint geometry inside the measured bounds. `default_slider_style(theme)` only borrows the Theme for the call and returns detached values. `resolve_slider_style()` performs no Theme lookup, callback dispatch or retained-tree mutation. Applying style changes to a live Slider remains UI/main-thread work; these style helpers provide no audio/DSP real-time guarantee.

```cpp
auto inherited = ui::default_slider_style(theme);
ui::SliderStyle local;
local.base.thumb_diameter = 18.0f;        // logical UI pixels
local.focused.focus_ring_width = 3.0f;   // logical UI pixels

ui::VisualState state;
state.focused = true;

const ui::ResolvedSliderStyle resolved =
    ui::resolve_slider_style(inherited, local, state);
```

## ProgressBar / Meter

`ProgressStylePatch` contains track/fill/border/text colors, outer/fill radii, border width, formatted-text size and four preferred sizes: horizontal/vertical, each with unformatted and formatted variants. Every size and scalar geometry value is in logical UI pixels and is stored verbatim. A disengaged optional inherits the value from the previous layer.

Both families resolve in this order:

```text
base -> interaction -> read_only -> focused
```

The interaction branch uses `disabled > pressed > hovered > normal`; inherited values are applied before local values at every layer. `ResolvedProgressStyle` is fully owned. No implicit Theme lookup or fallback occurs: if callers resolve incomplete recipes and a field is absent in both, the result keeps that field's default-constructed value.

`default_progress_bar_style(theme)` and `default_meter_style(theme)` synchronously borrow the Theme and return detached recipes. Their default geometry is the same except that ProgressBar uses a 6 logical-pixel fill radius while Meter uses 3. Resolution invokes no callbacks and mutates no retained state. Live-widget application remains UI/main-thread work and is outside the audio/DSP real-time contract.

## Toggle

`ToggleStylePatch` owns optional surface/border/text colors, switch track/thumb colors, logical-pixel control and switch geometry, typography, a font-family string and ordered fallback-family strings. Font data is copied into both recipes and resolved values; it is never borrowed from the Theme after default-style construction returns.

Toggle resolution deliberately places checked state before interaction:

```text
base -> checked -> interaction -> read_only -> focused
```

Consequently a checked+disabled Toggle first receives the checked patch, then the disabled patch may override overlapping fields. Read-only and focus apply after interaction, with focus last. Inherited values precede local values inside each layer.

`default_toggle_style(theme)` and `resolve_toggle_style()` may allocate while copying strings/vectors; allocation failure propagates rather than silently substituting typography. They invoke no callbacks and do not traverse or mutate the retained tree. Live style mutation is UI/main-thread work and these helpers are not audio/DSP real-time APIs.

## Scrollbar

`ScrollbarStylePatch` controls track/thumb color, logical-pixel thickness, minimum thumb length and corner radius. The ScrollView scrollbar is pointer-targetable but not keyboard-focusable, so the family intentionally has no focused patch.

Resolution order is:

```text
base -> interaction -> read_only
```

Interaction again selects one `disabled > pressed > hovered > normal` branch; local values win inherited values in each layer. `ResolvedScrollbarStyle` owns its complete snapshot. Its equality operator compares colors through the style color comparator and compares float geometry exactly, with no epsilon or normalization.

`default_scrollbar_style(theme)` borrows the Theme only while constructing the result. Current defaults use an 8 logical-pixel thickness and an 18 logical-pixel minimum thumb. The resolver performs no Theme lookup, validation, fallback, callbacks or retained-state mutation. Applying the result to a live ScrollView belongs to the UI/main-thread domain.

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

TextInputStylePatch covers field/border, label/text/placeholder, selection/caret/composition underline, control/field geometry, padding/insets and typography. All scalar geometry is in logical UI units. control_width / control_height are the preferred measured size; field_top and field_height locate the editable field. horizontal_padding and content_vertical_inset define the clipped editable-content rectangle. Selection, caret and IME underline widths/insets are paint geometry inside that field.

Patch optionals have literal layering semantics: disengaged means “inherit the value already resolved by earlier layers”; engaged means “replace it with this exact payload.” Numeric values are not clamped or normalized by the style resolver, including negative/non-finite payloads. Measurement-affecting fields include control/field extents, padding, text sizes and font selection; border/selection/caret/composition geometry is paint-side inside the retained field. The style value itself neither classifies nor publishes invalidation.

default_text_input_style(theme) synchronously borrows the Theme and returns detached owned recipe data. Its built-in interaction patches preserve the baseline geometry and change presentation only. resolve_text_input_style() applies inherited base, explicit base, one interaction branch (disabled > pressed > hovered > normal), read-only, then focused; explicit/local fields win inside each layer. Missing fields remain default constructed rather than triggering a hidden Theme lookup or fallback.

The returned ResolvedTextInputStyle owns its font-family/fallback strings and borrows nothing from either recipe or Theme. Resolution invokes no callbacks, performs no retained-tree mutation or invalidation and has no reentrant UI-dispatch path. Owned string/vector copies may allocate and allocation failure propagates; these helpers are UI/style-preparation work, not audio/DSP real-time operations.

## TextArea

TextAreaStylePatch is the multiline counterpart. control_width / control_height are preferred measured bounds; field_top begins the field and minimum_field_height prevents it from collapsing below the configured height. horizontal_padding / vertical_padding define the multiline content viewport, line_height is the per-line vertical advance, and newline_selection_width extends a selection highlight when the selected range includes a line break.

TextArea uses the same exact optional-layering contract and likewise stores numeric payloads verbatim. Control/field extents, padding, line height, text sizes and font selection can affect measurement/layout. Border, selection, caret, newline-selection extension and composition underline geometry are paint-side properties inside the published multiline field.

default_text_area_style(theme) borrows Theme data only for the call, owns all copied font data in the returned recipe and keeps the baseline geometry stable across its built-in interaction/read-only/focus variants. resolve_text_area_style() follows inherited/local base -> interaction -> read-only -> focused precedence, performs no implicit Theme lookup or numeric sanitization, and returns a fully owned ResolvedTextAreaStyle.

Both text-style resolver families are callback-free and have no reentrancy path of their own. Applying a newly resolved style to a live editor remains UI/main-thread retained work; the detached recipe/resolver layer itself provides no cross-thread synchronization.

### Text editor style example

```cpp
auto inherited = ui::default_text_input_style(theme);

ui::TextInputStyle local;
local.base.control_width = 480.0f;
local.base.horizontal_padding = 16.0f;
local.focused.border = ui::Color{0.2f, 0.7f, 1.0f, 1.0f};

ui::VisualState state;
state.focused = true;

ui::ResolvedTextInputStyle resolved =
    ui::resolve_text_input_style(inherited, local, state);
```

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
