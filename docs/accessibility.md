# NativeUI v1 accessibility semantics

Status: normative T045 design freeze for T067/T068. Production native accessibility bridges are implemented by T068, not here.

## 1. Semantic identity and snapshots

Every exposed semantic object has an owned backend-neutral `SemanticId`; `0` is invalid. Ordinary retained semantic IDs are derived deterministically from stable retained `NodeId` identity. Native accessibility objects never retain `Node*`, `Component*`, or another live-tree pointer.

Each native view publishes immutable `SemanticTreeSnapshot` generations from the UI thread. A native reader retains one shared immutable generation and may outlive publication of later generations. Removed IDs resolve as absent/defunct. Native mutation/action requests are marshalled to the owning UI thread and re-resolved against current state before execution.

Snapshot/proxy/cache state is per native view. No process-global semantic registry, current semantic root, singleton, or `thread_local` semantic state exists.

## 2. Closed v1 roles and actions

The closed role set is the `SemanticRole` enum in `nativeui/semantics.hpp`: `None`, `Button`, `Checkbox`, `RadioButton`, `Toggle`, `Slider`, `RangeSliderHandle`, `ProgressBar`, `Meter`, `Text`, `TextInput`, `TextArea`, `ComboBox`, `PopupMenu`, `MenuItem`, `ListView`, `ListItem`, `Tabs`, `Tab`, `TabPanel`, `Dialog`, `Group`, `Image`, `Custom`.

`None` means the node itself is flattened unless needed as the owner of exposed semantic descendants. Pure layout wrappers normally use `None`. `Group` is an explicitly exposed semantic grouping node. `Custom` is the generic application role when no more specific standard role is selected.

The closed action set is `Activate`, `Toggle`, `Focus`, `Increment`, `Decrement`, `SetValue`, `Select`, `Expand`, `Collapse`.

Only advertised actions may be requested. The platform bridge never writes widget internals directly and never synthesizes pointer/key events for accessibility. It dispatches one semantic action to NativeUI on the UI thread, where identity, effective enabled/read-only state, and current action eligibility are checked again.

Disabled nodes remain present when meaningful and reject activation/mutation actions. Read-only editable/value nodes remain readable/focusable/selectable as applicable and reject value-changing actions. Focus and non-mutating navigation remain available when otherwise eligible.

## 3. Tree, availability, focus, and geometry

- `Collapsed`: subtree absent from semantics.
- `Hidden`: subtree absent from semantics by default.
- Disabled: present when meaningful with `enabled=false`.
- Read-only: present with `read_only=true`.
- Child order follows logical reading/navigation order, never incidental paint-call order.
- A `None` layout wrapper is flattened while preserving descendant order.
- An explicit `Group` remains as one semantic parent.
- Visible overlays follow T061 creation/z order after root content.
- A T063 modal dialog is the active semantic focus domain while visible; underlying content is not exposed as an actionable focus domain while the modal is active.
- Tooltip visual overlay is not the semantic source of help text; help/description belongs to the anchor node.

Semantic bounds are NativeUI logical view-relative `Rect` values. T043 is the sole logical-to-native/screen conversion authority and a platform bridge applies scale/translation exactly once.

Native focus requests dispatch `SemanticAction::Focus`. NativeUI keyboard focus remains authoritative. Native focus notifications are emitted only from resulting NativeUI focus state, preventing request/notification feedback loops.

## 4. Standard-widget semantic contract

