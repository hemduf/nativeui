# NativeUI 1.0 composition, layout, widgets and interaction

This chapter documents the stable NativeUI 1.0 retained composition, layout, widget and interaction model that is already implemented. The landed `State<T>` / `Binding<T>` notification, reentrancy and lifetime contract is documented in [`v1-state-and-binding.md`](v1-state-and-binding.md), and current `main` is the working source of truth for the public/package surface after T069 was deprecated as a standalone freeze gate. A canonical copy-pasteable application journey is not currently delivered: T070 is closed as not planned, and T133–T137 remain the unresolved follow-up track in the roadmap.

## Retained composition model

NativeUI builds a retained component tree from declarative specifications. `Spec` is the public composition unit: builders such as layout containers and widgets produce specifications, and `UI` owns the resulting retained runtime tree.

The durable distinction is:

- **static composition** describes children whose structure is fixed after construction;
- **dynamic composition** changes the retained child set at a safe reconciliation checkpoint rather than mutating the tree directly from an arbitrary callback;
- component identity is retained by the tree, not by a process-global widget registry or hidden current-UI singleton.

Normal application code composes public builders and components. It should not include `nativeui/detail/` implementation headers or construct private retained-tree nodes directly.

### Component protocol, callback contexts and ownership

[`include/nativeui/component_base.hpp`](../include/nativeui/component_base.hpp) is the low-level public protocol behind custom retained components. A `Spec` owns a component factory plus child specifications; when the tree materializes that recipe, the resulting `Component` is owned by its retained node. `NodeId` is identity inside one tree/UI lifetime only: storing an ID does not keep a node alive, and an ID is not a cross-UI or cross-thread capability.

Measurement and layout use **logical UI pixels**. `ChildMetrics` is an owned snapshot of minimum/preferred sizes plus dimensionless flex weights; `ChildPlacement` is parent-produced logical geometry. Custom `Component::measure()`, `minimum_size()`, `child_constraints()`, `measure_constrained()` and `layout_children()` receive borrowed inputs for the duration of the call. Implementations must not retain references into those inputs. The retained tree owns resulting node geometry and reuses it for paint, focus, hit testing and invalidation.

The callback contexts deliberately have different lifetime rules:

- `PaintContext`, `InputContext`, `CanvasInputContext`, `FocusContext` and `LifecycleContext` are callback-scoped borrows. Their painter/platform/context references must not escape the callback.
- `MountContext` itself is callback-scoped, but invalidator callables returned by `invalidator()`, `layout_invalidator()`, `focus_invalidator()` and `availability_invalidator()` are copyable deferred handles for later UI-domain state changes. They do not transfer component/tree ownership.
- paint/input/focus geometry is expressed in logical coordinates; framebuffer/device scaling belongs below this layer.
- text measurement, clipboard/drop integration, retained invalidation and lifecycle hooks belong to the UI/main/render domain. They may allocate, call platform services or invoke application code and are not audio/DSP real-time APIs.

Pointer capture through `InputContext` is scoped to the pointer identity/generation associated with the current callback. Terminal pointer callbacks cannot reacquire capture, and stale/re-entrant callbacks cannot replace or release a newer generation. This preserves deterministic capture ownership during nested dispatch without exposing a process-global capture singleton.

`ComponentAvailability::interactive()` means **visible and enabled**; it intentionally does not fold in `read_only`. Read-only is a separate editing/mutation policy for controls that support it. Effective availability is ancestry-resolved and cached by the retained tree.

A minimal custom component keeps callback borrows local and stores only durable invalidation handles:

```cpp
class Meter final : public ui::Component {
public:
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 18.0f}; // logical pixels
    }

    void mount(ui::MountContext& context) override {
        repaint_ = context.invalidator();
    }

    void paint(ui::PaintContext&) const override {
        // Paint here; do not retain the callback-scoped context.
    }

private:
    std::function<void()> repaint_;
};
```

Factories stored in `Spec` own their captures according to ordinary C++ rules. If a factory or callback captures a reference/pointer to application state, NativeUI does not extend that referenced object's lifetime.
### Dynamic composition

[`include/nativeui/dynamic.hpp`](../include/nativeui/dynamic.hpp) provides three UI-thread-confined dynamic composition families:

