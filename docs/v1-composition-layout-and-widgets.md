# NativeUI 1.0 composition, layout, widgets and interaction

This chapter documents the stable NativeUI 1.0 retained composition, layout, widget and interaction model that is already implemented. The landed `State<T>` / `Binding<T>` notification, reentrancy and lifetime contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md), and current `main` is the working source of truth for the public/package surface after T069 was deprecated as a standalone freeze gate. A canonical copy-pasteable application journey is not currently delivered: T070 is closed as not planned, and T133–T137 remain the unresolved follow-up track in the roadmap.

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

These helpers observe `State<T>` to request structural reconciliation. The nested-observer, recursive-write, equality and subscription-destruction semantics are now part of the landed T123/T124 contract; this chapter relies on that contract and leaves the detailed rules to [`v1-state-and-binding.md`](v1-state-and-binding.md).

## Availability and read-only state

Every retained component has effective availability derived from its local state and retained ancestors. The public `ComponentAvailability` model distinguishes:

- `VisibilityMode::Visible` — participates normally;
- `VisibilityMode::Hidden` — remains in layout but is not presented/interacted with;
- `VisibilityMode::Collapsed` — does not participate as normal visible layout content;
- `enabled == false` — the component is non-interactive;
- `read_only == true` — editing/mutation interactions are suppressed while the component can remain visible and otherwise present.

Availability is a retained-tree concern rather than a platform-widget flag. Focus, pointer targeting, overlays and widget behavior consume the effective retained availability so an unavailable descendant cannot continue acting as an interactive target merely because a native event arrives later.

The State/Binding notification ordering that can drive availability is defined by the landed T123/T124 contract. The stable 1.0 rule documented here focuses on effective component behavior; detailed observer semantics remain centralized in [`v1-state-and-binding.md`](v1-state-and-binding.md).

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

These focused programs also expose the repository `--self-test` convention. Because the production reference application / Getting Started path is currently being replanned after T070 closed as not planned, this chapter links to shipped focused examples instead of inventing a second tutorial source.

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

The complete public event/focus/command contract is documented in [Input, focus and commands](v1-input-focus-and-commands.md), including [`input.hpp`](../include/nativeui/input.hpp), [`focus.hpp`](../include/nativeui/focus.hpp), [`command.hpp`](../include/nativeui/command.hpp) and the borrowed callback contexts in [`component_base.hpp`](../include/nativeui/component_base.hpp).

NativeUI routes input through the retained tree in logical coordinates. Interaction state is owned by the relevant `UI`/retained tree; there is no process-wide focus target, pointer-capture target or gesture registry shared by unrelated UI instances.

### Bubbling and keyboard handling

Pointer and keyboard events are resolved against retained nodes and may bubble through retained ancestors according to the public input model. Components can handle relevant events without installing platform event hooks. [`t013_bubbling`](../examples/features/t013_bubbling.cpp) is the focused routing example.

Keyboard activation and editing behavior belongs to the focused widget/component that owns the semantic action. NativeUI does not require application code to dispatch raw Cocoa/Win32/X11 messages to widgets.

### Focus and focus scopes

Focus is retained per UI. Focus scopes establish local traversal/containment boundaries and are used by higher-level interaction such as modal dialogs. Losing availability can make a focused node ineligible and trigger retained focus reconciliation; this is not a native-widget ownership transfer.

[`t014_focus_scopes`](../examples/features/t014_focus_scopes.cpp) is the focused example for scope/traversal behavior.

The landed T123/T124 work defines the interaction between reentrant state notification and availability-driven focus restoration. This chapter documents the resulting component behavior and delegates the observer-ordering details to [`v1-state-and-binding.md`](v1-state-and-binding.md).

### Pointer capture and gestures

Pointer capture is retained per UI and ties an active pointer sequence to the owning component without creating a process-global capture registry. Capture lifetime follows the retained target: teardown or loss of eligibility must not leave a stale target.