| NativeUI surface | Role(s) | Principal properties | Actions when enabled/editable |
| --- | --- | --- | --- |
| Button | Button | name, enabled, focused | Activate, Focus |
| Checkbox | Checkbox | name, checked, enabled, focused | Toggle, Focus |
| RadioButton | RadioButton | name, selected/checked, parent radio group | Select, Focus |
| Toggle | Toggle | name, checked, enabled | Toggle, Focus |
| Slider | Slider | numeric value/range, read-only | Increment, Decrement, SetValue, Focus |
| RangeSlider | two RangeSliderHandle children | each handle value/range | Increment, Decrement, SetValue, Focus |
| ProgressBar | ProgressBar | numeric value/range, read-only | none |
| Meter | Meter | numeric value/range, read-only | none |
| Label/Text | Text | name/text value | none |
| TextInput | TextInput | value, editable/read-only, focus | SetValue when editable, Focus |
| TextArea | TextArea | value, editable/read-only, focus | SetValue when editable, Focus |
| ComboBox | ComboBox | value, expanded, enabled | Expand, Collapse, Select, Focus |
| PopupMenu | PopupMenu | ordered MenuItem children | Focus where applicable |
| MenuItem | MenuItem | name, enabled, selected/checked when applicable | Activate or Select as declared |
| ListView | ListView | selected item + ordinary/virtual children | Focus |
| ListItem | ListItem | name, enabled, selected | Select, Focus, optional Activate |
| Tabs | Tabs | ordered Tab/TabPanel children | Focus |
| Tab | Tab | name, selected, enabled | Select, Focus |
| TabPanel | TabPanel | parent Tabs + paired Tab relation | none |
| Dialog | Dialog | name/title, modal state | Focus and child actions |
| Group/container | Group | optional name | none |
| Image/Icon | Image | accessible name/description when meaningful | none |
| Custom | Custom or selected standard role | application-provided fields | application-advertised subset |

Radio-group membership is encoded by semantic hierarchy: one exposed `Group` owns its `RadioButton` children. Tabs similarly owns its `Tab` and `TabPanel` children in logical order; the selected Tab and visible TabPanel are paired by stable selected-key semantics from T036. T068 may emit platform-native labelled/controlled relationships from that frozen hierarchy but does not invent raw component-pointer relationships.

## 5. Exact custom-component seam

T068 must add exactly one ordinary component hook with this public capability:

```cpp
virtual SemanticInfo Component::semantics() const;
```

The default implementation returns `SemanticInfo{}` with `role == SemanticRole::None`, which flattens the component while preserving semantic descendants. An application custom component overrides this hook to return owned/value semantic data. The signature uses NativeUI public types only and transfers no native object ownership.

Virtual-collection semantics are a separate ListView capability and are not part of the ordinary custom-component hook. T068 must not introduce a second competing ordinary semantic callback/provider API.

## 6. Virtual collection identity and immutable data

T067 ListView exposes the full logical dataset without materializing every visual row.

Each accepted logical key receives a non-zero `VirtualSemanticItemToken`. The token is **not a hash**. The owning ListView mints it when a key first enters an accepted dataset, retains it while that exact key remains present through reorder/metadata updates, and never reuses it for another live or stale identity during that ListView lifetime. If a key is removed and later reinserted, it receives a new token, so an old native proxy stays defunct.

Native virtual item identity is exactly `{ListView SemanticId, VirtualSemanticItemToken}`.

`VirtualSemanticChildren` is a concrete immutable, data-only snapshot value. T067 creates one O(N) immutable metadata allocation for each accepted dataset generation and combines it with cheap current scalar/view state: selected token, fixed row height, scroll transform, and list bounds. A new semantic generation caused by selection/scroll/focus may construct another cheap `VirtualSemanticChildren` value but must retain the exact same O(N) metadata pointer/generation while the dataset is unchanged.

`size()` reports the full logical item count. `item_at(index)` reads exactly one immutable metadata entry, resolves selected state from the small current selected token, and computes fixed-height logical bounds arithmetically. It never calls application code, a visual row factory, or retained-tree mutation. `index_of_selected_item()` follows T067's documented O(N) lookup policy; no hidden key-index cache is introduced.

T067 owns key equality, dataset validation, token retention, finite fixed-height geometry validation, and metadata snapshot construction. `VirtualSemanticChildren::from_metadata()` is consumed only with already validated T067 data. T068 shares this immutable metadata pointer; it must never recopy O(N) virtual item metadata on ordinary semantic publication.

## 7. macOS — exact NSAccessibility mapping

