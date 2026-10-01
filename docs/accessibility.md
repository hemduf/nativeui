# NativeUI v1 accessibility semantics

Status: normative T045 design freeze for T067/T068, amended by the two platform-constant substitutions recorded in [§0](#0-implementation-status); the normative sections below are otherwise unchanged. Production native accessibility bridges are implemented by T068.

## 0. Implementation status

Implementation is active on PR #241 (T068 / issue #80); T045 remains the authority for roles, actions, mappings and lifetime rules, and this section is status/evidence bookkeeping only.

| Slice | State |
| --- | --- |
| Immutable per-view semantic snapshots, deterministic diff and native publication | Implemented; locally validated |
| T065 action routing with execution-time eligibility recheck | Implemented; locally validated |
| T067 shared virtual metadata, lazy logical items and logical Select/Focus | Implemented; locally validated |
| Standard-widget projections (§4) | Implemented; locally validated |
| macOS NSAccessibility bridge and per-view publication sink | Implemented; locally validated with an in-process AppKit query fixture plus the consumer-prefix audit. A real AXUIElement/VoiceOver session needs TCC consent and stays a manual checklist item, not an automated gate. |
| Windows UIA fragment provider | Neutral mapping/provider core implemented and covered by host-independent suites; the `_WIN32` COM adapter and the Windows client fixture are compiled and registered, but their runtime evidence is pending remote Windows CI. |
| Linux AT-SPI2 over T072 | Not implemented. The fail-closed stub remains until T181 (issue #464 / PR #465) lands the explicit accessibility-bus address mode for the T072 transport. |

Local validation: serial macOS Release build with zero unapproved warnings and full local CTest 239/240. The only failure is `nativeui_fractal_noise_gpu_reference_tests` (T092 fractal-noise GPU divergence on the local Apple M1 Pro), tracked as Bug #479 and unrelated to T068. The new concurrent reader suite also passes locally under a Debug ASan+UBSan build (leak detection is unavailable on macOS; the Linux CI job keeps `detect_leaks=1`).

Remaining: Linux AT-SPI2 (T181 / #464 / PR #465) and remote Windows UIA runtime evidence. The dedicated concurrent reader-vs-publisher stress is `nativeui_accessibility_concurrent_snapshot_readers`: six reader threads repeatedly load the atomically published immutable generation while the owning thread publishes 512 tagged generations, deterministically prove per-reader overlap of at least 48 distinct generations through a lockstep handshake (no sleeps), and keep generation 1/2 publications readable through retirement. Sanitizer limitation: the repository configures ASan+UBSan only; there is no TSan target, and ASan/UBSan cannot prove the absence of data races, so race-freedom rests on the atomic `shared_ptr` publication contract plus this suite's deterministic consistency/overlap assertions rather than on sanitizer instrumentation.

### T045 amendments (recorded)

1. **Dialog mapping.** The §7 Dialog row uses `NSAccessibilityWindowRole` + `NSAccessibilityDialogSubrole` because the originally frozen `NSAccessibilityDialogRole` does not exist in AppKit on the pinned SDK. This is a constant substitution preserving the same role semantics; no role was added, renamed or removed.
2. **Children-changed notification.** The historical `NSAccessibilityChildrenChangedNotification` constant is absent from the pinned SDK. A committed structure transition announces the root transition with `NSAccessibilityCreatedNotification` / `NSAccessibilityUIElementDestroyedNotification` plus `NSAccessibilityLayoutChangedNotification` for the structure change, exactly as recorded in §7.

### Acceptance-criteria matrix (issue #80)

All CTest names below are root-tree targets; `nativeui_accessibility_root_integration_contract` guards the T068 registrations, their labels and the isolated public-header target, and platform-only suites are registered when that platform is built. Paths are relative to the repository root.

| Acceptance criterion | Implementation | CTest coverage |
| --- | --- | --- |
| Native read queries on immutable snapshots only; no retained/traversed live-tree pointer | `include/nativeui/detail/semantic_snapshot.hpp`, `include/nativeui/detail/semantic_proxy.hpp`, `include/nativeui/detail/semantic_native_query.hpp`, `include/nativeui/detail/semantic_native_view_bridge.hpp` | `nativeui_accessibility_semantic_proxy`, `nativeui_accessibility_semantic_children_query`, `nativeui_t045_semantics` |
| Generation advances only when exposed semantic data changes | `SemanticSnapshotPublisher::publish` (`include/nativeui/detail/semantic_snapshot.hpp`), `SemanticNativePublicationState::publish` (`include/nativeui/detail/semantic_native_publication.hpp`) | `nativeui_accessibility_semantic_native_generation`, `nativeui_accessibility_semantic_view_state`, `nativeui_smoke_accessibility` |
| Concurrent readers keep the old immutable generation while a new one is published | Atomic `shared_ptr` publication plus the lifetime-safe weak reader source (`include/nativeui/detail/semantic_snapshot.hpp`, `include/nativeui/detail/semantic_native_publication.hpp`) | `nativeui_accessibility_concurrent_snapshot_readers`, `nativeui_accessibility_semantic_proxy`, `nativeui_accessibility_semantic_proxy_cache`, `nativeui_accessibility_semantic_native_generation` |
| Removed/stale ordinary and virtual proxies become defunct safely | `SemanticSnapshotProxy` / `SemanticProxyCache` (`include/nativeui/detail/semantic_proxy.hpp`), `include/nativeui/detail/semantic_virtual_source.hpp` | `nativeui_accessibility_semantic_proxy`, `nativeui_accessibility_semantic_proxy_cache`, `nativeui_accessibility_virtual_list_action_tests`, `nativeui_semantic_macos_proxy_cache` |
| T065 action executes at most once after current-state eligibility recheck | `include/nativeui/detail/semantic_action.hpp`, `include/nativeui/detail/semantic_action_target_binding.hpp`, `include/nativeui/detail/semantic_action_view_binding.hpp`, `include/nativeui/detail/semantic_live_action.hpp` | `nativeui_accessibility_semantic_action_router`, `nativeui_accessibility_semantic_action_target_binding`, `nativeui_accessibility_semantic_action_view_binding`, `nativeui_accessibility_retained_action_bridge_tests` |
| Disabled/ReadOnly revalidated at execution | `include/nativeui/detail/semantic_rules.hpp` (`normalize_semantic_info`, `semantic_action_mutates_value`, `semantic_action_allowed`), `include/nativeui/detail/semantic_live_action.hpp` | `nativeui_accessibility_semantic_action_policy`, `nativeui_accessibility_semantic_read_only`, `nativeui_accessibility_retained_action_bridge_tests` |
| Deterministic diff categories; slider value never rebuilds the native root | `diff_semantic_snapshots` (`include/nativeui/detail/semantic_snapshot.hpp`), `include/nativeui/detail/semantic_native_publication.hpp`, `src/detail/native_accessibility_macos.mm` | `nativeui_accessibility_semantic_diff_regression`, `nativeui_accessibility_semantic_publication_batch`, `nativeui_semantic_macos_notifications` |
| Bounds convert exactly once through T043 and stay correct after scale/scroll/layout | `src/detail/semantic_native_bounds.hpp`, `src/detail/native_screen_origin.hpp`, `src/detail/view_geometry.hpp`, `include/nativeui/detail/semantic_native_publication.hpp` | `nativeui_accessibility_semantic_native_bounds`, `nativeui_accessibility_semantic_native_geometry_capture`, `nativeui_smoke_accessibility` |
| 100k dataset: 1,000 scroll/selection/focus publications retain the same T067 metadata pointer with zero O(N) copy | `include/nativeui/detail/semantic_view_state.hpp`, `VirtualSemanticChildren` (`include/nativeui/semantics.hpp`), T067 `include/nativeui/detail/virtual_list_model.hpp` | `nativeui_accessibility_semantic_view_state`, `nativeui_t067_semantic_api_tests`, `nativeui_example_accessibility_self_test` |
| Dataset replacement swaps one new T067 metadata generation; old readers stay safe | T067 `include/nativeui/detail/virtual_list_model.hpp` / `include/nativeui/detail/virtual_list_retained.hpp`, `virtual_storage_refresh_required` (`include/nativeui/detail/semantic_snapshot.hpp`) | `nativeui_accessibility_semantic_view_state`, `nativeui_t067_semantic_api_tests`, `nativeui_example_accessibility_self_test` |
| 100k virtual collection exposes full logical children without eager proxies or visual rows | `include/nativeui/detail/semantic_virtual_source.hpp`, T067 immutable metadata, lazy providers in `include/nativeui/detail/semantic_uia_provider.hpp` | `nativeui_t067_semantic_api_tests`, `nativeui_accessibility_semantic_children_query`, `nativeui_accessibility_uia_provider`, `nativeui_example_accessibility_self_test` |
| Offscreen virtual Select/Focus runs as a logical action, then normal T067 scroll/materialization | `include/nativeui/detail/semantic_live_action.hpp`, `include/nativeui/detail/virtual_list_semantic_action.hpp`, `include/nativeui/detail/semantic_action.hpp` | `nativeui_accessibility_virtual_list_action_tests`, `nativeui_accessibility_semantic_action_policy`, `nativeui_example_accessibility_self_test` |
| macOS VoiceOver-representative fixture works with consumer-prefixed runtime names | `src/detail/native_accessibility_macos.mm`, `src/detail/semantic_macos_*.hpp`, `tests/smoke_accessibility_appkit.mm` | `nativeui_semantic_macos_production_bridge`, `nativeui_semantic_macos_notifications`, `nativeui_accessibility_objc_runtime_prefix_probe`, `nativeui_smoke_accessibility`, `nativeui_smoke_accessibility_objc_runtime_prefix` |
| Windows UIA/Narrator representative fixture works | `include/nativeui/detail/semantic_uia_mapping.hpp`, `include/nativeui/detail/semantic_uia_provider.hpp`, `src/detail/native_accessibility_windows.cpp` | `nativeui_accessibility_uia_mapping`, `nativeui_accessibility_uia_provider` (host-independent); `nativeui_accessibility_uia_win32_provider_tests` and the `nativeui_smoke_accessibility` Windows fixture run on Windows CI (evidence pending) |
| Linux AT-SPI2 works via T072; missing bus disables once/boundedly | Pending T181 (#464 / PR #465); fail-closed `src/detail/native_accessibility_stub.c` until then | None yet |
| Two views keep isolated roots/snapshots/proxy caches/transports | `include/nativeui/detail/semantic_native_view_bridge.hpp`, `include/nativeui/detail/semantic_platform_identity.hpp`, per-view caches and sinks | `nativeui_accessibility_semantic_native_view_bridge`, `nativeui_accessibility_semantic_proxy_cache`, `nativeui_semantic_macos_proxy_cache`, `nativeui_accessibility_uia_provider`, `nativeui_accessibility_publication_sink_tests` |
| Public accessibility headers contain no platform/D-Bus/Pugl types | `include/nativeui/semantics.hpp`, umbrella export in `include/nativeui/nativeui.hpp` | `nativeui_accessibility_root_integration_contract` (guards the isolated `tests/headers/semantics.cpp` compile target) |

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

T062 implements that rule through the T045 `Component::semantics()` seam: the role-`None` Tooltip decorator publishes its owned tooltip text as a description, which T068 flattens onto the decorated anchor without reading the rendered overlay. An empty tooltip string contributes no description and never overwrites a description supplied by the decorated child itself.

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

`Component::semantics()` is a publication/advertisement seam only: it declares the exposed role, state and action vocabulary, and platform bridges derive their advertised actions and patterns from it. `Focus` needs no widget handler; the retained tree executes it through its focus manager. Executing any other advertised action requires the component to implement the internal detail interface `ui::detail::SemanticActionHandler` (`perform_semantic_action`), which is intentionally not part of the public API. A component that advertises actions without implementing it remains a valid read-only semantic projection, but those requests fail closed at dispatch: the platform bridge may already have acknowledged a request once it passed the immutable-snapshot and live eligibility rechecks, and the request is then rejected execution-time when no handler exists instead of a mutation silently running. Standard widgets ship that internal implementation for their T045 action sets. This also applies to a custom component that selects a standard role such as `Custom`/`Button`: the role decides presentation and platform mapping, while execution still requires the internal handler (or the tree focus path for `Focus`).

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
| Dialog | `NSAccessibilityWindowRole` + `NSAccessibilityDialogSubrole` |
| Image | `NSAccessibilityImageRole` |
| Custom | `NSAccessibilityGroupRole` unless the application selected another standard `SemanticRole` |

`Activate`/`Toggle`/`Select` map to standard press/selection semantics as appropriate. `Increment` and `Decrement` map to standard increment/decrement actions. `SetValue` uses the writable value attribute. `Focus` uses focused-element semantics. `Expand`/`Collapse` use expanded-state semantics only where the role advertises them.

Virtual ListView children are exposed lazily through NSAccessibility children/index queries. Native proxies are per-view and lazy; no O(N) eager `NSAccessibilityElement` creation is allowed. Any runtime-visible Objective-C class added by T068 must use the T053 consumer-specific runtime prefix. Categories, swizzling, and `+load` are not permitted.

Production attach: one lazily allocated subclass of the actual consumer-prefixed native wrapper view (`<consumer-view-class>_NativeUIAccessibilityView`) is created per native view class and reused by every view of that class. The wrapper view itself is not an accessibility element; it exposes the current semantic root as its single `accessibilityChildren` entry and the focused semantic node, when any, through `accessibilityFocusedUIElement`. Attribute/action callbacks stay on the existing lazy per-view proxies, so removed roots become defunct and foreign/superseded batches cannot notify. Attach or proxy allocation failure disables accessibility for that view only and leaves the native view fully functional.

Committed batches map to the closed AppKit notification set on the main thread: `NSAccessibilityCreatedNotification` / `NSAccessibilityUIElementDestroyedNotification` for an announced root transition plus `NSAccessibilityLayoutChangedNotification` for the structure change (current SDKs no longer provide the historical children-changed constant), `NSAccessibilityFocusedUIElementChangedNotification` for focus, `NSAccessibilitySelectedChildrenChangedNotification` for selection, `NSAccessibilityValueChangedNotification` for values, and `NSAccessibilityLayoutChangedNotification` for bounds. A value-only update therefore never announces a root recreation and a bounds-only update never announces created/destroyed elements.

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
