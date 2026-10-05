# NativeUI component specifications

This catalog is the entry point for **83 public component specifications**.
It covers the **54 documented MyGo families**, their standalone variants,
NativeUI's additional widgets and its composition containers.
A MyGo family does not necessarily correspond to one component: alerts are
a Dialog variant, whereas Form, Field and Fieldset have separate responsibilities
and file pairs.

[Implementation tracking](widgets_implementation.md): integrated files, validation and contracts still to be completed.

[Demo gallery](widgets_demo.md): an interactive application using all 83 components, launch instructions and self-tests.

[Widget review](widgets_review.md): confirmed defects, corrections and the mandatory review/validation record.

## 1. Scope and reading guide

These documents preserve the initial documentation snapshot and specify the development target.
The implementation requested subsequently is tracked separately in
[widgets_implementation.md](widgets_implementation.md). Each page distinguishes the verified
existing API, the proposed target API and delivery criteria. Target examples are not
presented as having been compiled against the current version.

References frozen for this inventory on October 4, 2026:

- NativeUI: `e10077ff39b8cb977669a7d5604562f66d07cb4c`, inspected checkout
  `/Volumes/T7/Code/nativeui`; [architecture](../DESIGN.md),
  [review contract](../CODE_REVIEW.md), [widget API](../include/nativeui/widgets.hpp).
