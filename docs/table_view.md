# TableView<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

A table of virtualized rows and resizable/reorderable columns, with sort requests. No generic TableView in NativeUI; [ListView](list_view.md) and [ScrollView](scroll_view.md) are foundations.

MyGo `ui/table.go`: `TableColumn`, `SortOrder`, `TableLayout`, `Table`, `tableHeader`, `tableFit`. Autofit measures materialized header/cells; the application sorts rows on request. The target makes layout persistable through stable IDs and does not sort opaque business objects.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
using ColumnId=std::string;
enum class SortDirection { Ascending, Descending };
struct SortOrder {
    ColumnId column;
    SortDirection direction=SortDirection::Ascending;
    bool operator==(const SortOrder&) const=default;
};
struct TableColumn {
    ColumnId id;
    std::string title;
    double width=0.0;
    double minimum_width=60.0;
    std::optional<double> maximum_width;
    Align align=Align::Start;
    bool sortable=false;
    bool fixed=false;
    bool operator==(const TableColumn&) const=default;
};
struct TableLayout {
    std::vector<ColumnId> order;
    std::map<ColumnId,double> widths;
    bool operator==(const TableLayout&) const=default;
};
template<class Key>
TableView(Binding<std::vector<CollectionItem<Key>>> rows, Selection<Key>& selection);
template<class Key>
TableView(State<std::vector<CollectionItem<Key>>>& rows, Selection<Key>& selection);
TableView&& columns(std::vector<TableColumn>) &&;
TableView&& cell(std::function<Spec(const CollectionItem<Key>&,const ColumnId&)>) &&;
TableView&& layout(Binding<TableLayout>) &&;
TableView&& sort(Binding<std::optional<SortOrder>>) &&;
TableView&& row_heights(ListRowHeights) &&;
TableView&& selection_mode(SelectionMode) &&;
TableView&& autofit_width(std::function<double(const ColumnId&)>) &&;
TableView&& on_sort_request(std::function<void(const SortOrder&)>) &&;
TableView&& on_layout_change(std::function<void(const TableLayout&)>) &&;
TableView&& on_activate(std::function<void(const Key&)>) &&;
TableView&& style(TableViewStyle) &&;
Spec spec() &&;
```

State overloads of layout/sort delegate to Binding. Without layout/sort bindings, per-instance internal layout; sort headers are not activatable without a sort binding or on_sort_request. Future example:

```cpp
ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1,"Ada"},{2,"Lin"}}};
ui::State<ui::SelectionSnapshot<int>> selected{{}};
ui::Selection<int> selection{selected};
auto table = ui::TableView<int>{rows,selection}
    .columns({ui::TableColumn{.id="name",.title="Name",.sortable=true}})
    .cell([](const auto& row,const auto&){return std::move(ui::Label{row.label}).spec();});
```

TableColumn/TableLayout/SortOrder are shared in this pair and reused by OutlineTableView; Selection/CollectionItem/ListRowHeights are defined in [ListView](list_view.md). Defaults: Single selection, variable heights estimated at 24 DIP, overscan two.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

- Copied rows Binding, one consistent snapshot per generation; unique keys.
- Layout order names ColumnId values; unknown IDs ignored, new ones follow known ones in declaration order.
- Fixed columns retain their declared position and width, without user resize/reorder.
- User width map applies only to nonfixed columns, bounded by min/max; width=0 distributes remaining available width.
- selected/active/anchor follow row keys, not indices; an old selection snapshot remains readable if the binding is invalid, but writes are forbidden.
- Sort click: initially Ascending, then reverse; set effective sort + request callback, dataset explicitly sorted by the application.
- External layout/sort/row updates do not trigger their gesture callbacks.
- No automatic disk serialization: the application saves its TableLayout.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

The body follows ListView selection/navigation/typeahead/activation; a child cell consuming the event takes priority. Header: a 6 DIP edge arms resize, 4 DIP threshold before movement dragging; these operations do not trigger sort. Resize Up publishes final layout; Escape/cancel returns to the initial snapshot if there was no external update. Double-click on the edge autofits: maximum of currently measured header+cell widths; optional autofit_width supplies an application measurement over all rows without constructing the entire dataset. Header sorts on click release and focused Enter/Space. Horizontal wheel synchronizes header/body; vertical does not move the header. Fixed columns can still sort if sortable.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Fixed header Y, X translation identical to body content, common clip. Row height = maximum cells+padding and minimum header style. A section-header row spans columns and receives the cell factory with only the first ColumnId. Effective widths are shared precisely in logical pixels between header/cells. A deficit creates horizontal scrolling rather than shrinking below minima. Width change invalidates height cache/row wrapping and retains anchor key/inset. No full materialization for default autofit; prepare order and widths before publication.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

TableViewStyle: header/body typography, padding, separator, sort marker, row style, insertion marker, and focus rings. Active column and row selection are distinct. Resize shows a guide; reorder shows an insertion boundary. Paint/layout classification: widths/order require layout; sort marker requires paint unless label metrics change. Style comes from resolved Theme, with explicit new slots.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Table/Row/Cell/ColumnHeader do not exist in current SemanticRole. Target Group/Custom with labels/ranges/allowed actions pending a separate neutral extension. Full logical rows and data-only virtual item snapshots are needed, derived from the ListView extension; no cell factory calls from a bridge. Columns retain stable IDs and name/order in the snapshot, with consistent selection/value per generation.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Resize/reorder state contains ColumnId+generation, not a header index or pointer. A removed column/external layout changed during drag: cancel capture without rolling back the external value. A throwing autofit callback retains previous widths and disarms interaction. A throwing sort/layout callback is not retried; dirty committed layout remains durable. Remount receives current bindings/layout and recreates local subscriptions.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[ListView](list_view.md) variable heights, [ScrollView](scroll_view.md), shared Selection and metadata, and this pair's TableColumn types. Duplicate/empty ID, invalid width/min/max/nonfinite: invalid_argument during configuration/validation before commit. Malformed external layout: ignore unknowns, retain bounded valid values and diagnostics; no automatic write. No columns = table with no cells, no factory; empty rows retain headers and zero offset.

`Key::PageUp` and `Key::PageDown` are target additions at the end of the current portable enum, with platform translation and tests. The current source does not define them. Typeahead uses committed-text InputEvent objects; no complete native IME support is assumed.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/table_view.hpp` and `src/table_view.cpp`.

table_view.hpp declares column models/style/public builder; Key templates adapt metadata/factory to the non-template table_view.cpp core. The .cpp contains headers, column transactions, synchronized scrolling, measure/layout/paint/input, rather than a generic widget switch. Column types are reused through includes and never duplicated in OutlineTableView.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `table_column_ids_layout`: restored order/widths, unknown/new/fixed columns.
- `table_resize_bounds_cancel`: min/max, callback counts, cancel without external rollback.
- `table_header_gesture_priority`: resize/reorder do not trigger sort.
- `table_autofit_materialized`: header+visible cells measured, no factory outside viewport.
- `table_autofit_provider_fault`: a throwing provider retains old layout.
- `table_sort_request`: asc/desc, single callback, application reorders stable keys.
- `table_header_body_scroll`: same X/bounds, header retains fixed Y.
- `table_variable_row_resize`: wrapping changes anchor without a jump.
- `table_column_removed_drag`: stale ID/capture recovered.
- `table_empty_invalid_models`: zero columns/rows and atomic validation.
- `table_virtual_semantics`: snapshots without cell callbacks.

Create the future public example `examples/features/table_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