- `If` conditionally retains one child while a boolean `Binding` is true;
- `Switch<T>` selects the first equality-matching branch, with an optional fallback;
- `ForEach<T>` reconciles `std::vector<T>` by application-provided stable keys.

All three builders convert their declarative children to owned `Spec` state and subscribe only while their retained host is mounted. A `State` notification requests structural work; it does **not** synchronously splice nodes into the tree on the observer callback stack. Actual mount/unmount/reorder work runs at NativeUI's retained safe reconciliation checkpoint and follows normal focus, pointer-capture and lifecycle teardown rules.

#### If and Switch identity

`If` has one stable logical key while its condition is true. Turning the condition off removes that retained subtree; turning it on later reconstructs it from the stored Spec.

`Switch<T>` requires equality comparison for `T`. `when()` branches are tested in declaration order, so if several stored values compare equal the first one wins. Branch retained keys are derived from declaration position, not from `T`; `otherwise()` supplies the fallback and a later `otherwise()` call replaces the previous fallback. Returning to a branch that was previously removed reconstructs its subtree from the reusable stored Spec.

```cpp
ui::State<bool> advanced{false};
ui::State<int> page{1};

auto content = ui::Column{
    ui::If{advanced, ui::Label{"Advanced controls"}},
    ui::Switch<int>{page}
        .when(1, ui::Label{"Main"})
        .when(2, ui::Label{"Modulation"})
        .otherwise(ui::Label{"Unknown page"}),
};
```

#### ForEach keys and callback contract

`ForEach<T>` takes an observable `std::vector<T>`, a key callback and a child callback. Key results may be string/string-view-like, integral, or enum values. NativeUI encodes those values into owned type-tagged string identities, so for example a string key and an integer key are not silently treated as the same domain.

Keys must be **unique within each snapshot**. Reordering items while preserving keys preserves the corresponding retained children. Removing a key tears down that child, and changing an item's key is removal plus insertion. Duplicate-key snapshots are rejected atomically: an already-valid retained structure is left intact rather than partially updated; an initially duplicated snapshot does not publish a partial initial list.

The key callback is not a one-shot callback. NativeUI may evaluate it more than once while preparing one reconciliation, so it should be deterministic and should not use call count as state. The child callback may also run while preparing a changed snapshot for items whose retained nodes are ultimately reused. Both callbacks may allocate or throw, run in the UI/main-thread domain, and can be invoked again when a recoverable reconciliation is retried.

```cpp
struct Item {
    int id;
    std::string label;
    bool operator==(const Item&) const = default;
};

ui::State<std::vector<Item>> items{{{1, "Alpha"}, {2, "Beta"}}};

auto rows = ui::ForEach<Item>{
    items,
    [](const Item& item) { return item.id; },       // stable unique identity
    [](const Item& item) { return ui::Label{item.label}; },
};
```

Dynamic builders inherit the normal `State<T>` / `Binding<T>` notification and lifetime contract documented in [State and binding](v1-state-and-binding.md). They are not thread-safe collection primitives and are not suitable for direct mutation from an audio callback. Worker/audio domains must hand state changes into the UI domain through an explicitly reviewed mechanism.

The maintained [`t058_dynamic_composition`](../examples/features/t058_dynamic_composition.cpp) example exercises conditional composition, branch revisiting, keyed reordering, duplicate-key rejection and recovery.


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

[`combo_popup.hpp`](../include/nativeui/combo_popup.hpp) provides two focusable retained controls presented through NativeUI's in-view overlay stack rather than separate native popup windows. Their providers and callbacks execute synchronously in the UI domain; none of this API is intended for an audio/DSP real-time callback.

### ComboBox<T>

`ComboBox<T>` requires a copy-constructible, equality-comparable value and writes through a `Binding<T>` (or a `State<T>&` convenience constructor). Each `ComboBoxOption<T>` is an owned snapshot row:

- `value` is compared with the current selection and copied into the deferred commit path;
- `label` owns UTF-8 presentation text;
- `enabled == false` keeps the row visible but removes it from keyboard/pointer selection.

The stable-vector constructors take ownership of the supplied vector. NativeUI retains it as the anchor-display snapshot and copies it for each popup open, so later caller mutations of the original vector cannot affect the control.

