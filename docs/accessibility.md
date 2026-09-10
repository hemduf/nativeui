# NativeUI v1 accessibility semantics

Status: T045 design freeze for T067/T068. This document is normative for the v1 accessibility implementation. Production native bridges are implemented by T068, not here.

## 1. Semantic identity and snapshots

Every exposed semantic object has an owned, backend-neutral `SemanticId`. Ordinary retained nodes use a deterministic identity derived from their retained `NodeId`; `0` is invalid. Native objects never retain `Node*`, `Component*`, or other live-tree pointers.

Each native view publishes immutable `SemanticTreeSnapshot` generations from the UI thread. A native reader retains a shared immutable generation, performs read-only queries from it, and may outlive publication of the next generation. Removed IDs resolve as defunct/absent. Native mutation/action requests are marshalled to the owning UI thread and re-resolved against current state before execution.

No process-global semantic registry, current semantic root, or `thread_local` semantic state is permitted. Snapshot/proxy/cache state is per native view.

## 2. Semantic roles

The closed v1 role set is the `SemanticRole` enum in `nativeui/semantics.hpp`:

`None`, `Button`, `Checkbox`, `RadioButton`, `Toggle`, `Slider`, `RangeSliderHandle`, `ProgressBar`, `Meter`, `Text`, `TextInput`, `TextArea`, `ComboBox`, `PopupMenu`, `MenuItem`, `ListView`, `ListItem`, `Tabs`, `Tab`, `TabPanel`, `Dialog`, `Group`, `Image`, `Custom`.

`None` means the node itself is flattened from the semantic tree unless it is needed as the owner of semantic children. Pure layout wrappers normally use `None`. `Group` is used only for intentionally exposed grouping semantics. `Custom` is the generic role for an application component that deliberately opts into semantics without selecting another standard role.

## 3. Semantic actions

The closed v1 action set is:

- `Activate`: invoke a button/menu-item/default action;
- `Toggle`: change check/toggle state;
- `Focus`: request NativeUI keyboard focus;
- `Increment` / `Decrement`: one logical value step;
- `SetValue`: set a numeric/text value when the role supports it;
- `Select`: select one logical item/tab/radio entry;
- `Expand` / `Collapse`: change expansion state for roles that expose it.

Only advertised actions may be requested. The platform bridge never writes widget internals directly and never synthesizes fake pointer/key input. It dispatches one semantic action to NativeUI on the UI thread, where existence, effective enabled/read-only state and current action eligibility are checked again.

Disabled nodes remain semantically present when meaningful but reject activation/mutation actions. Read-only editable/value nodes remain readable/focusable/selectable as applicable and reject value-changing actions. Focus and non-mutating navigation remain available when otherwise eligible.

## 4. Tree, availability and ordering

- `Collapsed`: subtree absent from semantics.
- `Hidden`: subtree absent from semantics by default.
- Disabled: present when meaningful, `enabled=false`.
- Read-only: present, `read_only=true`.
- Semantic child order follows logical reading/navigation order, never incidental paint-call order.
- A `None` layout wrapper is flattened while preserving descendant order.
- An explicit `Group` is retained as one semantic parent.
- Visible overlays are ordered after root content according to T061 creation/z order. A modal dialog becomes the active semantic focus domain; underlying content remains represented only according to the final T061/T063 modal policy implemented by T068.
- Tooltip visual overlay is not the source of accessible help; help/description belongs to the anchor semantic node.

## 5. Focus and geometry

Semantic bounds are logical view-relative `Rect` values. T043 is the sole logical-to-native/screen conversion authority; a platform bridge applies scale/translation exactly once.

Native accessibility focus requests dispatch `SemanticAction::Focus`. NativeUI keyboard focus remains authoritative. Focus notifications are emitted only from the resulting NativeUI focus state, preventing request/notification feedback loops.

## 6. Standard-widget semantic contract

| NativeUI surface | Role(s) | Principal properties | Actions when enabled/editable |
| --- | --- | --- | --- |
| Button | Button | name, enabled, focused | Activate, Focus |
| Checkbox | Checkbox | name, checked, enabled, focused | Toggle, Focus |
| RadioButton | RadioButton | name, selected/checked, group relation | Select, Focus |
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
| MenuItem | MenuItem | name, enabled, selected/checked when applicable | Activate/Select as declared |
| ListView | ListView | selected item + ordinary/virtual children | Focus |
| List item | ListItem | name, enabled, selected | Select, Focus, optional Activate |
| Tabs | Tabs | ordered Tab children | Focus |
| Tab | Tab | name, selected, enabled | Select, Focus |
| TabPanel | TabPanel | labelled/group relation | none |
| Dialog | Dialog | name/title, modal state | Focus and child actions |
| Group/container | Group | optional name | none |
| Image/Icon | Image | accessible name/description when meaningful | none |
| Custom | Custom or chosen standard role | application-provided fields | application-advertised subset |

