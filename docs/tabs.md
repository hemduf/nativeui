# Tabs<T>

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Tabs<T>` exists with key/label/panel tabs, Binding selection, style, and navigation. Source: [widgets_list_tabs.inc](../include/nativeui/detail/widgets_list_tabs.inc), `Tabs`, `TabsRuntime`, `TabsComponent`, `TabPanelComponent`.

MyGo `ui/tabs.go`, `Tabs`, and its use of `TabsBase` compose the header and application panels separately. NativeUI already owns panels; their mounted state is retained even while Collapsed. The port here is compatible extraction, not a new close/reorder tab API.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Current API to preserve:

```cpp
explicit Tabs(Binding<T> selection);
explicit Tabs(State<T>& selection);
template<class Child>
Tabs&& tab(T key,std::string label,Child&& panel,bool enabled=true) &&;
Tabs&& style(TabsStyle value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<int> page{0};
auto tabs = ui::Tabs<int>{page}.tab(0,"General",ui::Label{"Settings"})
    .tab(1,"Advanced",ui::Label{"Other options"});
```

Equal keys rejected with invalid_argument at `.tab`. Preserve implicit deduction guides and equality-comparable user T without a hash requirement. There are currently no on_change callbacks; application models observe their State.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Owned Binding, labels/keys/enabled/Spec copied or moved into runtime. The external selected key determines the visible panel even if the tab is disabled; disabled prevents user navigation to this tab without forcing a new model. Unknown selected key = no visible panel, no correction/default writes. Panels are mounted, and availability collapses them according to selected key.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- Left/Right cycle among enabled tabs with wrapping, Home/End first/last.
- Entering focus uses the enabled selection or first enabled tab as active without automatically writing the Binding.
- Click down captures and arms an index; up on the same tab chooses, release outside the tab cancels.
- PointerCancel releases capture; hover only on enabled nonselected tabs.
- No wheel/close/reorder/Enter shortcut added during extraction.
- Automatic activation navigation: arrows write selection directly, without manual confirmation mode.
- Safety target: invalid Binding refuses writes; availability/read-only is revalidated when selecting.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Header with resolved TabsStyle height, tabs evenly sized at `bounds.w/count`. Panels below header+panel_gap receive identical bounds; nonselected collapsed panels do not contribute. Preferred width at least 120 DIP, minimum width zero; header bounded by available height. No automatic header scrolling or natural tab widths added. Empty tabs do not divide by zero.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

TabsStyle/list_tabs_style.hpp preserved with resolvers and VisualState. Selected underline/disabled text/focus/hover follow the current recipe. Style metrics header_height/panel_gap require layout; colors require paint. Panel subtrees retain local state since they remain mounted; no animation added.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Normative Tabs/Tab/TabPanel contract and paired-key relationship in accessibility.md. Complete hooks/native bridges must not be claimed as already delivered: the target implementation extends snapshots under the existing closed role set, stable nodes, and absent inactive panels. Focus requests go through Tree, not synthetic key actions.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

The shared runtime remains owned, with an RAII observer per panel. Choosing a new tab during input may hide a captured panel: availability recovery releases it at the checkpoint. A throwing setter observer leaves no active pressed/capture flag; a started callback is not replayed. Unmounted subscription inactive; invalid state get returns the last value, without an automatic destruction observer.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Binding<T>, TabsStyle, retained availability/focus, [Visibility](visibility.md). Cases: empty, all disabled, unknown/external disabled selected key, small bounds, long labels, equal duplicate key, and throwing equality. Do not import MyGo Router to choose a panel.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/tabs.hpp` and `src/tabs.cpp`.

tabs.hpp declares key/panel adapter templates and existing public types; tabs.cpp contains the non-template header/panel core, input/paint/layout with equality/selection adapters. widgets.hpp and list_tabs_style.hpp remain compatible includes; extract Tabs from widgets_list_tabs.inc without touching duplicate ListView implementations.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `tabs_legacy_keyboard`: enabled wrapping and Home/End.
- `tabs_panel_retention`: same mounted identities/state after switching.
- `tabs_disabled_external_key`: panel displayed without enabled interaction.
- `tabs_unknown_empty`: no panel/default write/division by zero.
- `tabs_pointer_cancel`: release outside tab, cancel, and recovered capture.
- `tabs_style_invalidation`: classified metrics versus colors.
- `tabs_setter_fault`: next input works after an observer throws.
- Reuse T036 list/tabs tests as the compatibility baseline.

Create the future public example `examples/features/tabs.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