- MyGo: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`, inspected checkout
  `/Volumes/T7/Code/mygo`; `docs/ui/README.md`, sources under `ui/` and examples
  under `examples/gallery/`. Each page identifies the files and functions consulted
  at this version.

Status labels describe the work remaining at that snapshot; they do not replace
GitHub issue statuses:

| Documentation status | Meaning |
| --- | --- |
| **existing — extraction required** | Capability already present; preserve its behavior/API while separating header and cpp. |
| **existing — enhancements required** | Capability already present; the page explicitly separates extraction from extensions. |
| **new — implementation required** | Public widget absent; its API and behavior are described as targets. |

Each page has eleven sections: purpose/current behavior, API, state/ownership,
interactions, layout, presentation, accessibility, lifecycle/recovery,
dependencies/edge cases, files/compatibility, and tests/acceptance.

## 2. Catalog and target files

The page, header and cpp use the same `snake_case` name. The C++ paths in the
table are **targets**, even when a header with that name already exists.
Variants, item models and private implementation classes stay in their component's
file pair. Data types and controllers do not receive an empty cpp solely to
satisfy this rule.

### Text and drawing

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Label](label.md) | existing — extraction required | `include/nativeui/label.hpp` | `src/label.cpp` |
| [Header](header.md) | existing — enhancements required | `include/nativeui/header.hpp` | `src/header.cpp` |
| [RichText](rich_text.md) | new — implementation required | `include/nativeui/rich_text.hpp` | `src/rich_text.cpp` |
| [Canvas](canvas.md) | existing — enhancements required | `include/nativeui/canvas.hpp` | `src/canvas.cpp` |
| [Knob](knob.md) | existing — enhancements required | `include/nativeui/knob.hpp` | `src/knob.cpp` |
| [Divider](divider.md) | new — implementation required | `include/nativeui/divider.hpp` | `src/divider.cpp` |

### Actions

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Button](button.md) | existing — enhancements required | `include/nativeui/button.hpp` | `src/button.cpp` |
| [Link](link.md) | new — implementation required | `include/nativeui/link.hpp` | `src/link.cpp` |
| [PopupMenu](popup_menu.md) | existing — enhancements required | `include/nativeui/popup_menu.hpp` | `src/popup_menu.cpp` |
| [ContextMenu](context_menu.md) | new — implementation required | `include/nativeui/context_menu.hpp` | `src/context_menu.cpp` |
| [ToggleButton](toggle_button.md) | new — implementation required | `include/nativeui/toggle_button.hpp` | `src/toggle_button.cpp` |
| [ToggleGroup](toggle_group.md) | new — implementation required | `include/nativeui/toggle_group.hpp` | `src/toggle_group.cpp` |
| [SegmentedControl<T>](segmented_control.md) | new — implementation required | `include/nativeui/segmented_control.hpp` | `src/segmented_control.cpp` |
| [Toolbar](toolbar.md) | new — implementation required | `include/nativeui/toolbar.hpp` | `src/toolbar.cpp` |

### Selection controls

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Checkbox](checkbox.md) | existing — extraction required | `include/nativeui/checkbox.hpp` | `src/checkbox.cpp` |
| [CheckboxGroup](checkbox_group.md) | new — implementation required | `include/nativeui/checkbox_group.hpp` | `src/checkbox_group.cpp` |
| [RadioButton<T> and RadioGroup<T>](radio_button.md) | existing — extraction required | `include/nativeui/radio_button.hpp` | `src/radio_button.cpp` |
| [Toggle](toggle.md) | existing — extraction required | `include/nativeui/toggle.hpp` | `src/toggle.cpp` |
| [ComboBox<T>](combo_box.md) | existing — extraction required | `include/nativeui/combo_box.hpp` | `src/combo_box.cpp` |
| [Slider](slider.md) | existing — extraction required | `include/nativeui/slider.hpp` | `src/slider.cpp` |
| [RangeSlider](range_slider.md) | existing — enhancements required | `include/nativeui/range_slider.hpp` | `src/range_slider.cpp` |
| [Stepper](stepper.md) | new — implementation required | `include/nativeui/stepper.hpp` | `src/stepper.cpp` |
| [Rating](rating.md) | new — implementation required | `include/nativeui/rating.hpp` | `src/rating.cpp` |

### Input

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [TextInput](text_input.md) | existing — enhancements required | `include/nativeui/text_input.hpp` | `src/text_input.cpp` |
| [TextArea](text_area.md) | existing — enhancements required | `include/nativeui/text_area.hpp` | `src/text_area.cpp` |
| [NumberInput](number_input.md) | new — implementation required | `include/nativeui/number_input.hpp` | `src/number_input.cpp` |
| [SearchField](search_field.md) | new — implementation required | `include/nativeui/search_field.hpp` | `src/search_field.cpp` |
| [EditableComboBox](editable_combo_box.md) | new — implementation required | `include/nativeui/editable_combo_box.hpp` | `src/editable_combo_box.cpp` |
| [Autocomplete](autocomplete.md) | new — implementation required | `include/nativeui/autocomplete.hpp` | `src/autocomplete.cpp` |
| [TokenField](token_field.md) | new — implementation required | `include/nativeui/token_field.hpp` | `src/token_field.cpp` |
| [EditableText](editable_text.md) | new — implementation required | `include/nativeui/editable_text.hpp` | `src/editable_text.cpp` |
| [FindBar](find_bar.md) | new — implementation required | `include/nativeui/find_bar.hpp` | `src/find_bar.cpp` |

### Dates and colors

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Calendar](calendar.md) | new — implementation required | `include/nativeui/calendar.hpp` | `src/calendar.cpp` |
| [DateInput](date_input.md) | new — implementation required | `include/nativeui/date_input.hpp` | `src/date_input.cpp` |
| [TimeInput](time_input.md) | new — implementation required | `include/nativeui/time_input.hpp` | `src/time_input.cpp` |
| [ColorPicker](color_picker.md) | new — implementation required | `include/nativeui/color_picker.hpp` | `src/color_picker.cpp` |
| [ColorWell](color_well.md) | new — implementation required | `include/nativeui/color_well.hpp` | `src/color_well.cpp` |

### Collections and navigation

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [ListView<Key>](list_view.md) | existing — enhancements required | `include/nativeui/list_view.hpp` | `src/list_view.cpp` |
| [TableView<Key>](table_view.md) | new — implementation required | `include/nativeui/table_view.hpp` | `src/table_view.cpp` |
| [TreeView<Key>](tree_view.md) | new — implementation required | `include/nativeui/tree_view.hpp` | `src/tree_view.cpp` |
| [OutlineView<Key>](outline_view.md) | new — implementation required | `include/nativeui/outline_view.hpp` | `src/outline_view.cpp` |
| [OutlineTableView<Key>](outline_table_view.md) | new — implementation required | `include/nativeui/outline_table_view.hpp` | `src/outline_table_view.cpp` |
| [GridView<Key>](grid_view.md) | new — implementation required | `include/nativeui/grid_view.hpp` | `src/grid_view.cpp` |
| [Tabs<T>](tabs.md) | existing — extraction required | `include/nativeui/tabs.hpp` | `src/tabs.cpp` |
| [Sidebar<Key>](sidebar.md) | new — implementation required | `include/nativeui/sidebar.hpp` | `src/sidebar.cpp` |
| [Breadcrumbs](breadcrumbs.md) | new — implementation required | `include/nativeui/breadcrumbs.hpp` | `src/breadcrumbs.cpp` |
| [HistoryButton](history_button.md) | new — implementation required | `include/nativeui/history_button.hpp` | `src/history_button.cpp` |

### Containers

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Row](row.md) | existing — extraction required | `include/nativeui/row.hpp` | `src/row.cpp` |
| [Column](column.md) | existing — extraction required | `include/nativeui/column.hpp` | `src/column.cpp` |
| [Grid](grid.md) | existing — enhancements required | `include/nativeui/grid.hpp` | `src/grid.cpp` |
| [Scroll](scroll.md) | existing — extraction required | `include/nativeui/scroll.hpp` | `src/scroll.cpp` |
| [ScrollView](scroll_view.md) | existing — extraction required | `include/nativeui/scroll_view.hpp` | `src/scroll_view.cpp` |
| [Clip](clip.md) | existing — extraction required | `include/nativeui/clip.hpp` | `src/clip.cpp` |
| [Flex](flex.md) | existing — extraction required | `include/nativeui/flex.hpp` | `src/flex.cpp` |
| [Spacer](spacer.md) | existing — extraction required | `include/nativeui/spacer.hpp` | `src/spacer.cpp` |
| [Stack](stack.md) | existing — extraction required | `include/nativeui/stack.hpp` | `src/stack.cpp` |
| [Padding](padding.md) | existing — extraction required | `include/nativeui/padding.hpp` | `src/padding.cpp` |
| [SplitView](split_view.md) | new — implementation required | `include/nativeui/split_view.hpp` | `src/split_view.cpp` |
| [Collapsible](collapsible.md) | new — implementation required | `include/nativeui/collapsible.hpp` | `src/collapsible.cpp` |
| [Accordion](accordion.md) | new — implementation required | `include/nativeui/accordion.hpp` | `src/accordion.cpp` |
| [Form](form.md) | new — implementation required | `include/nativeui/form.hpp` | `src/form.cpp` |
| [Field](field.md) | new — implementation required | `include/nativeui/field.hpp` | `src/field.cpp` |
| [Fieldset](fieldset.md) | new — implementation required | `include/nativeui/fieldset.hpp` | `src/fieldset.cpp` |

### Dialogs and messages

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Dialog](dialog.md) | existing — enhancements required | `include/nativeui/dialog.hpp` | `src/dialog.cpp` |
| [Popover](popover.md) | new — implementation required | `include/nativeui/popover.hpp` | `src/popover.cpp` |
| [Tooltip](tooltip.md) | existing — enhancements required | `include/nativeui/tooltip.hpp` | `src/tooltip.cpp` |
| [Toast](toast.md) | new — implementation required | `include/nativeui/toast.hpp` | `src/toast.cpp` |

### Indicators

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [ProgressBar](progress_bar.md) | existing — enhancements required | `include/nativeui/progress_bar.hpp` | `src/progress_bar.cpp` |
| [Spinner](spinner.md) | new — implementation required | `include/nativeui/spinner.hpp` | `src/spinner.cpp` |
| [Meter](meter.md) | existing — enhancements required | `include/nativeui/meter.hpp` | `src/meter.cpp` |
| [Badge](badge.md) | new — implementation required | `include/nativeui/badge.hpp` | `src/badge.cpp` |

### Retained composition

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [Visibility](visibility.md) | existing — enhancements required | `include/nativeui/visibility.hpp` | `src/visibility.cpp` |
| [Enabled](enabled.md) | existing — enhancements required | `include/nativeui/enabled.hpp` | `src/enabled.cpp` |
| [ReadOnly](read_only.md) | existing — enhancements required | `include/nativeui/read_only.hpp` | `src/read_only.cpp` |
| [If](if.md) | existing — extraction required | `include/nativeui/if.hpp` | `src/if.cpp` |
| [Switch<T>](switch.md) | existing — extraction required | `include/nativeui/switch.hpp` | `src/switch.cpp` |
| [ForEach<T>](for_each.md) | existing — extraction required | `include/nativeui/for_each.hpp` | `src/for_each.cpp` |
| [FocusScope](focus_scope.md) | existing — extraction required | `include/nativeui/focus_scope.hpp` | `src/focus_scope.cpp` |
| [CommandScope](command_scope.md) | existing — extraction required | `include/nativeui/command_scope.hpp` | `src/command_scope.cpp` |
| [StyleScope](style_scope.md) | existing — extraction required | `include/nativeui/style_scope.hpp` | `src/style_scope.cpp` |

### Images

| Component | Status | Target header | Target implementation |
| --- | --- | --- | --- |
| [ImageView](image_view.md) | new — implementation required | `include/nativeui/image_view.hpp` | `src/image_view.cpp` |
| [IconView](icon_view.md) | new — implementation required | `include/nativeui/icon_view.hpp` | `src/icon_view.cpp` |
| [Avatar](avatar.md) | new — implementation required | `include/nativeui/avatar.hpp` | `src/avatar.cpp` |

## 3. Complete MyGo mapping

| MyGo catalog family | NativeUI specification |
| --- | --- |
| Button | [button](button.md) |
| Link | [link](link.md) |
| Menu button | [popup_menu](popup_menu.md) |
| Context menu | [context_menu](context_menu.md) |
| Toggle | [toggle_button](toggle_button.md), [toggle_group](toggle_group.md) |
| Segmented control | [segmented_control](segmented_control.md) |
| Toolbar | [toolbar](toolbar.md) |
| Checkbox | [checkbox](checkbox.md), [checkbox_group](checkbox_group.md) |
| Switch | [toggle](toggle.md) |
| Radio | [radio_button](radio_button.md) |
| Select | [combo_box](combo_box.md) |
| Slider | [slider](slider.md) |
| Range slider | [range_slider](range_slider.md) |
| Stepper | [stepper](stepper.md) |
| Rating | [rating](rating.md) |
| Text input | [text_input](text_input.md), [text_area](text_area.md) |
| Number input | [number_input](number_input.md) |
| Search field | [search_field](search_field.md) |
| Combobox | [editable_combo_box](editable_combo_box.md) |
| Autocomplete | [autocomplete](autocomplete.md) |
| Token field | [token_field](token_field.md) |
| Editable text | [editable_text](editable_text.md) |
| Find bar | [find_bar](find_bar.md) |
| Calendar | [calendar](calendar.md) |
| Date input | [date_input](date_input.md) |
| Time input | [time_input](time_input.md) |
| Color picker | [color_picker](color_picker.md), [color_well](color_well.md) |
| List | [list_view](list_view.md) |
| Table | [table_view](table_view.md) |
| Tree | [tree_view](tree_view.md) |
| Outline | [outline_view](outline_view.md), [outline_table_view](outline_table_view.md) |
| Grid view | [grid_view](grid_view.md) |
| Scroll view | [scroll_view](scroll_view.md) |
| Grid | [grid](grid.md) |
| Split view | [split_view](split_view.md) |
| Collapsible | [collapsible](collapsible.md) |
| Accordion | [accordion](accordion.md) |
| Form | [form](form.md), [field](field.md), [fieldset](fieldset.md) |
| Tabs | [tabs](tabs.md) |
| Sidebar | [sidebar](sidebar.md) |
| Breadcrumbs | [breadcrumbs](breadcrumbs.md) |
| Back and forward buttons | [history_button](history_button.md) |
| Dialog | [dialog](dialog.md) |
| Alert dialog | [dialog](dialog.md) |
| Popover | [popover](popover.md) |
| Tooltip | [tooltip](tooltip.md) |
| Toast | [toast](toast.md) |
| Progress bar | [progress_bar](progress_bar.md) |
| Spinner | [spinner](spinner.md) |
| Meter | [meter](meter.md) |
| Badge | [badge](badge.md) |
| Image | [image_view](image_view.md) |
| Icon | [icon_view](icon_view.md) |
| Avatar | [avatar](avatar.md) |

### Mappings with different names

- MyGo Switch → NativeUI **Toggle**; `Switch<T>` remains conditional composition,
  and no `Switch = Toggle` alias is introduced.
- MyGo Toggle → **ToggleButton**; ToggleGroup groups pressed buttons.
- MyGo Select → **ComboBox<T>**; MyGo Combobox → **EditableComboBox**.
- MyGo MenuButton → **PopupMenu**; ContextMenu has its own decorator.
- MyGo Text/Textf → **Label**; TextLabel remains the existing alias. RichText
  specifies spans and embedded links in addition to the catalog's 54 families.
- MyGo Box/Column → **Column**; Row/Spacer/Divider remain named primitives,
  without creating a redundant Box component.
- ImageView and IconView are widgets; **Image** and **SvgIcon** are existing
  resources. TreeView displays a hierarchy; **Tree** remains the composition runtime.
- MyGo's unstyled bases provide behavioral and customization references in the
  pages; they do not become 15 additional public classes without an independent responsibility.
- PrimaryButton is a Button variant; AlertDialog is a Dialog variant;
  SplitVertical is a SplitView orientation; Back/ForwardButton are two
  HistoryButton directions. Tree items, Sidebar sections and Accordion items
  stay in their parent's module.

## 4. Shared architecture

### Composition and state

Builders produce a `ui::Spec`, normally through `spec() &&`, and Tree owns their
retained components. Existing controllers, particularly Dialog, keep their
controller API; this convention does not turn them into builders. Constructors
and templates needed for C++ composition remain adapters, rather than a
per-frame execution engine transplanted from MyGo.

Tree, widget and State operations belong to the UI/main thread. An audio or host
adapter applies updates after passing them through its own safe bridge between
threads. NativeUI receives no parameter IDs, automation or audio-format gestures.
External updates are reflected without synthetic user actions; a temporary edit
must never restore an old value after an external update.

Existing APIs retain their numeric types, particularly `float`. New numeric
controls use `double`, except when explicitly converting to an existing primitive.
The source State must satisfy its documented lifetime requirements; a Binding
links to the model and does not authorize retaining stale application pointers.
When a constructor converts State to Binding, the control block remains readable
after State destruction: valid() becomes false, get() returns the last value,
set() is ignored and observe() returns an inactive registration. There is no
implicit destruction notification; every new mutation checks valid() and publishes
no user callback if the model has disappeared. Actual borrowed references in
historical APIs retain their lifetime requirements, as stated in their pages.
Subscriptions, animations and overlays are instance-owned and released on unmount.

### Input, focus and notifications

Reuse the toolkit's retained routing, pointer capture, focus scopes, commands and
overlays. An interaction is canceled cleanly when its component is removed,
hidden or disabled. Tab traverses widgets; groups using arrow keys retain a
consistent Tab entry point. Collection typeahead defaults to ASCII case-insensitive
matching; non-ASCII UTF-8 characters continue to match exactly. This v1 choice
differs from MyGo's Unicode case folding and introduces no implicit Unicode backend.
ListView defines the buffer and navigation contract for its consumers to reuse.
Each page defines its local Space, Enter, Escape, wheel and release rules;
extraction does not silently standardize them.

The current Key does not cover PageUp/PageDown, function keys or Menu; Command
currently covers editing commands. Calendar, collections, FindBar, ContextMenu
and Form require additions explicitly described as targets to the enums and
platform translations. Append these values to the enums, preserve historical
numeric values and test normalized events. InputType::ContextMenu already exists;
its presence does not validate every native shortcut. Submit/Cancel remain
explicit commands; Enter in every editor is not automatically converted into
form submission.

A callback that has already started and throws is not automatically replayed.
Restore captures, transactions and dispatch flags before propagating C++ exceptions.
Callbacks that may destroy their owner finish component work before invocation,
or revalidate a safe identity afterward. Deferred closures carry safe identities
and lifetimes; a queue failure does not permit synchronous destruction without
a lifetime guarantee. Destruction and subscription cleanup remain no-throw.

### Layout, style and resources

Public geometry uses logical coordinates; the platform boundary applies the
framebuffer scale factor. Hit testing and painting use the same clips. Metric
invalidation triggers the required layout, while a purely visual change does not
require rebuilding the subtree. Form and Field specify a future optional
first-baseline hook, absent from the current ChildMetrics and Component.
Extracting existing layouts does not assume this hook. Style recipes are typed,
and disabled/read-only/selected/focused states remain distinct.

Widgets draw through Painter; Skia remains its implementation. Image decoding,
SVG parsing, prepared metrics, shaders and resources are prepared outside paint
when their cost or allocation requires it. No widget depends on Pugl, AppKit,
Win32, Xlib, HTML or the MyGo rendering engine.

### Platform, menus and navigation

ContextMenu, PopupMenu and Toolbar overflow use the same menu model and retained
overlays. MyGo uses system menus; this target chooses menus drawn by NativeUI,
with the same explicitly specified user features and without introducing a new
window abstraction. Toast specifies an additive
`OverlayPlacement::ViewportBottomCenter` extension to position a stack at its
natural size; it remains in the existing Overlay service. This placement is
unavailable in the inspected snapshot. Link reuses DesktopServices to open a URL,
or a callback for a local action. HistoryButton receives availability and actions
from the application; MyGo's Router, URL routing and page cache are not ported here.

### Accessibility and IME

The reference contract is [accessibility.md](accessibility.md), with
[SemanticInfo](../include/nativeui/semantics.hpp) and the `Component::semantics()`
hook. The presence of a hook or an enum role does not prove that every existing
widget already publishes this information. Each page states its target semantics
and the actions actually allowed by its state.

Native macOS/Windows/Linux bridges remain a deferred T068 dependency in the
[roadmap](../ROADMAP.md). No example establishes VoiceOver, UI Automation or
AT-SPI conformance. A new role absent from the v1 enum uses a composition of
existing roles or Custom/Group; any extension to the closed contract requires
its own architecture decision rather than an assumed enum value.

Text widgets may have composition paths testable headlessly. Complete native
transport of IME preedit and candidate rectangles remains distinct from committed
Unicode text, in accordance with DESIGN §17.4. New widgets consume this shared
model instead of inventing separate IME bridges. Label/description/error
relationships and status announcements beyond the current SemanticInfo are
explicit extensions.

<a id="modeles-partages"></a>
<a id="shared-models"></a>

## 5. Shared models

The following models are target APIs when absent from the snapshot. Complete
signatures and validation rules belong to their defining pages; consuming
components reuse these types instead of creating incompatible variants.

| Model | Definition and consumers |
| --- | --- |
| `SelectionSnapshot<Key>`, `Selection<Key>`, `CollectionItem<Key>` | [ListView](list_view.md) defines ordered selection, the active key, anchor and metadata; TableView, GridView and outlines reuse them. |
| `TreeNode<Key>` and expansion state | [TreeView](tree_view.md) defines a hierarchical snapshot, keys/parent and branches; [OutlineView](outline_view.md) and Sidebar consume this identity. |
| `TableColumn`, `TableLayout`, `SortOrder` | [TableView](table_view.md) defines stable IDs, geometry/persistence and sort requests; [OutlineTableView](outline_table_view.md) reuses these types. |
| `RadioGroup<T>` | [RadioButton](radio_button.md) keeps the existing shared selection controller; it does not become an artificial standalone widget. |
| `ScrollState`, `VirtualListState<Key>` | [Scroll](scroll.md), [ScrollView](scroll_view.md) and [ListView](list_view.md) preserve existing controllers and define their enhancements separately. |
| `PopupMenuItem` | [PopupMenu](popup_menu.md) keeps existing actions/separators and specifies menu extensions; ContextMenu and Toolbar reuse them. |
| `OverlayHandle`, `DialogSpec`, `DialogResult` | [Dialog](dialog.md), [Popover](popover.md), [Tooltip](tooltip.md) and [Toast](toast.md) retain one Overlay owner per UI. |
| Civil date and time | [Calendar](calendar.md)/DateInput use `std::chrono::sys_days`, with absence represented by optional; [TimeInput](time_input.md) uses seconds since midnight without a time zone. |
| Color and image | ColorPicker/ColorWell consume `ui::Color` in sRGB; ImageView/IconView share the existing immutable Image/SvgIcon handles. |

Collections use copyable, equality-comparable keys; they do not require integer
or hashable keys. The non-template kernel receives opaque identities with safe
lifetimes and comparison, construction and notification adapters. Reinserting a
removed key does not reactivate its old semantic identity. Token indexes separate
preparation cost from scrolling cost: encodable keys can use a sorted O(N log N)
preparation index; the fallback for equality-only keys may cost O(N²). Metadata
copies remain O(N), and scrolling does not repeat key resolution.

Virtualization must construct visible rows and overscan while exposing a
consistent logical dataset. O(N) metadata snapshots are shared during simple
scroll/focus changes; semantic reads trigger neither the row factory nor Tree
mutation. Variable heights, multiple selection and hierarchies must explicitly
extend this contract.

## 6. One header and one cpp per component

For every catalog name:

```text
docs/<component>.md
include/nativeui/<component>.hpp
src/<component>.cpp
```

The `.hpp` contains public declarations, required configuration types and only
essential template adapters. The `.cpp` contains a real non-template kernel:
retained behavior, measurement/layout, interactions, presentation resolution and
painting. Type erasure preserves user-key constraints and capabilities; explicit
instantiations for three known types do not satisfy this contract.

Variants (accent button, stepped slider, alert, orientations, back/forward button),
items/sections and internal runtime classes stay in their parent's file pair.
Shared controllers and data types may have useful modules of their own, but are
not counted as standalone widgets. An empty cpp or a facade leaving all
implementation in an `.inc` does not satisfy the separation requirement.

Preserve historical includes: `widgets.hpp`, `layout.hpp`, `combo_popup.hpp`,
`dynamic.hpp`, `component_state.hpp`, `nativeui.hpp`, and already-public service
and style headers. They become aggregators/compatibility entry points as
extraction progresses; currently used transitive includes are preserved and tested.
Also preserve publicly visible Component classes, aliases, State/Binding overloads,
deduction guides, template constraints, `&`/`&&` qualifications, option types and styles.

Future `.cpp` files are registered in `NativeUI::Core`. Extraction changes neither
the Skia/Pugl pins, consumer-specific macOS bridges nor the installed-package
model. An `.inc` is replaced only after migrating its contents; no second active
definition of the same runtime is retained.

## 7. Dependencies and implementation order

The following order organizes the work; each component retains its own acceptance
criteria and its page's explicit dependencies.

1. **Compatible extraction and examples**: existing widgets/layouts/wrappers,
   standalone headers, Core linkage and collective includes; baseline tests
   before any behavioral change.
2. **Small controls and containers**: ToggleButton/ToggleGroup, SegmentedControl,
   CheckboxGroup, Stepper/NumberInput, SearchField, Divider, Badge,
   Collapsible/Accordion, SplitView and Form/Field/Fieldset.
3. **Overlays and composed input**: enhanced menu model, ContextMenu, Popover,
   EditableComboBox/Autocomplete, EditableText, TokenField, FindBar and Toast.
   These widgets reuse focus/input/text/overlay.
4. **Collections**: shared selection and dataset model, ListView enhancements,
   TableView and TreeView, then OutlineView/OutlineTableView, GridView and Sidebar;
   measure scrolling and snapshot-publication costs.
5. **Navigation and presentation**: Breadcrumbs/HistoryButton, Toolbar and overflow,
   ImageView/IconView/Avatar, RichText, indeterminate progress, Spinner and Meter thresholds.
6. **Dates and colors**: Calendar → DateInput; TimeInput; ColorPicker → ColorWell.
   No implicit calendar, time-zone or system color-management service is introduced.

The concrete graph includes TextInput → NumberInput/SearchField/EditableComboBox;
EditableComboBox → Autocomplete/TokenField; ScrollView → ListView → TableView/GridView;
TreeView + ListView → OutlineView; OutlineView + TableView → OutlineTableView;
PopupMenu + Overlay → ContextMenu/Toolbar; Form + Field + Fieldset share one label
measurement; Image/SvgIcon resources → ImageView/IconView/Avatar.

## 8. Validation and future delivery

### Documentation

Documentation checks verify all 83 pages and eleven sections, exact coverage of
the 54 MyGo families, local links, consistent names/APIs across pages and one
distinct target header/cpp pair per component. For the initial documentation-only
phase, they also verify that the repository diff contains only the requested documents.
Future APIs are reviewed as proposals; they are not described as compiled or
tested without implementation.

### Implementation of each component

- Compile its header alone and through historical includes; also test State/Binding,
  custom keys and multiple translation units.
- Add the public example `examples/features/<component>.cpp` and its
  `nativeui_example_<component> --self-test`, usable without a display when the
  component allows it. At the specification snapshot, cpp files and examples are future deliverables.
- Test pointer/keyboard interactions, disabled/read-only states, external updates,
  boundaries, removal during interaction and hit-test/layout accuracy.
- Test failure injection, recovery after exceptions/scheduling failures, deferred
  destruction and destroying A while continuing B in two UIs.
- Verify component-specific metrics/headless/goldens, animations idle at rest and
  collection allocation/complexity where applicable.
- Apply CODE_REVIEW and the ticket's required tests/sanitizers/platform/package checks,
  with local builds executed serially and zero unapproved warnings.

A written specification does not automatically confer Ready/Done status. Issues,
required architecture decisions, execution evidence and merge gates continue to
follow [AGENTS.md](../AGENTS.md). These documents do not change official dependencies
or the v1 accessibility contract merely by describing them.