The dynamic `OptionsProvider` returns a fresh owned `std::vector<ComboBoxOption<T>>`. A non-empty provider is called **synchronously once during builder construction** to seed anchor display state and again **on every popup open**. The returned vector becomes NativeUI-owned snapshot state; no borrow into provider-local storage survives the call. An empty provider/result is valid. Provider exceptions are not translated into fallback rows and propagate through the construction/input path.

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
    .placeholder("Choose a voice")
    .spec();
```

The placeholder is owned UTF-8 text displayed when the selected value is absent from the latest display snapshot. `.style()` owns the anchor `ComboBoxStyle`; `.item_style()` owns the `MenuItemStyle` applied to popup rows. Style dimensions follow NativeUI logical UI units.

Opening, provider evaluation, option copying, text measurement and `spec()` may allocate. Keep them on the UI/resource-preparation side of an application rather than an RT callback.

Selection commit is deliberately deferred: the chosen value is copied out of the popup snapshot, the overlay is closed/detached, and only then is `Binding<T>::set()` performed at the retained safe commit point. Selection observers may therefore perform ordinary state/UI reentrant work without seeing a half-detached popup subtree. If the anchor has been unmounted before the deferred commit runs, the weak anchor check suppresses the write.

### PopupMenu

`PopupMenuItem` owns all row state. `PopupMenuItem::action(label, callback, enabled)` moves the UTF-8 label and callback into the row. An action is actionable only when its kind is `Action`, it is enabled, and its `std::function<void()>` is non-empty. `separator()` creates a disabled structural row with no callback.

A stable-vector `PopupMenu` owns the supplied rows and copies them into a fresh session snapshot on every open. The dynamic `ItemsProvider` differs from ComboBox providers in one important detail: it is **not** called during builder construction; it runs only when the menu opens. Its returned vector is fully owned by the popup session, so provider-local data can be released immediately after return. Empty snapshots are valid, and provider exceptions propagate through the opening input path.

Action callbacks execute only after the popup has closed/detached at the retained safe commit point. The callback may perform ordinary state/UI work, including work that causes retained reconciliation, without racing popup teardown. Callbacks are application/UI callbacks, may allocate, and are not audio-RT work.

```cpp
auto menu = ui::PopupMenu{
    "Actions",
    std::vector<ui::PopupMenuItem>{
        ui::PopupMenuItem::action("Duplicate", [] { duplicate_selection(); }),
        ui::PopupMenuItem::separator(),
        ui::PopupMenuItem::action("Delete", [] { delete_selection(); }, can_delete),
    }}
    .spec();
