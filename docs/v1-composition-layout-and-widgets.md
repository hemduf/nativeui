# NativeUI 1.0 composition, layout, widgets and interaction

This chapter documents the stable NativeUI 1.0 retained composition, layout, widget and interaction model that is already implemented. It deliberately does **not** freeze the final `State<T>` / `Binding<T>` notification, reentrancy or lifetime wording while T123/T124 are still active. Those details must be reconciled before T122 is completed. Final package/API inventory belongs to T069 and the canonical copy-pasteable application journey belongs to T070.

## Retained composition model

NativeUI builds a retained component tree from declarative specifications. `Spec` is the public composition unit: builders such as layout containers and widgets produce specifications, and `UI` owns the resulting retained runtime tree.

The durable distinction is:

- **static composition** describes children whose structure is fixed after construction;
- **dynamic composition** changes the retained child set at a safe reconciliation checkpoint rather than mutating the tree directly from an arbitrary callback;
- component identity is retained by the tree, not by a process-global widget registry or hidden current-UI singleton.

Normal application code composes public builders and components. It should not include `nativeui/detail/` implementation headers or construct private retained-tree nodes directly.

### Dynamic composition

[`include/nativeui/dynamic.hpp`](../include/nativeui/dynamic.hpp) provides three public dynamic composition families:

- `If` conditionally includes one child;
- `Switch<T>` selects one branch, with an optional fallback;
- `ForEach<T>` reconciles a sequence by application-provided keys.

`ForEach<T>` keys are retained identity. Reordering items while preserving keys preserves their retained identity; changing a key replaces that item. Duplicate-key snapshots are rejected rather than partially mutating the currently valid retained tree. The maintained [`t058_dynamic_composition`](../examples/features/t058_dynamic_composition.cpp) example exercises conditional composition, branch switching, keyed reordering, duplicate-key rejection and recovery.

These helpers currently observe `State<T>` to request structural reconciliation. This chapter intentionally does not specify the final nested-observer, recursive-write, equality or subscription-destruction semantics of `State<T>` / `Binding<T>`; T123/T124 own that contract and this documentation must be reconciled against their frozen result.

## Availability and read-only state

Every retained component has effective availability derived from its local state and retained ancestors. The public `ComponentAvailability` model distinguishes:

- `VisibilityMode::Visible` — participates normally;
- `VisibilityMode::Hidden` — remains in layout but is not presented/interacted with;
- `VisibilityMode::Collapsed` — does not participate as normal visible layout content;
- `enabled == false` — the component is non-interactive;
- `read_only == true` — editing/mutation interactions are suppressed while the component can remain visible and otherwise present.

Availability is a retained-tree concern rather than a platform-widget flag. Focus, pointer targeting, overlays and widget behavior consume the effective retained availability so an unavailable descendant cannot continue acting as an interactive target merely because a native event arrives later.

The exact State/Binding notification ordering that can drive availability is still deferred to T123/T124. The stable 1.0 rule documented here is only the effective component behavior, not unfinished observer internals.

## Layout model

NativeUI layout is expressed in logical pixels. Platform scale/framebuffer conversion occurs below the retained component model; application layout code should not compensate manually for device scale.

The public layout entry point is [`include/nativeui/layout.hpp`](../include/nativeui/layout.hpp). The retained layout protocol is constraint based:

1. parents provide constraints;
2. children report minimum/preferred metrics within those constraints;
3. containers compute child placements;
4. retained bounds are then used consistently by painting, hit testing, focus and invalidation.

The stable container families include rows/columns, alignment, flex sizing, grid placement and scrollable content. Layout invalidation propagates through the retained tree; application code should request the appropriate NativeUI invalidation rather than manually calling private layout passes.

### Focused layout examples

The feature examples are the maintained executable references for individual layout families:

| Area | Focused example |
| --- | --- |
| constraints / bounded measurement | [`t007_constraints`](../examples/features/t007_constraints.cpp) |
| alignment | [`t008_alignment`](../examples/features/t008_alignment.cpp) |
| flex layout | [`t009_flex`](../examples/features/t009_flex.cpp) |
| grid layout | [`t010_grid`](../examples/features/t010_grid.cpp) |
| scroll/layout interaction | [`t012_scroll`](../examples/features/t012_scroll.cpp) |
| dynamic retained structure | [`t058_dynamic_composition`](../examples/features/t058_dynamic_composition.cpp) |

These focused programs also expose the repository `--self-test` convention. T070 remains the owner of the final production reference application, so this chapter links to focused examples instead of creating a second independent Getting Started snippet source.

## Standard widget families

[`include/nativeui/widgets.hpp`](../include/nativeui/widgets.hpp) is the normal umbrella for the standard widget set. NativeUI 1.0 includes stable families for:

- labels and basic content;
- buttons and activation behavior;
- checkbox/radio selection;
- slider and range-slider value input;
- progress display;
- single-line text input and multi-line text area;
- scroll views;
- list and tabs;
- combo-popup selection;
- virtualized lists for larger item sets.