Radio group membership and tab-to-panel relationships are semantic relationships represented by stable semantic IDs in the T068 snapshot implementation; they are never raw component pointers.

## 7. Custom-component seam

The v1 component semantic hook is fixed conceptually as a platform-neutral `SemanticInfo` producer on a retained component/context. T068 may implement it as a virtual `Component::semantics()` or an equivalent context hook, but the observable contract is fixed:

- returns owned/value semantic data using only NativeUI public types;
- no Pugl/AppKit/Win32/Xlib/UIA/AT-SPI types in the public signature;
- no native object ownership is transferred to application code;
- a default component with role `None` is flattened;
- virtual-collection capability is separate and optional, used by ListView rather than required for ordinary custom components.

T068 must not introduce a second incompatible custom semantic API.

## 8. Virtual collection contract

T067 ListView exposes the full logical dataset without materializing every visual row.

Each accepted logical key receives a non-zero `VirtualSemanticItemToken`. The token is **not a hash**. It is minted by the owning ListView when a key first enters the accepted dataset, remains stable while that exact logical key remains present (including reorder and metadata updates), and is never reused for another live or stale item identity during that ListView lifetime. If a key is removed and later reinserted, it receives a new token so old native proxies remain defunct.

Native virtual item identity is the pair `{ListView SemanticId, VirtualSemanticItemToken}`.

`VirtualSemanticChildren` is an immutable snapshot interface. A T067 implementation stores one O(N) immutable logical metadata object per accepted dataset generation and combines it with small current scalar/view state such as selected token, row height, scroll transform and viewport geometry. Creating a new semantic generation for selection/scroll/focus may create a new lightweight provider object but must retain the same O(N) metadata allocation/generation while the dataset is unchanged.

`size()` returns the full logical count. `item_at(index)` returns semantic metadata and lazily computed bounds for exactly one logical item and must never call the visual row factory, mount a component, or allocate all item proxies. `index_of_selected_item()` may use the provider's current scalar selection state and T067's documented lookup policy.

T067 owns key equality, dataset validation, token retention and metadata snapshot construction. T068 owns native proxy creation and must create virtual item proxies lazily on query/action.

## 9. macOS mapping — NSAccessibility

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
| Tab | `NSAccessibilityRadioButtonRole` + tab-button subrole when supported |
| TabPanel / Group / Custom | `NSAccessibilityGroupRole` unless a more specific standard role is supplied |
| Dialog | `NSAccessibilityDialogRole` |
| Image | `NSAccessibilityImageRole` |

`Activate`/`Toggle`/`Select` use press/selection semantics as appropriate; `Increment` and `Decrement` map to the standard increment/decrement actions; `SetValue` uses the writable value attribute; `Focus` uses focused-element semantics; `Expand`/`Collapse` use expanded-state semantics where the role supports it.

Virtual ListView queries expose logical children lazily through NSAccessibility children/index queries. Native proxy objects are per-view and lazy; no 100k eager `NSAccessibilityElement` construction is allowed. Any T068 runtime-visible Objective-C class must use the T053 consumer-specific runtime prefix; no category/swizzle/`+load` solution is permitted.

## 10. Windows mapping — UI Automation

| SemanticRole | UIA ControlType / primary pattern |
| --- | --- |
| Button | Button / Invoke |
| Checkbox | CheckBox / Toggle |
| RadioButton | RadioButton / SelectionItem |
| Toggle | CheckBox / Toggle |
| Slider | Slider / RangeValue |
| RangeSliderHandle | Thumb / RangeValue |
| ProgressBar / Meter | ProgressBar / read-only RangeValue |
| Text | Text |
| TextInput | Edit / Value + Text where available |
| TextArea | Edit / Text + Value semantics as supported |
| ComboBox | ComboBox / ExpandCollapse + Selection |
| PopupMenu | Menu |
| MenuItem | MenuItem / Invoke or SelectionItem as declared |
| ListView | List / Selection + ItemContainer |
| ListItem | ListItem / SelectionItem; virtual items also expose VirtualizedItem when applicable |
| Tabs | Tab |
| Tab | TabItem / SelectionItem |
| TabPanel / Group / Custom | Group unless a more specific standard role is supplied |
| Dialog | Window control type within the NativeUI fragment; modal/focus state is semantic, not a second native HWND |
| Image | Image |

T068 exposes one fragment/provider root per NativeUI view. Providers retain semantic identity + weak bridge, never Component pointers. Stale objects return UIA element-not-available semantics.

Virtualized ListView uses `ItemContainerPattern` on the list and lazy item providers; an offscreen logical item may expose `VirtualizedItemPattern`/selection behavior without materializing a visual NativeUI row. No eager provider creation proportional to logical item count is permitted.