```

### Interaction and lifetime

Both controls create modal retained overlays anchored to their mounted node. Popup snapshots and callback/value objects are owned by the active session; the anchor runtime is shared internally only for retained coordination. Application code receives no borrowed pointer into the popup implementation.

Down/Enter/Space can open; Up/Down/Home/End navigate eligible rows; Enter/Space commit or invoke; Escape dismisses; and Tab closes before normal focus traversal. Pointer activation captures/release through the normal retained input path. Disabled ComboBox rows, separators, disabled menu actions and menu actions with empty callbacks are skipped.

Opening-key suppression prevents the same key-down that created a popup from immediately re-triggering it before the matching key-up. Dismissal/teardown clears the retained runtime state so stale popup handles are not reused.

Keyboard behavior is demonstrated by [`t035_combo_popup.cpp`](../examples/features/t035_combo_popup.cpp). Typed anchor/row presentation is documented in [Widget style reference](v1-widget-style-reference.md), and the underlying overlay lifetime rules are documented later in this chapter.

## Text input and IME

Text editing uses NativeUI's retained text/editing model while platform integration supplies native text/IME events. The stable conceptual boundary is:

- the retained text model owns text, selection/caret and editing state;
- the focused text component owns the editing interaction;
- platform IME/composition integration feeds the retained model through NativeUI rather than exposing AppKit/Win32/X11 implementation objects to normal application code;
- disabling/hiding/removing the editing target must not leave a stale active text target.

Focused references are [`t025_text_edit_model`](../examples/features/t025_text_edit_model.cpp), [`t028_text_area`](../examples/features/t028_text_area.cpp) and [`t029_ime_composition`](../examples/features/t029_ime_composition.cpp).

## Overlays

[`include/nativeui/overlay.hpp`](../include/nativeui/overlay.hpp) defines the generic retained overlay model and [`UI::show_overlay()` / `close_overlay()`](../include/nativeui/ui.hpp) own publication. Overlays remain retained content inside one `UI`; they are not native popup windows and they do not create process-global modal state.

### Ownership, specification and lifetime

`OverlaySpec` is an owned value request. Successful `show_overlay()` moves the content `Spec` and policy into one UI-owned overlay entry. The overlay therefore owns its retained specification, but it does **not** own the optional anchor node and it cannot extend the lifetime of application objects borrowed by factories or callbacks captured inside that specification.

- `OverlayMode::NonModal` leaves ordinary root keyboard focus active. `Modal` creates an active trapping focus scope and blocks pointer input to lower retained content in the same UI.
- `OverlayPointerPolicy::Normal` participates in retained pointer hit testing. `Ignore` makes the wrapper and descendants pointer-transparent. `Modal + Ignore` is invalid: publication returns an invalid handle and does not leave a live entry.
- `anchor` is an optional non-owning `NodeId` from the **same UI**. Without an anchor, presentation is centered regardless of the requested placement. If a published anchor later disappears or can no longer resolve geometry, the UI dismisses the overlay rather than reusing stale coordinates.
- `dismiss_on_escape` and `dismiss_on_outside_pointer_down` are opt-in policies. Outside-pointer dismissal is evaluated against resolved logical bounds.

`OverlayHandle` is a non-owning identity token for the exact entry in the exact UI. Keeping a handle does not keep an entry alive. It becomes stale after explicit close, policy dismissal, anchor loss or UI teardown. Empty, stale, already-closing/already-closed and cross-UI handles are rejected by `close_overlay()` with `false`. Equality compares identity, not current liveness, so `valid()` remains the liveness observation.

### Placement and units

All overlay geometry is expressed in **logical UI pixels** in the owning viewport. Retained measurement chooses the content's natural size; placement changes the origin but does not shrink that measured size merely to fit the viewport.

For an explicit anchor-relative side, NativeUI uses the requested side when it fully fits. If it does not fit, the opposite side wins when it fully fits; when neither side fully fits, the side with the larger viewport intersection wins, with a tie preserving the requested side. The resulting origin is then clamped to the viewport. Oversized content keeps its measured size, so it may extend past the far edge even though its origin is clamped.

`OverlayPlacement::Auto` evaluates Below, Above, Right and Left in that exact order. The first full fit wins. If no side fully fits, the greatest viewport-intersection area wins and the same order breaks ties. Anchorless overlays use centered placement regardless of the enum value.

`OverlayEntryInfo::placement` reports the **requested policy**, not the side chosen after flip/auto resolution. `bounds` is meaningful only when `resolved == true`; it is an owned point-in-time geometry snapshot and does not track later layout.

### Publication, failure and retained mutation

Publication and close are transactional around retained structural invalidation:

- opening may allocate; allocation failures propagate;
- `Modal + Ignore` is a contract rejection represented by an invalid handle rather than an exception;
- if retained structural notification throws while opening, the provisional entry is removed before the exception propagates;
- if structural notification throws while closing, the logical entry remains live and the same handle remains the retry owner instead of exposing a half-closed state;
- a successful close makes that exact handle stale.

`overlay_entries()` is diagnostics only. It allocates/copies an owned vector in creation order and exposes policy plus resolved geometry; it never returns content components, mutable overlay state or platform objects.

`show_overlay()`, `close_overlay()`, layout-driven placement/dismissal and `overlay_entries()` all belong to the UI/main-thread domain. They may allocate and participate in retained structural work. None of this surface is audio/DSP real-time safe, and a handle is not a cross-thread synchronization or mutation capability.

```cpp
ui::OverlaySpec popup;
popup.anchor = button_node_id;                 // borrowed retained identity
popup.placement = ui::OverlayPlacement::Auto; // logical-pixel placement
popup.dismiss_on_escape = true;
popup.dismiss_on_outside_pointer_down = true;
popup.content = ui::make_spec(ui::Label{"Actions"});