[`t015_pointer_capture`](../examples/features/t015_pointer_capture.cpp) exercises capture behavior. [`t016_gestures`](../examples/features/t016_gestures.cpp) covers the gesture layer built on the same retained input stream.

Drag/drop integration is demonstrated by [`t018_drop`](../examples/features/t018_drop.cpp). Application code should consume the public NativeUI drop/input abstractions rather than reaching into platform event objects.

## ComboBox and PopupMenu

[`combo_popup.hpp`](../include/nativeui/combo_popup.hpp) provides two focusable retained controls presented through NativeUI's in-view overlay stack rather than separate native popup windows.

### ComboBox<T>

`ComboBox<T>` requires a copy-constructible, equality-comparable value and writes through a `Binding<T>` (or a `State<T>&` convenience constructor). Each `ComboBoxOption<T>` owns a value, display label and enabled flag. Disabled options remain visible but are skipped by navigation and cannot be committed.

Options may be an owned vector or an `OptionsProvider`. Provider-backed ComboBoxes call the provider once during builder construction to seed anchor display state and again **on every popup open**. The popup owns the returned vector snapshot.

```cpp
ui::State<int> voice{1};

auto combo = ui::ComboBox<int>{
    voice,
    [] {
        return std::vector<ui::ComboBoxOption<int>>{
            {1, "Mono", true},
            {2, "Poly", true},
            {3, "Unavailable", false},
        };
    }}
    .placeholder("Choose a voice");
```

The placeholder appears when the selected value is absent from the current display snapshot. `.style()` customizes the anchor; `.item_style()` customizes popup rows.

Selection is committed only after popup close/reconciliation reaches its safe retained checkpoint, so selection observers cannot run against a popup subtree still being detached.

### PopupMenu

`PopupMenuItem::action(label, callback, enabled)` builds an action row; `separator()` builds a disabled structural row. `actionable()` is true only for enabled action rows with a callback.

`PopupMenu` accepts a stable vector or an `ItemsProvider`; providers are evaluated on each open and the session owns that snapshot. Action callbacks run after popup close/detach reaches its retained commit point.

Keyboard behavior is demonstrated by [`t035_combo_popup.cpp`](../examples/features/t035_combo_popup.cpp): Down/Enter/Space can open, Up/Down/Home/End navigate eligible rows, Enter/Space commit, Escape dismisses, and Tab closes before normal focus traversal.

Typed anchor/row presentation is documented in [Widget style reference](v1-widget-style-reference.md).

## Text input and IME

Text editing uses NativeUI's retained text/editing model while platform integration supplies native text/IME events. The stable conceptual boundary is:

- the retained text model owns text, selection/caret and editing state;
- the focused text component owns the editing interaction;
- platform IME/composition integration feeds the retained model through NativeUI rather than exposing AppKit/Win32/X11 implementation objects to normal application code;
- disabling/hiding/removing the editing target must not leave a stale active text target.

Focused references are [`t025_text_edit_model`](../examples/features/t025_text_edit_model.cpp), [`t028_text_area`](../examples/features/t028_text_area.cpp) and [`t029_ime_composition`](../examples/features/t029_ime_composition.cpp).

## Overlays

[`include/nativeui/overlay.hpp`](../include/nativeui/overlay.hpp) defines the generic retained overlay model and [`UI::show_overlay()` / `close_overlay()`](../include/nativeui/ui.hpp) own publication. Overlays remain retained content inside one `UI`; they are not native popup windows and they do not create process-global modal state.

### Overlay specification and lifetime

`OverlaySpec` owns the overlay content specification plus its presentation policy:

- `OverlayMode::NonModal` leaves ordinary root keyboard focus active. `Modal` creates an active trapping focus scope and blocks pointer input to lower retained content.
- `OverlayPointerPolicy::Normal` participates in pointer hit testing. `Ignore` makes the overlay and its descendants pointer-transparent. `Modal + Ignore` is invalid and `show_overlay()` returns an invalid handle for that combination.
- `anchor` is an optional `NodeId` from the **same UI**. Without an anchor, presentation is centered. If an anchored node disappears or becomes unresolvable, the UI dismisses that overlay rather than keeping stale anchor state.
- `placement` is resolved in logical UI coordinates. Anchor-relative sides may flip to their opposite side when that fits better and are then clamped to the viewport. `Auto` considers below, above, right and left deterministically in that order.
- Escape and outside-pointer dismissal are opt-in through `dismiss_on_escape` and `dismiss_on_outside_pointer_down`.