Widgets are retained NativeUI components. They share the same layout, availability, focus, input, style/theme and invalidation model as custom components rather than delegating ownership to a second native-widget hierarchy.

Focused executable examples are the preferred behavioral references:

| Widget/interaction family | Focused example |
| --- | --- |
| button | [`t030_button`](../examples/features/t030_button.cpp) |
| checkbox / radio | [`t031_checkbox_radio`](../examples/features/t031_checkbox_radio.cpp) |
| slider | [`t032_slider`](../examples/features/t032_slider.cpp) |
| progress meter | [`t033_progress_meter`](../examples/features/t033_progress_meter.cpp) |
| scroll view | [`t034_scroll_view`](../examples/features/t034_scroll_view.cpp) |
| combo popup | [`t035_combo_popup`](../examples/features/t035_combo_popup.cpp) |
| list / tabs | [`t036_list_tabs`](../examples/features/t036_list_tabs.cpp) |
| text area | [`t028_text_area`](../examples/features/t028_text_area.cpp) |
| virtual list | [`t067_virtual_list`](../examples/features/t067_virtual_list.cpp) |

Widget-specific styling and animation are documented separately in the T122 styling/rendering chapter. This chapter focuses on retained behavior and interaction ownership.

## Event routing and interaction

NativeUI routes input through the retained tree in logical coordinates. Interaction state is owned by the relevant `UI`/retained tree; there is no process-wide focus target, pointer-capture target or gesture registry shared by unrelated UI instances.

### Bubbling and keyboard handling

Pointer and keyboard events are resolved against retained nodes and may bubble through retained ancestors according to the public input model. Components can handle relevant events without installing platform event hooks. [`t013_bubbling`](../examples/features/t013_bubbling.cpp) is the focused routing example.

Keyboard activation and editing behavior belongs to the focused widget/component that owns the semantic action. NativeUI does not require application code to dispatch raw Cocoa/Win32/X11 messages to widgets.

### Focus and focus scopes

Focus is retained per UI. Focus scopes establish local traversal/containment boundaries and are used by higher-level interaction such as modal dialogs. Losing availability can make a focused node ineligible and trigger retained focus reconciliation; this is not a native-widget ownership transfer.

[`t014_focus_scopes`](../examples/features/t014_focus_scopes.cpp) is the focused example for scope/traversal behavior.

The current T123 work touches the precise interaction between reentrant state notification and availability-driven focus restoration. This chapter therefore does not freeze that observer-ordering detail until T123/T124 are complete.

### Pointer capture and gestures

Pointer capture is retained per UI and ties an active pointer sequence to the owning component without creating a process-global capture registry. Capture lifetime follows the retained target: teardown or loss of eligibility must not leave a stale target.

[`t015_pointer_capture`](../examples/features/t015_pointer_capture.cpp) exercises capture behavior. [`t016_gestures`](../examples/features/t016_gestures.cpp) covers the gesture layer built on the same retained input stream.

Drag/drop integration is demonstrated by [`t018_drop`](../examples/features/t018_drop.cpp). Application code should consume the public NativeUI drop/input abstractions rather than reaching into platform event objects.

## Text input and IME

Text editing uses NativeUI's retained text/editing model while platform integration supplies native text/IME events. The stable conceptual boundary is:

- the retained text model owns text, selection/caret and editing state;
- the focused text component owns the editing interaction;
- platform IME/composition integration feeds the retained model through NativeUI rather than exposing AppKit/Win32/X11 implementation objects to normal application code;
- disabling/hiding/removing the editing target must not leave a stale active text target.

Focused references are [`t025_text_edit_model`](../examples/features/t025_text_edit_model.cpp), [`t028_text_area`](../examples/features/t028_text_area.cpp) and [`t029_ime_composition`](../examples/features/t029_ime_composition.cpp).

## Overlays

[`include/nativeui/overlay.hpp`](../include/nativeui/overlay.hpp) defines the generic retained overlay model. An `OverlaySpec` selects modal/non-modal behavior, pointer policy, optional retained anchor, placement policy and dismissal policy. Overlay content remains owned by the same UI domain; the overlay stack is not a second native-window model.

Stable placement modes include anchor-relative below/above/right/left, centered and automatic placement. Placement is resolved in logical coordinates and constrained to the owning viewport. Modal overlays participate in retained focus/input policy rather than installing an unrelated process-global modal loop.

[`t061_overlay_portal`](../examples/features/t061_overlay_portal.cpp) is the focused overlay example.

## Tooltip

[`include/nativeui/tooltip.hpp`](../include/nativeui/tooltip.hpp) builds tooltip behavior on the retained interaction/overlay and Dispatcher timer services. Tooltip state and timing are per controller/UI; there is deliberately no process-wide current-tooltip or shared warm-up singleton.

At a public-contract level:

- hover or focus can make a tooltip eligible after its delay;
- pointer-button interaction suppresses hover presentation during the interaction;
- an unavailable anchor cancels pending/visible presentation;
- pointer-down dismissal remains suppressed until a real eligibility transition instead of immediately reappearing under a stationary pointer;
- teardown cancels pending presentation work.