ui::OverlayHandle handle = screen.show_overlay(std::move(popup));
if (handle) {
    // Later, still in the owning UI/main-thread domain.
    for (const auto& entry : screen.overlay_entries()) {
        if (entry.resolved) {
            // entry.bounds is an owned logical-geometry snapshot.
        }
    }
    (void)screen.close_overlay(handle);
}
```

[`t061_overlay_portal`](../examples/features/t061_overlay_portal.cpp) exercises non-modal, modal, pointer-transparent, anchored/dismissal, placement and handle-lifetime behavior.

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

[`include/nativeui/dialog.hpp`](../include/nativeui/dialog.hpp) defines a UI-scoped retained dialog transaction. Presentation stays inside the owning UI's retained overlay/modal stack; NativeUI does not create a second platform modal-window hierarchy or a process-global current-dialog object.

### Specification, ownership and validation

`DialogSpec` is an owned value description consumed by `show()`:

- `title` is owned text; an empty title removes the title row;
- `body` is a retained `Spec` whose component factory must be valid;
- `actions` is an owned vector in presentation order and may be empty;
- `backdrop_color` is copied with the specification and paints the owning UI viewport behind the panel.

Every `DialogActionId` is an owned string. IDs must be non-empty and unique within one specification. A spec may contain at most one `Default` action and at most one `Cancel` action. Disabled actions remain visible and keep their row/order, but cannot be activated. The semantic role does not reorder actions.

`Dialog::show()` reports contract-level outcomes explicitly:

- `Shown`: the per-UI dialog slot and retained modal overlay were published;
- `Busy`: this controller or another dialog already owns the UI's single active dialog slot;
- `InvalidSpec`: the body factory or action invariants are invalid;
- `Unavailable`: the UI is tearing down/unavailable or retained overlay publication cannot complete.

An empty completion callable is valid. Validation occurs before the single per-UI slot is acquired. Allocation and retained structural-notification failures are not encoded as `DialogShowResult` and may throw; failed publication rolls the slot/local ownership back instead of leaving the UI stranded in `Busy`.

### Result and keyboard policy

`DialogResult` is an owned terminal value. `Action` carries the selected action's exact owned ID. `Dismissed` carries an empty ID.

An enabled `Default` action is the fallback for Enter **after focused content has had its normal input opportunity**. For example, a focused single-line text input may submit and a text area may insert a newline without triggering the dialog default action. Escape follows the dialog policy before it can leak to focused dialog content: an enabled `Cancel` action produces an `Action` result for that ID; otherwise Escape produces `Dismissed`. `Dialog::close()` requests the same dismissed terminal result programmatically.

The dialog panel is a trapping focus scope. Pointer/keyboard ownership, focus restoration and dismissal remain scoped to the same retained `UI`; no native focus/modal lifetime is exported to application code.

### Completion, reentrancy and failure recovery

A non-empty `Dialog::Completion` is owned by the active transaction and runs at most once. NativeUI first reaches the retained close safe point, releases the per-UI dialog slot and makes the controller terminal, then invokes application completion. The callback may therefore re-enter NativeUI, present another dialog, destroy the `Dialog`, or destroy the owning `UI`.

When a close is requested from inside retained dispatch, NativeUI may defer the whole close transaction until the retained safe checkpoint. If retained reconciliation fails before that commit point, the same dialog remains repairable. The **first requested terminal result is preserved across retry**: if an action requested completion and close reconciliation threw, a later repair call to `close()` cannot silently replace that pending action result with `Dismissed`.

If application completion throws after a successful close commit, the dialog is already terminal and the per-UI slot is reusable before the exception propagates. `Dialog` destruction is `noexcept`: while the UI is live it performs best-effort dismissed close, forces terminal cleanup if ordinary close fails, and contains completion exceptions.

UI deactivation or whole-UI teardown is different from an application-requested close: NativeUI abandons the presentation and suppresses the application completion callback. The controller does not keep its `UI` alive; if the UI disappears first, `active()` becomes false and later close/show paths are inert/unavailable rather than dereferencing ownership that no longer exists.

All dialog construction, retained publication/reconciliation, callback movement and application completion are UI/main-thread work and may allocate. None of this surface is intended for an audio/DSP real-time callback.

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

The panel uses logical UI geometry and bounded body scrolling rather than a separate native coordinate/window model.

[`t063_dialog`](../examples/features/t063_dialog.cpp) is the executable contract reference for Default/Cancel behavior, focused-editor precedence, exactly-once completion, deactivation suppression, reentrant UI destruction, close-retry preservation, focus trapping, bounded scrolling and close-before-callback semantics.
## Virtualized list controller

[`VirtualListState<Key>`](../include/nativeui/virtual_list.hpp) is the external controller for the fixed-row-height virtualized `ListView` path. It owns the instance-local retained runtime while the application supplies a borrowed `State<std::optional<Key>>` for selection. That selection State must outlive every live controller/view/runtime that still refers to it; there is no process-global current-list registry.

The constructor takes a fixed `row_height` in **logical UI pixels**, a row factory, and an `overscan` count in **rows**. Row height must be finite and greater than zero or construction throws `std::invalid_argument`. The row factory receives `const VirtualListState<Key>::Item&`, may return either a `Spec` or a public builder accepted by `make_spec()`, and is retained for later viewport materialization. It is not invoked merely to accept a large logical dataset.

`Item` owns the logical key plus semantic name, description, enabled/read-only/checked state and semantic actions. Keys use the same string-like/integral/enum encoding domain as dynamic composition and must be unique within an accepted dataset.

### Dataset replacement and stable identity

`replace(std::vector<Item>)` consumes a complete logical dataset. Duplicate keys, unrepresentable total content height, exhausted semantic tokens, or exhausted dataset generation reject the update with `false`; these contract-level rejections keep the previously accepted logical dataset. Allocation failures and application callback exceptions are not encoded as `false` and may propagate.

Replacing with an exactly identical dataset succeeds without changing `dataset_generation()`. A changed accepted dataset advances the generation. Stable keys preserve semantic item tokens across reorder, so assistive/native semantic identity does not depend on a row staying at the same numeric index.

Viewport materialization stays bounded by the visible range plus overscan, with focused/captured exception rows retained when required. Dataset replacement can refresh the current materialization window and therefore may invoke the row factory for newly materialized rows. Dataset/state/scroll mutation is UI/main-thread work and is not an audio-thread API.

```cpp
using ListState = ui::VirtualListState<int>;