| SemanticRole | NSAccessibility mapping |
| --- | --- |
| Button | `NSAccessibilityButtonRole` |
| Checkbox | `NSAccessibilityCheckBoxRole` |
| RadioButton | `NSAccessibilityRadioButtonRole` |
| Toggle | `NSAccessibilityCheckBoxRole` with checked value semantics |
| Slider / RangeSliderHandle | `NSAccessibilitySliderRole` |
| ProgressBar | `NSAccessibilityProgressIndicatorRole` |
| Meter | `NSAccessibilityLevelIndicatorRole` |
| Text | `NSAccessibilityStaticTextRole` |
| TextInput | `NSAccessibilityTextFieldRole` |
| TextArea | `NSAccessibilityTextAreaRole` |
| ComboBox | `NSAccessibilityComboBoxRole` |
| PopupMenu | `NSAccessibilityMenuRole` |
| MenuItem | `NSAccessibilityMenuItemRole` |
| ListView | `NSAccessibilityListRole` |
| ListItem | `NSAccessibilityRowRole` |
| Tabs | `NSAccessibilityTabGroupRole` |
| Tab | `NSAccessibilityRadioButtonRole` with tab-button subrole when available on the deployment target |
| TabPanel | `NSAccessibilityGroupRole` |
| Group | `NSAccessibilityGroupRole` |
| Dialog | `NSAccessibilityDialogRole` |
| Image | `NSAccessibilityImageRole` |
| Custom | `NSAccessibilityGroupRole` unless the application selected another standard `SemanticRole` |

`Activate`/`Toggle`/`Select` map to standard press/selection semantics as appropriate. `Increment` and `Decrement` map to standard increment/decrement actions. `SetValue` uses the writable value attribute. `Focus` uses focused-element semantics. `Expand`/`Collapse` use expanded-state semantics only where the role advertises them.

Virtual ListView children are exposed lazily through NSAccessibility children/index queries. Native proxies are per-view and lazy; no O(N) eager `NSAccessibilityElement` creation is allowed. Any runtime-visible Objective-C class added by T068 must use the T053 consumer-specific runtime prefix. Categories, swizzling, and `+load` are not permitted.

## 8. Windows — exact UI Automation mapping

| SemanticRole | UIA ControlType / required primary pattern |
| --- | --- |
| Button | Button / Invoke |
| Checkbox | CheckBox / Toggle |
| RadioButton | RadioButton / SelectionItem |
| Toggle | CheckBox / Toggle |
| Slider | Slider / RangeValue |
| RangeSliderHandle | Thumb / RangeValue |
| ProgressBar | ProgressBar / read-only RangeValue |
| Meter | ProgressBar / read-only RangeValue |
| Text | Text |
| TextInput | Edit / Value + Text where the platform requests text access |
| TextArea | Edit / Text + Value semantics |
| ComboBox | ComboBox / ExpandCollapse + Selection |
| PopupMenu | Menu |
| MenuItem | MenuItem / Invoke or SelectionItem according to advertised NativeUI action |
| ListView | List / Selection + ItemContainer |
| ListItem | ListItem / SelectionItem; virtual logical items additionally expose VirtualizedItem |
| Tabs | Tab |
| Tab | TabItem / SelectionItem |
| TabPanel | Group |
| Group | Group |
| Dialog | Window ControlType as an in-view UIA fragment element; no second native HWND |
| Image | Image |
| Custom | Group unless another standard `SemanticRole` was selected |

T068 exposes one fragment/provider root per NativeUI view. Providers retain semantic identity plus a weak bridge, never a `Component*`. Stale objects return UIA element-not-available semantics.

Virtualized ListView uses `ItemContainerPattern` on the list and lazy item providers. An offscreen logical item may expose `VirtualizedItemPattern` and selection/focus behavior without materializing a visual NativeUI row. No provider creation proportional to the logical item count occurs at publication time.

## 9. Linux/X11 — exact AT-SPI2 mapping