`OverlayHandle` is a non-owning per-UI identity token. It becomes stale after explicit close, policy dismissal, anchor loss or UI teardown; keeping the handle does not keep the overlay alive. Passing an empty, stale, already-closed or cross-UI handle to `close_overlay()` returns `false`.

Publication/close are transactional around retained structural invalidation. If structural notification throws while opening, the provisional entry is rolled back before the exception escapes. If it throws while closing, the logical entry remains live so the same handle can be retried instead of exposing a half-closed overlay.

`overlay_entries()` is diagnostics only: it returns creation-order policy plus resolved bounds in **logical coordinates**. `resolved == false` means retained layout has not produced final bounds yet.

```cpp
ui::OverlaySpec popup;
popup.anchor = button_node_id;
popup.placement = ui::OverlayPlacement::Auto;
popup.dismiss_on_escape = true;
popup.dismiss_on_outside_pointer_down = true;
popup.content = ui::make_spec(ui::Label{"Actions"});

ui::OverlayHandle handle = screen.show_overlay(std::move(popup));
if (handle) {
    // Later, still on the UI thread.
    (void)screen.close_overlay(handle);
}
```

Overlay presentation, dismissal and inspection are UI/main-thread operations. A handle is not a synchronization primitive and does not permit cross-thread retained mutation.

[`t061_overlay_portal`](../examples/features/t061_overlay_portal.cpp) exercises non-modal, modal, pointer-transparent, anchored/dismissal and handle-lifetime behavior.

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

[`include/nativeui/dialog.hpp`](../include/nativeui/dialog.hpp) defines a UI-scoped retained dialog controller. A dialog is ordinary NativeUI retained content presented through the overlay/modal stack; it does not introduce a second platform modal-window hierarchy.

### Specification and validation

`DialogSpec::body` must contain a valid component factory. Action IDs must be non-empty and unique, and a spec may contain at most one `Default` action and one `Cancel` action. Disabled actions remain visible but cannot activate.

`Dialog::show()` reports contract-level failures without inventing platform exceptions:

- `Shown`: the per-UI dialog slot and modal overlay were published;
- `Busy`: this controller or another dialog already owns the UI's single active dialog slot;
- `InvalidSpec`: body/action invariants are invalid;
- `Unavailable`: the UI is tearing down/unavailable or the backing overlay cannot publish.

Allocation or retained structural-notification failures can still throw. A failed publication rolls back ownership so the UI is not stranded in `Busy`.

### Completion, keyboard policy and lifetime

An enabled `Default` action handles Enter. Escape completes the enabled `Cancel` action when present; otherwise it produces `DialogResultKind::Dismissed`. `Dialog::close()` is the programmatic dismissed path.

Completion runs only after the retained overlay close/reconciliation reaches its safe commit point and the per-UI dialog slot is released. Application completion code can therefore re-enter and present another dialog. If a normal completion callback throws, NativeUI has already made the completed dialog terminal before the exception propagates. Destructor cleanup is `noexcept` and contains failures.

`Dialog` is bound to one `UI`, is non-copyable/non-movable and is UI/main-thread confined. `active()` observes whether that controller still owns the active generation; it does not extend UI lifetime. UI deactivation/teardown may abandon active presentation as part of UI lifecycle cleanup rather than preserving a native modal session outside the UI.

