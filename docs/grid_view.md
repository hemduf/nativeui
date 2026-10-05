# GridView<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

An item collection virtualized as a grid with adaptive columns, selection, and 2D navigation. NativeUI has [Grid](grid.md), an eager layout, but no such large-collection model.

MyGo `ui/gridview.go`: `GridState`, `GridView`, `gridFit`, `keys`, `reorder`. Columns are calculated from a minimum width and rows have fixed height; resizing retains the item at the viewport top.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API, member signatures of `GridView<Key>`:

```cpp
GridView(Binding<std::vector<CollectionItem<Key>>> items, Selection<Key>& selection);
GridView(State<std::vector<CollectionItem<Key>>>& items, Selection<Key>& selection);
GridView&& minimum_cell_width(double value) &&;
GridView&& cell_height(double value) &&;
GridView&& gap(double value) &&;
GridView&& cell(std::function<Spec(const CollectionItem<Key>&)>) &&;
GridView&& selection_mode(SelectionMode) &&;
GridView&& on_activate(std::function<void(const Key&)>) &&;
GridView&& on_reorder(std::function<void(const std::vector<Key>&,std::optional<Key>)>) &&;
GridView&& style(GridViewStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::CollectionItem<int>>> items{{{1,"Photo 1"},{2,"Photo 2"}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
auto grid = ui::GridView<int>{items,selection}.minimum_cell_width(140.0)
    .cell_height(120.0)
    .cell([](const auto& item){return std::move(ui::Label{item.label}).spec();});
```

Defaults min_width140 DIP, cell_height120 DIP, gap8 DIP, Single, overscan2 rows. Target `GridViewState<Key>` controller in this pair with scroll_to_key/index, visible_range with exclusive end; optional `.state(GridViewState<Key>&)`, safe control token. Item/selection models come from [ListView](list_view.md).

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Copied items Binding, unique keys and monotonic tokens. section_header has no meaning in GridView: reject true in the dataset, use a list for sections. Selection selected ordered by dataset; active/anchor by key. Changing column count does not modify selected or its bindings. Invalid bindings refuse writes/mutating activation/synthetic callbacks.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Left/Right follow linear dataset order (−1/+1): right on the last item of a row moves to the first of the next, left does the reverse; no wrapping between dataset beginning and end. Up/Down jump by column_count and target the same column, clamping on the last row. Home/End go to dataset beginning/end; PageUp/Down jump by the number of visible rows. Multiple selection/toggle modifiers and typeahead follow ListView; Shift extends a linear dataset range. Enter/double-click activate. Opt-in reorder: dragged selected keys in dataset order, insertion before key/nullopt for end, application replaces dataset; Escape/cancel/removal cancel it, bounded local autoscroll. Wheel through ScrollView.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Columns = max(1,floor((viewport_width+gap)/(minimum_cell_width+gap))). Effective width `(max(0,W)-(C−1)*gap)/C` bounded at zero; requested minimum is used to calculate C; with C=1 in a narrow viewport, the cell takes available width even below the requested minimum, without division by zero. Overflowing intrinsic content is clipped. The last row retains the same cell widths, with noninteractive empty spaces. Fixed rows height+gap with double cumulative sums; viewport/overscan materialization and active capture exceptions. Column resize retains the first visible key+inset, never an old row index. Extent overflow rejected before commit; scrolling O(V), metadata copying O(N); key validation/rematching at a new generation O(N log N) for encodable keys and up to O(N²) for equality-only keys, as in [ListView](list_view.md).

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

GridViewStyle: selected/hover/pressed/focus cell, padding, and insertion marker; content (image/title) comes from the factory. Separate active focus and multiple selection. Gap/cell sizes change layout/window; color requires only paint. No synchronous image loading during layout/paint; resources are application-provided and per instance.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Use ListView/ListItem or Custom for provisional semantics; GridRole/row/column relationships require a future neutral extension. Full dataset data-only shared metadata, with item bounds calculated from the current columns/row-size snapshot. No cell factory on semantic reads. Reflow publishes consistent geometry, rather than indices of the old grid.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Resize/dataset replacement prepares a new window and stable mapping before commit. A throwing cell factory does not publish an incomplete row; durable dirty recovery. Removing a dragged key ends capture/autoscroll. Reentrant on_reorder may rebuild the dataset; continue only with an owned ID snapshot. An expired controller makes operations safe/no-op.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[ListView](list_view.md) selection/tokens, [ScrollView](scroll_view.md), common models. Minimum width/cell height strictly positive and finite; finite nonnegative gap, invalid_argument for invalid options. Empty/duplicate items, very small width, a million items, partial last row, and focusable child controls. Key PageUp/PageDown = future additive input-system extension, requiring platform translations.

`Key::PageUp` and `Key::PageDown` are target additions at the end of the current portable enum, with platform translation and tests. The current source does not define them. Typeahead uses committed-text InputEvent objects; no complete native IME support is assumed.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/grid_view.hpp` and `src/grid_view.cpp`.

grid_view.hpp contains style/state/key/factory templates; grid_view.cpp contains column fitting, 2D window, selection navigation, drag transactions, and paint/layout. Do not repurpose GridComponent as a collection or duplicate its span auto-placement. Virtualization bodies and tokens are reused from common cores.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `grid_linear_row_boundary`: left/right cross rows, remain bounded at the ends, and preserve linear Shift extension.

- `grid_view_fit_columns`: min width+gap thresholds and zero width.
- `grid_view_last_row`: same widths, inert blank cells.
- `grid_view_resize_anchor`: stable item/inset as columns change.
- `grid_view_navigation_2d`: arrows/Home/End/Page and disabled skipping.
- `grid_view_range_selection`: Shift linear range with stable anchor.
- `grid_view_reorder_keys_cancel`: one-shot callback and removal during drag.
- `grid_view_million_items`: Components bounded by visible rows.
- `grid_view_geometry_overflow_fault`: reject without a new partial generation.
- `grid_view_semantic_reflow`: stable metadata pointer, data-only geometry change.

Create the future public example `examples/features/grid_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
