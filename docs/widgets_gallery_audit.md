# Component gallery interaction and layout audit

This audit covers the 83 component specifications shown by the eight-screen
`nativeui_example_widgets_gallery` application. Each screen was rendered at
1280 × 900 and at 1280 × 1800 for visual inspection. The gallery self-test also
renders a 780 × 600 viewport at 1.25 scale and exercises collection, composition,
overlay, and multi-instance behavior. Component examples and unit tests provide
the broader executable behavior coverage.

The native macOS application launched, but this environment's Accessibility
inspection timed out on its window. Pointer behavior below was checked by
dispatching input into the same retained components in headless tests. The
captures establish headless layout, not a pixel identity claim for the GPU path.

## Confirmed findings and corrections

| ID | Component or screen | Confirmed problem | Correction and executable check |
| --- | --- | --- | --- |
| G1 | NumberInput | Both pointer step buttons ignored clicks when the embedded Stepper was removed from keyboard focus. | Keep the Stepper pointer-targetable independently of tab focus. `nativeui_widget_number_input_tests` clicks increment and decrement and checks the shared value. |
| G2 | NumberInput | The Stepper stretched across the TextInput label and field, rather than aligning with the numeric field. | Place the Stepper at the resolved field's vertical center. The layout test checks its bounds; full-height gallery captures check the result. |
| G3 | SearchField and FindBar | A hidden editor label still reserved an 82-point control height, stretching the search and find bars. | Use compact geometry for the embedded editor. `nativeui_widget_search_field_tests` checks the preferred height; gallery captures check both screens. |
| G4 | SearchField | The visible clear action was disabled at first mount for a non-empty query. | Compute initial availability from the source, then check mount and mutation guards on activation. The test clicks the clear action and checks the query. |
| G5 | EditableComboBox | An empty-label editor inside a Form still reserved label space, making the Font control twice as tall as neighboring fields. | Use compact geometry when the composite label is empty. `nativeui_widget_editable_combo_box_tests` checks the preferred height. |
| G6 | EditableComboBox | The Options action was disabled at first mount. | Compute initial availability without the mount flag. The composite action test checks availability and dispatches a pointer click. |
| G7 | DateInput | The initially visible Clear date action was disabled. | Compute availability from the optional value and source; the composite action test clears an initial date through `UI` input. |
| G8 | TimeInput | The initially visible Clear time action was disabled. | Apply the same initial availability rule; the composite action test clears an initial time through `UI` input. |
| G9 | TokenField | The remove action on an initial tag was disabled. | Evaluate source validity and dataset generation before mount; the composite action test removes the tag through `UI` input. |
| G10 | Calendar | Previous and next month buttons were disabled on the first display. | Allow navigation actions to be available when the source exists; the composite action test clicks Next month and checks the selected date. |
| G11 | Containers gallery | The Stack demonstration printed its centered background text under the overlay badge. | Keep the colored background and show a single `STACK OVERLAY` badge. The full-height Containers capture checks legibility. |
| G12 | NumberInput state styles | The Stepper stayed at the normal field position when a focused TextInput style moved the numeric field. | Read the mounted editor's resolved field geometry during layout. A focus-transition regression checks the Stepper center against the focused field. |

The same PR validation and independent review exposed three retained-lifetime
defects, one Windows font-selection defect, and one Windows packaging collision
outside the visual gallery pass:

| ID | Surface | Confirmed problem | Correction and executable check |
| --- | --- | --- | --- |
| V1 | TextInput | Copying a key or submit callback can retire its own editor and free the callback before the copy finishes. Linux ASan reported a use-after-free in the key test. | Keep component callbacks in independently owned storage during copying. The key retirement test covers both key and submit copies. |
| V2 | Link | Copying a navigation callback can retire its own link and free the callback before the copy finishes. Linux ASan reported a use-after-free. | Keep the callback storage alive across copying; `nativeui_widget_link_tests` covers self-removal. |
| V3 | Retained descendant actions | A blur callback can deactivate the tree while descendant actions drain; the subsequent availability pass dereferenced a null platform pointer. Linux UBSan reported this in the existing deactivation test. | Recheck activation and platform after draining before the final availability pass. |
| V4 | RichText on Windows | The DirectWrite manager can resolve a font for a character without exposing a default family; empty-coverage metrics lookup then fails. Windows CI reported failures in the gallery, RichText tests, and installed-package Unicode fixture. | Retry metrics font selection with a space only when the ordinary lookup fails. The existing Windows RichText and package tests qualify this path. |
| V5 | Windows ICU packaging | Two modules built with different Skia ICU payloads could stage the same `icudtl.dat` in one output directory, silently replacing the first module's data. | Compare the existing file under the staging lock and fail before overwrite if bytes differ. A portable fixture reproduces the conflict, verifies the original bytes survive, and rebuilds the first module successfully. |

## Screen coverage

| Screen | Components inspected in the gallery | Visual result after corrections |
| --- | --- | --- |
| Controls and actions | Button, Link, PopupMenu, ContextMenu, ToggleButton, ToggleGroup, SegmentedControl, Checkbox, CheckboxGroup, RadioButton, Toggle, ComboBox, Slider, RangeSlider, Knob, Stepper, NumberInput, Rating | No additional confirmed layout defect. The last card requires scrolling at 900 points. |
| Input and forms | Form, Field, Fieldset, TextInput, NumberInput, EditableComboBox, Autocomplete, SearchField, TokenField, EditableText, TextArea, FindBar | Corrected field heights and actions above. |
| Dates and colors | Calendar, DateInput, TimeInput, ColorPicker, ColorWell | Corrected initial calendar and clear actions above. |
| Collections | ListView, TableView, TreeView, OutlineView, OutlineTableView, GridView, Tabs, Breadcrumbs, HistoryButton | No additional confirmed visual defect. |
| Containers | Row, Column, Grid, Scroll, ScrollView, Clip, Flex, Spacer, Stack, Padding, SplitView, Collapsible, Accordion | Corrected Stack example overlap above. |
| Composition | Visibility, Enabled, ReadOnly, If, Switch, ForEach, FocusScope, CommandScope, StyleScope | No additional confirmed visual defect. |
| Text and drawing | Label, Header, RichText, Canvas, Divider, ImageView, IconView, Avatar | No additional confirmed visual defect. |
| Messages and indicators | Dialog, Popover, Tooltip, Toast, ProgressBar, Spinner, Meter, Badge | No additional confirmed visual defect. |

These screen inspections do not claim that every possible interaction of every
component was manually exercised. The confirmed interaction defects have direct
regression tests, and the complete local CTest suite is the broad functional
gate. Native Accessibility inspection and platform-specific pointer behavior
remain separate qualification surfaces.