| SemanticRole | AT-SPI2 role / required interfaces |
| --- | --- |
| Button | `PUSH_BUTTON` / Action, Component |
| Checkbox | `CHECK_BOX` / Action, Component |
| RadioButton | `RADIO_BUTTON` / Action, Component + parent selection relation |
| Toggle | `TOGGLE_BUTTON` / Action, Component |
| Slider / RangeSliderHandle | `SLIDER` / Value, Component |
| ProgressBar | `PROGRESS_BAR` / Value, Component |
| Meter | `LEVEL_BAR` / Value, Component |
| Text | `STATIC` / Text when textual navigation is exposed |
| TextInput | `ENTRY` / Text, EditableText, Component |
| TextArea | `TEXT` / Text, EditableText, Component |
| ComboBox | `COMBO_BOX` / Action, Selection, Component |
| PopupMenu | `MENU` / Component |
| MenuItem | `MENU_ITEM` / Action, Component |
| ListView | `LIST` / Component, Selection, Collection-style child access |
| ListItem | `LIST_ITEM` / Component, selection state, advertised Action |
| Tabs | `PAGE_TAB_LIST` / Selection, Component |
| Tab | `PAGE_TAB` / Action, Component, selection state |
| TabPanel | `PANEL` / Component |
| Group | `PANEL` / Component |
| Dialog | `DIALOG` / Component |
| Image | `IMAGE` / Image, Component |
| Custom | `PANEL` / Component unless another standard `SemanticRole` was selected |

T068 uses T072 as the only D-Bus transport. AT-SPI read handlers may answer from immutable semantic snapshots on the T072 I/O thread. Mutating actions are posted through T065 to the UI thread. Full logical child count and indexed/collection queries are resolved lazily; virtual ListView enumeration never mounts visual rows or pre-creates O(N) D-Bus objects.

If the accessibility bus is unavailable, T068 disables the Linux accessibility bridge with one bounded diagnostic/state transition. It does not busy-poll and does not add another D-Bus stack.

## 10. Native proxy lifetime

All native proxies store only:

- a weak/lifetime-safe reference to the owning view accessibility bridge;
- `SemanticId`, or `{ListView SemanticId, VirtualSemanticItemToken}` for a virtual item;
- platform provider bookkeeping required by the OS API.

Every read resolves against a retained immutable snapshot. Every action is re-resolved against current live semantic state on the UI thread. A missing identity is reported as absent/defunct/element-not-available. Proxy caches are per view, may use weak values, and must not keep removed semantic content alive indefinitely.

## 11. Closed notification categories

T068 diffs successive exposed semantic snapshots into exactly:

- `StructureChanged`: child insertion/removal/reorder or semantic flatten/group structure change;
- `FocusChanged`: focused semantic identity changed;
- `SelectionChanged`: selected item/tab/radio state changed;
- `ValueChanged`: text/numeric/checked/expanded value changed;
- `BoundsChanged`: exposed logical bounds changed without structure change.

One publication may emit multiple categories. Changes are coalesced per resulting semantic generation. Scroll/bounds changes are not structure changes. Virtual ListView selection/bounds publication retains the same T067 O(N) metadata generation when the dataset is unchanged.

Platform notification mapping is fixed:

- macOS: NSAccessibility children/layout/focus/value/selected-children notifications on the appropriate AppKit thread from snapshot data;
- Windows: UIA structure-changed, automation-focus-changed, selection/value/property-changed events from the per-view provider root;
- Linux: AT-SPI object children-changed, focused/selected state, value/text/property, and bounds events through T072.

Platform event coalescing may be stronger than one event per field, but a value-only update may not be represented as a root/structure rebuild.

## 12. Exact T068 implementation order

T068 implements this design in this order:

1. add `virtual SemanticInfo Component::semantics() const` with the default `None` implementation and standard-widget producers;
2. build deterministic semantic-tree flattening/order/availability logic;
3. publish immutable per-view `SemanticTreeSnapshot` generations with no-op generation suppression;
4. route semantic actions through T065, revalidating current identity and eligibility on the UI thread;
5. integrate T067 virtual metadata sharing with no O(N) recopy on scroll/selection/focus;
6. implement per-view native proxy caches with stable IDs/tokens and safe stale behavior;
7. implement NSAccessibility mapping plus consumer-prefixed Objective-C runtime audit;
8. implement UIA fragment/pattern mapping and lazy virtual list items;
9. implement AT-SPI2 over T072 only, with immutable off-thread reads and UI-thread actions;
10. map/coalesce the five notification categories;
11. validate two-view isolation, stale proxies, concurrent old/new snapshot readers, actions, and representative platform/screen-reader fixtures;
12. preserve public-header isolation: no Pugl/AppKit/Win32/UIA/AT-SPI/D-Bus implementation type enters normal NativeUI public signatures.

Any native API constraint that contradicts this document requires reopening/amending T045 before T068 chooses another architecture.