```cpp
ui::Dialog confirm{screen};

ui::DialogSpec spec;
spec.title = "Delete preset?";
spec.body = ui::make_spec(ui::Label{"This cannot be undone."});
spec.actions = {
    {"delete", "Delete", true, ui::DialogActionRole::Default},
    {"cancel", "Cancel", true, ui::DialogActionRole::Cancel},
};

const auto shown = confirm.show(
    std::move(spec),
    [](ui::DialogResult result) {
        if (result.kind == ui::DialogResultKind::Action) {
            // result.action_id is "delete" or "cancel".
        }
    });
```

The dialog panel is a focus scope; modal pointer/keyboard ownership and focus restoration remain part of the same retained `UI`. The implementation uses logical viewport geometry and bounded body scrolling, not a separate native coordinate/window model.

[`t063_dialog`](../examples/features/t063_dialog.cpp) is the focused executable reference for Default/Cancel behavior, exactly-once completion, focus trapping, bounded scrolling and close-before-callback semantics.

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

The final `State<T>` / `Binding<T>` borrowing, subscription, recursive-write and callback-exception contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md); this chapter does not duplicate it.

## Public reference map for this chapter

| Public family | Purpose | Focused reference |
| --- | --- | --- |
| `Spec`, Component builders | declarative retained composition | [`include/nativeui/component.hpp`](../include/nativeui/component.hpp) |
| `If`, `Switch<T>`, `ForEach<T>` | dynamic retained structure | [`t058_dynamic_composition`](../examples/features/t058_dynamic_composition.cpp) |
| `ComponentAvailability`, `VisibilityMode`, `Visibility`, `Enabled`, `ReadOnly` | retained visibility/enabled/read-only state | [`component_base.hpp`](../include/nativeui/component_base.hpp), [`component_state.hpp`](../include/nativeui/component_state.hpp), [State and binding](v1-state-and-binding.md) |
| layout containers / scroll layout | constraints, measurement and placement | [`include/nativeui/layout.hpp`](../include/nativeui/layout.hpp) |
| standard widgets | retained controls and text/list families | [`include/nativeui/widgets.hpp`](../include/nativeui/widgets.hpp) |
| focus/input/commands/gesture | retained interaction routing | [Input, focus and commands](v1-input-focus-and-commands.md), [`t016_gestures`](../examples/features/t016_gestures.cpp) |
| overlay | transient/modal retained content | [`include/nativeui/overlay.hpp`](../include/nativeui/overlay.hpp), [`t061_overlay_portal`](../examples/features/t061_overlay_portal.cpp) |
| tooltip | delayed transient help presentation | [`include/nativeui/tooltip.hpp`](../include/nativeui/tooltip.hpp), [`t062_tooltip`](../examples/features/t062_tooltip.cpp) |
| dialog | retained modal dialog/action model | [`include/nativeui/dialog.hpp`](../include/nativeui/dialog.hpp), [`t063_dialog`](../examples/features/t063_dialog.cpp) |
| virtual list | bounded live retained rows for large collections | [`include/nativeui/virtual_list.hpp`](../include/nativeui/virtual_list.hpp), [`t067_virtual_list`](../examples/features/t067_virtual_list.cpp) |
| semantic/accessibility data | roles, actions, stable IDs and immutable projections | [Semantic and accessibility data model](v1-semantics-and-accessibility.md), [`semantics.hpp`](../include/nativeui/semantics.hpp) |

This table is chapter navigation, not the authoritative complete v1 API inventory. The public headers/package surface on current `main`, together with the repository review and validation policies, are the working source of truth for that inventory.

## Remaining reconciliation before T122 Done

The landed T123/T124 State/Binding contract and the current public/package surface on `main` are already the source of truth for this chapter; T069 is not an active gate.

Remaining closeout work is release-facing rather than an implementation dependency:

- keep public family/name coverage aligned with `main` as the toolkit evolves;
- link the eventual reference application / Getting Started artifacts only after the post-T070 replan produces them; T133–T137 remain unresolved;
- align final support/release terminology with the replacement release-qualification plan created after T071 closed as not planned.

Until those replacement artifacts exist, this chapter documents the shipped retained composition/layout/widget/interaction surface and points readers to maintained feature examples.
