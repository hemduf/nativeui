# OutlineTableView<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Virtualized hierarchy with column cells: disclosure in the main column, sort/resize/reorder in the header. NativeUI has no such component. It combines the [OutlineView](outline_view.md) and [TableView](table_view.md) contracts.

MyGo `ui/outline.go`: `OutlineTable`, `prefix`, `keys`, and `ui/table.go`: `table`, `tableHeader`. The first declared column carries disclosure. The target provides an explicit ID so moving columns does not arbitrarily move the hierarchy.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
template<class Key>
OutlineTableView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
                 Binding<std::vector<Key>> expanded);
template<class Key>
OutlineTableView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
                 State<std::vector<Key>>& expanded);
OutlineTableView&& columns(std::vector<TableColumn>) &&;
OutlineTableView&& tree_column(ColumnId id) &&;
OutlineTableView&& cell(std::function<Spec(const TreeNode<Key>&,const ColumnId&)>) &&;
OutlineTableView&& layout(Binding<TableLayout>) &&;
OutlineTableView&& sort(Binding<std::optional<SortOrder>>) &&;
OutlineTableView&& row_heights(ListRowHeights) &&;
OutlineTableView&& selection_mode(SelectionMode) &&;
OutlineTableView&& on_sort_request(std::function<void(const SortOrder&)>) &&;
OutlineTableView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
OutlineTableView&& on_activate(std::function<void(const Key&)>) &&;
OutlineTableView&& autofit_width(std::function<double(const ColumnId&)>) &&;
OutlineTableView&& style(OutlineTableViewStyle) &&;
Spec spec() &&;
```

State overloads of layout/sort retained in this target. Future example:

```cpp
ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1,std::nullopt,"Folder",true,true}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
ui::State<std::vector<int>> expanded{{}};
auto outline = ui::OutlineTableView<int>{nodes,selection,expanded}
    .columns({ui::TableColumn{.id="name",.title="Name"}}).tree_column("name")
    .cell([](const auto& node,const auto&){return std::move(ui::Label{node.label}).spec();});
```

Default tree column = first declared if nonempty; IDs/types come from TableView, TreeNode from TreeView, and Selection from ListView, never duplicated.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Graph, expansion, selection, and column layout are four separate domains. Row keys follow nodes; columns follow IDs. A sort request does not globally sort and break parents: the application returns a snapshot sorting siblings while retaining parents. No internal file persistence. An invalid domain Binding forbids its mutations without a fictitious callback.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

TableView header gestures: resize/reorder/click sort/autofit are distinct. OutlineView body gestures: disclosure click, Right/Left navigation, row activation. Disclosure retains the same tree_column when its header moves. Left/Right keys navigate the hierarchy when focus is on the row; a cell editor consuming its keys retains priority. PointerCancel/Escape cancel column drag without closing nodes. X wheel synchronized across header/body, Y only in body.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Flatten visible tree metadata before the virtual window. The tree_column cell reserves depth*indent + chevron within its width; others retain normal padding. Row height = maximum cell metrics; width changes remeasure variable heights while retaining key/inset. Fixed header Y and shared X. A narrow tree_column clips content with a bounded chevron hit target without moving other columns. Zero columns means no cell factory, preserved graph state but empty body presentation.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

OutlineTableViewStyle reuses row/header/disclosure types from both parents, with owned composition and explicit local priority; do not copy imaginary Theme slots. Resize/reorder guide and chevron painted by the .cpp. Selected/expanded/sort marker and focus are distinct; invalidations classified by domain.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

TreeTable/Cell/ColumnHeader are absent from current roles; provisional Group/Custom, with a future neutral extension for hierarchical relationships and columns. Immutable logical metadata and geometry generation; no cell/row factory from a reader. Expansion, selection, and columns published in one consistent generation, without mixing old order with new widths.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Dataset and layout changes during input prepare a new consistent view at the checkpoint. Removed ColumnId/tree_column: if the new configuration no longer contains it, default to the first column and diagnose; an initially invalid explicit tree_column rejects configuration. Removed captured node/focus recovered through Outline. A reentrant sort/expansion callback may modify the graph; the stack finishes without a stale cell reference. Fit provider failure retains previous widths.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on OutlineView/TableView/ListView cores and common models. Validate graph and column IDs; cycles/duplicates/orphans rejected under Outline rules. Finite min/max, initially absent tree_column throws invalid_argument. Empty graph/columns, great depth, zero width, fixed or moved column, and invalid external sort are covered. No Router or lazy I/O.

`Key::PageUp` and `Key::PageDown` are target additions at the end of the current portable enum, with platform translation and tests. The current source does not define them. Typeahead uses committed-text InputEvent objects; no complete native IME support is assumed.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/outline_table_view.hpp` and `src/outline_table_view.cpp`.

outline_table_view.hpp exposes its builder/style and typed adapters; outline_table_view.cpp is the orchestration core for visible graph/columns/gestures/layout/paint. Share non-template algorithms from both components without maintaining competing height or scroll caches. No additional file per cell, disclosure, or header.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `outline_table_tree_column_id`: reorder retains disclosure in its ID.
- `outline_table_sibling_sort`: sort callback, parents retained by the application snapshot.
- `outline_table_header_body_sync`: exact horizontal movement and fixed vertical header.
- `outline_table_indent_autofit`: includes indent/chevron in column width.
- `outline_table_resize_anchor`: variable rows with stable anchor.
- `outline_table_editor_keys`: respect Left/Right consumed by cell editing.
- `outline_table_remove_column_node`: safe capture/focus, explicit fallback.
- `outline_table_combined_fault`: no mixed generation on graph/layout failure.
- `outline_table_semantics_no_factory`: snapshots without cell callback.

Create the future public example `examples/features/outline_table_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