[`t062_tooltip`](../examples/features/t062_tooltip.cpp) is the focused tooltip example.

## Dialog

[`include/nativeui/dialog.hpp`](../include/nativeui/dialog.hpp) defines the retained dialog contract. A dialog is composed from NativeUI content/actions and presented through the existing overlay/modal stack; it does not introduce a second platform modal-window hierarchy.

The public model distinguishes dialog actions (including default/cancel roles), a shown/busy/invalid/unavailable presentation result, and action-versus-dismissed completion. The dialog panel is a focus scope so keyboard focus remains within the active modal content while it is presented. The owning UI remains responsible for dialog lifetime and retained focus restoration.

[`t063_dialog`](../examples/features/t063_dialog.cpp) is the focused dialog example.

## Choosing static, dynamic and virtualized composition

Use the smallest retained mechanism that matches the structural requirement:

- use normal declarative children when the child structure is fixed;
- use `If`/`Switch` for conditional branch structure;
- use keyed `ForEach` when a moderate collection changes membership/order and retained identity matters;
- use `VirtualList` when a large logical collection should not instantiate every row as live retained content at once.

Do not replace dynamic composition with manual private-tree mutation, and do not use `VirtualList` merely as an alternate state container. The tree, widget and virtualization layers remain instance-owned by the relevant UI.

## Threading, lifetime and instance boundaries

Composition, layout, focus, widget mutation and retained interaction are UI/main-thread work. Worker threads should hand work to the concrete UI/window owner through the supported Dispatcher boundary documented in [`v1-services-testing-and-limits.md`](v1-services-testing-and-limits.md).

For application code and reviews, keep these rules visible:

- retained component/widget state belongs to a concrete UI instance;
- no mutable global, singleton or `thread_local` current UI/focus/capture/overlay owner is part of the normal model;
- dynamic sources and callback targets must not outlive the state/owner they borrow;
- removing/collapsing/disabling retained content must reconcile focus, capture and transient presentation rather than leaving stale interaction targets;
- layout and interaction callbacks must respect retained lifecycle/reentrancy boundaries rather than mutating private tree structures directly;
- normal application code stays on public NativeUI headers and abstractions.

The final `State<T>` / `Binding<T>` borrowing, subscription, recursive-write and callback-exception wording is intentionally omitted until T123/T124 freeze it.

## Public reference map for this chapter

| Public family | Purpose | Focused reference |
| --- | --- | --- |
| `Spec`, Component builders | declarative retained composition | [`include/nativeui/component.hpp`](../include/nativeui/component.hpp) |
| `If`, `Switch<T>`, `ForEach<T>` | dynamic retained structure | [`t058_dynamic_composition`](../examples/features/t058_dynamic_composition.cpp) |
| `ComponentAvailability`, `VisibilityMode` | retained visibility/enabled/read-only state | [`include/nativeui/component_base.hpp`](../include/nativeui/component_base.hpp) |
| layout containers / scroll layout | constraints, measurement and placement | [`include/nativeui/layout.hpp`](../include/nativeui/layout.hpp) |
| standard widgets | retained controls and text/list families | [`include/nativeui/widgets.hpp`](../include/nativeui/widgets.hpp) |
| focus/input/gesture | retained interaction routing | [`t013_bubbling`](../examples/features/t013_bubbling.cpp), [`t014_focus_scopes`](../examples/features/t014_focus_scopes.cpp), [`t016_gestures`](../examples/features/t016_gestures.cpp) |
| overlay | transient/modal retained content | [`include/nativeui/overlay.hpp`](../include/nativeui/overlay.hpp), [`t061_overlay_portal`](../examples/features/t061_overlay_portal.cpp) |
| tooltip | delayed transient help presentation | [`include/nativeui/tooltip.hpp`](../include/nativeui/tooltip.hpp), [`t062_tooltip`](../examples/features/t062_tooltip.cpp) |
| dialog | retained modal dialog/action model | [`include/nativeui/dialog.hpp`](../include/nativeui/dialog.hpp), [`t063_dialog`](../examples/features/t063_dialog.cpp) |
| virtual list | bounded live retained rows for large collections | [`include/nativeui/virtual_list.hpp`](../include/nativeui/virtual_list.hpp), [`t067_virtual_list`](../examples/features/t067_virtual_list.cpp) |

This table is chapter navigation, not the authoritative complete v1 API inventory. T069 owns the final mechanical public-family inventory and ownership/thread/failure matrix.

## Deferred reconciliation before T122 Done

This chapter must be revisited before T122 is complete:

- **T123/T124:** replace the deliberate State/Binding deferral with the final ownership, notification, reentrancy, exception and subscription-lifetime contract;
- **T069:** verify every public family/name against the frozen backend-neutral v1 API inventory and remove any compatibility surface that does not survive the freeze;
- **T070:** link the final reference application/Getting Started journey instead of duplicating its compile-tested snippets;
- **T071:** align final support/release terminology if release policy changes the durable wording.

Until those gates land, this chapter intentionally documents only stable retained composition/layout/widget/interaction behavior and marks changing contracts instead of guessing them.