## 11. Linux/X11 mapping — AT-SPI2

| SemanticRole | AT-SPI2 role / principal interfaces |
| --- | --- |
| Button | PUSH_BUTTON / Action, Component |
| Checkbox | CHECK_BOX / Action, Component |
| RadioButton | RADIO_BUTTON / Action, Selection relation |
| Toggle | TOGGLE_BUTTON / Action |
| Slider / RangeSliderHandle | SLIDER / Value, Component |
| ProgressBar | PROGRESS_BAR / Value |
| Meter | LEVEL_BAR or closest supported value role / Value |
| Text | STATIC / Text when useful |
| TextInput | TEXT / Text, EditableText, Component |
| TextArea | TEXT / Text, EditableText, Component |
| ComboBox | COMBO_BOX / Action, Selection |
| PopupMenu | MENU |
| MenuItem | MENU_ITEM / Action |
| ListView | LIST / Accessible children + Collection/Selection where supported |
| ListItem | LIST_ITEM / Action/Selection state |
| Tabs | PAGE_TAB_LIST / Selection |
| Tab | PAGE_TAB / Action/Selection |
| TabPanel / Group / Custom | PANEL/SECTION equivalent according to exposed grouping semantics |
| Dialog | DIALOG |
| Image | IMAGE / Image interface where meaningful |

T068 uses T072 as the only D-Bus transport. AT-SPI read handlers may answer from immutable semantic snapshots on the T072 I/O thread. Mutating actions are posted through T065 to the UI thread. The root reports logical child count and resolves `GetChildAtIndex`/Collection queries lazily; virtual ListView enumeration never mounts visual rows or pre-creates 100k D-Bus objects.

If the accessibility bus is unavailable, T068 disables the Linux accessibility bridge with one bounded diagnostic/state transition. It does not busy-poll or add another D-Bus stack.

## 12. Native proxy lifetime

All native proxies store only:

- weak/lifetime-safe reference to their owning view accessibility bridge;
- `SemanticId`, or `{ListView SemanticId, VirtualSemanticItemToken}` for a virtual item;
- platform provider bookkeeping required by the OS API.

Every query resolves against a retained immutable snapshot. Every action is re-resolved against current live semantic state on the UI thread. Missing identity is reported as absent/defunct/element-not-available. Proxy caches are per view and may use weak values; they must not keep removed semantic content alive indefinitely.

## 13. Notification categories

T068 diffs successive exposed semantic snapshots into this closed set:

- `StructureChanged` — child insertion/removal/reorder or semantic flatten/group structure change;
- `FocusChanged` — focused semantic identity changed;
- `SelectionChanged` — selected item/tab/radio state changed;
- `ValueChanged` — text/numeric/checked/expanded value changed;
- `BoundsChanged` — exposed logical bounds changed without structure change.

One publication may emit multiple categories. Changes are coalesced per resulting semantic generation. Scroll/bounds changes do not become structure changes. Virtual ListView selection/bounds publication must retain the same T067 O(N) metadata generation when the dataset is unchanged.

## 14. Platform notification mapping

- macOS: post the corresponding NSAccessibility layout/children/focus/value/selected-children notifications on the appropriate AppKit thread using snapshot data.
- Windows: raise UIA structure-changed, automation-focus-changed, selection/value/property-changed events from the per-view provider root.
- Linux: emit AT-SPI object children-changed, state-changed:focused/selected, property/value/text and bounds-related events through T072.

Platform event coalescing may be stronger than one-event-per-field, but it may not invent a structure rebuild for a value-only update.

## 15. T068 implementation checklist

T068 must implement this design in the following order:

1. add the exact platform-neutral component semantic hook and standard-widget `SemanticInfo` producers;
2. build deterministic semantic-tree flattening/order/availability logic;
3. publish immutable per-view `SemanticTreeSnapshot` generations with no-op generation suppression;
4. add semantic action routing that revalidates current target/state on the UI thread through T065;
5. integrate T067 virtual metadata/provider sharing without O(N) recopy on scroll/selection/focus;
6. implement per-view native proxy caches with stable IDs and safe stale behavior;
7. implement NSAccessibility mapping and consumer-prefixed runtime audit;
8. implement UIA mapping/provider patterns and lazy virtual list items;
9. implement AT-SPI2 over T072 only, including immutable off-thread read queries and UI-thread actions;
10. map/coalesce the five notification categories;
11. validate two-view isolation, stale proxies, concurrent old/new snapshot readers, native actions and representative screen-reader/platform fixtures;
12. preserve public-header isolation: no Pugl/AppKit/Win32/UIA/AT-SPI/D-Bus implementation type enters normal NativeUI public signatures.

Any native API constraint that contradicts this document requires reopening/amending T045 before T068 chooses an alternate architecture.