ui::State<std::optional<int>> selected{std::nullopt};
ListState presets{
    selected,
    28.0f, // logical pixels
    [](const ListState::Item& item) {
        return ui::Label{item.name};
    },
    2};     // overscan rows

std::vector<ListState::Item> items;
items.emplace_back(1, "Init");
items.emplace_back(2, "Bass");
(void)presets.replace(std::move(items));
```

### Programmatic scrolling and geometry

`scroll_to_index()` and `scroll_to_key()` use the current viewport and `ScrollAlignment`. `Nearest` leaves an already-visible row in place; `Start`, `Center`, and `End` position it accordingly while clamping to representable content bounds. These methods return `false` for a missing/out-of-range target or invalid geometry. A successful scroll may cause viewport materialization and row-factory calls.

`offset()`, `viewport_size()`, `content_size()`, and `row_height()` are all expressed in logical UI pixels. `overscan()` is a row count.

### Immutable semantic snapshots

`metadata_snapshot()` exposes the shared immutable O(N) semantic metadata for the accepted dataset. The method returns a reference to the controller's shared-pointer handle; copy that `shared_ptr` if the snapshot must outlive the controller or a later handle replacement.

`semantic_children(list_bounds)` captures the current dataset generation, selection, immutable metadata and scroll geometry into a `VirtualSemanticChildren` value. `list_bounds` is in logical UI coordinates. Querying off-screen semantic items does **not** create visual rows and does not call application row factories, so accessibility inspection is independent of viewport materialization. The returned value owns a shared metadata reference and is data-only after capture.

The maintained [`t067_virtual_list`](../examples/features/t067_virtual_list.cpp) example demonstrates a 100,000-item dataset, bounded row-factory calls, keyboard selection/activation, programmatic semantic inspection, and stable semantic metadata across scrolling.

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
