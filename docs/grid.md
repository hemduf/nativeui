# Grid

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Grid` exists with Fixed/Auto/Flex tracks and sequential placement row by row. Sources: [layout_model.inc](../include/nativeui/detail/layout_model.inc), `Track`, `GridTracks`; [layout_components.inc](../include/nativeui/detail/layout_components.inc), `GridComponent`; [layout_builders.inc](../include/nativeui/detail/layout_builders.inc).

MyGo `ui/grid.go`: `Grid`, `ColumnStart`, `RowStart`, `ColumnSpan`, `RowSpan`, `gridPlace`, `sizeTracks`. The gaps to address are explicit placement and merged cells; layout remains separate from [GridView](grid_view.md).

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API to preserve:

```cpp
template<class... Children> Grid(GridTracks tracks, Children&&... children);
Grid&& gap(float value) &&;
Grid&& column_gap(float value) &&;
Grid&& row_gap(float value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
auto grid = ui::Grid{
    ui::GridTracks{.columns={ui::Track::fixed(100.0f), ui::Track::flex(1.0f)},
                   .rows={ui::Track::auto_size()}},
    ui::Label{"Name"}, ui::Label{"Value"}}.gap(8.0f);
```

Target extension: `GridCell{std::size_t row, column, row_span=1, column_span=1}` and `template<class Child> Grid&& cell(GridCell, Child&&) &&`. Indices are zero-based; constructor content continues to use auto-placement. `Grid(GridTracks)` accepts zero children followed by `.cell`.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Tracks, placements and Spec objects are owned. No selection binding or callback. A cell never directly references a Tree node. Validate placements before consumption; Spec order remains the logical order even when explicit positions differ.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus/capture of its own. Tab follows composition order; the pointer uses child bounds. The layout does not provide cell selection or arrow navigation, which belong to TableView/GridView. Gestures and confirmation remain in the child controls.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

- Fixed retains the sanitized extent; Auto follows intrinsic size; Flex distributes free space.
- An empty column list becomes a single Auto track; append Auto tracks for required rows.
- Claim explicit cells before auto-placement; auto-placement proceeds in row-major order through the first free cells, without retroactive densification.
- A zero span, size_t overflow, explicit overlap or unrepresentable extent causes `invalid_argument` before publication.
- Distribute a span’s intrinsic contribution across Auto/Flex tracks after subtracting gaps, without changing Fixed tracks.
- Deficit: reduce sizes down to their minima, then allow explicit overflow without automatic clipping.
- Bounded measurement passes and a single publication; no nondeterministic convergence loop.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting. Track/span changes request layout; child color changes request paint. Descendant styles affect tracks through their metrics. Do not add table lines or selection to Grid.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`; children follow composition order. The 2D position does not create Table semantics. Spans create no phantom accessible descendant or duplicate announcement.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare occupancy, tracks and allocations before publishing placements. A failure preserves the last coherent generation. A throwing measurement must not leave a track permanently marked “measured”; a subsequent pass can recover.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Uses existing track/constraint allocation. Test empty content, insufficient declared rows, spans beyond the initial tracks, large indices, zero weights and a reduced window size. The number of children differs from the number of occupied cells.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/grid.hpp` and `src/grid.cpp`.

Preserve `GridComponent`, `Track`, `TrackType` and `GridTracks` through single declarations and appropriate re-exports; do not duplicate models across headers. `grid.cpp` contains placement and span sizing. `layout.hpp`, historical signatures/float gaps and auto-placement without spans remain compatible.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `grid_legacy_tracks`: preserve the `grid_layout_tests` scenarios.
- `grid_span_intrinsic`: spans across Fixed/Auto/Flex with minima.
- `grid_auto_occupied`: skip explicitly occupied cells.
- `grid_overlap_overflow`: reject validation without a partial commit.
- `grid_empty_resize`: empty/zero bounds followed by a positive resize.
- `grid_span_fault`: throwing allocation/measurement followed by a valid generation.

Create the future public example `examples/features/grid.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
