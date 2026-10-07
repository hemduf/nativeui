# ListView<Key>

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`ListView<T>` exists with single selection, retained items, activation, and ListViewStyle in [widgets_list_tabs.inc](../include/nativeui/detail/widgets_list_tabs.inc). The [VirtualListState](../include/nativeui/virtual_list.hpp) path virtualizes fixed-height rows with overscan, focus/capture exceptions, and immutable metadata.

MyGo `ui/list.go`, `List`, `ListState`, `layoutList`, `listHeights`, `navigate`, `reorder`, `pinHeader`: variable heights, key anchoring, multiple selection, typeahead, follow-end, sticky sections, and reordering. These are target extensions; they are not already present in NativeUI.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Current API to preserve:

```cpp
explicit ListView(Binding<std::optional<T>> selection);
explicit ListView(State<std::optional<T>>& selection);
explicit ListView(VirtualListState<T>& state);
template<class Child> ListView&& item(T key, Child&& child, bool enabled=true) &&;
ListView&& on_activate(std::function<void(const T&)> callback) &&;
ListView&& style(ListViewStyle value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<std::optional<int>> selected{std::nullopt};
auto list = ui::ListView<int>{selected}.item(1,ui::Label{"First"})
    .item(2,ui::Label{"Second"}).on_activate([](const int&){});
```

Common **target** models, declared once in `collection_model.hpp` with the core in `collection_model.cpp`:

```cpp
enum class SelectionMode { Single, Multiple };
template<class Key> struct SelectionSnapshot {
    std::vector<Key> selected;
    std::optional<Key> active;
    std::optional<Key> anchor;
    bool operator==(const SelectionSnapshot&) const=default;
};
template<class Key> struct CollectionItem {
    Key key;
    std::string label;
    bool enabled=true;
    bool section_header=false;
    bool operator==(const CollectionItem&) const=default;
};
template<class Key> class Selection {
public:
    explicit Selection(Binding<SelectionSnapshot<Key>> value);
    explicit Selection(State<SelectionSnapshot<Key>>& value);
    Binding<SelectionSnapshot<Key>> binding() const;
    SelectionSnapshot<Key> snapshot() const;
    bool valid() const noexcept;
    bool set(SelectionSnapshot<Key> value);
};
struct ListRowHeights { double estimate=24.0; bool variable=true; };
```

Target extensions: `ListView(Selection<Key>&)`, `.selection_mode(SelectionMode)` default Single, `.on_selection_change(std::function<void(const SelectionSnapshot<Key>&)>)`, `.on_reorder(std::function<void(const std::vector<Key>&,std::optional<Key> before)>)`, `.follow_end(bool=true)`, `.typeahead(bool=true)`. VirtualListState adds constructor `(Selection<Key>&, ListRowHeights, RowFactory, std::size_t overscan=2)` and operations `scroll_to_end()`, `visible_range()` with an exclusive end, `at_end()`. The factory receives an owned Item for the call; new section_header metadata is added without breaking the current Item constructor.

For the nonvirtualized path, a separate target addition:

```cpp
template<class Child>
ListView&& item(CollectionItem<Key> metadata, Child&& child) &&;
```

This overload owns key/label/enabled/section_header, then the child Spec. The historical `.item(Key, Child, bool)` overload remains unchanged and never infers a label from rendering or the key: its rows without textual metadata are excluded from typeahead. Virtualized rows use their existing `Item.name`. The search engine neither constructs nor inspects child components to obtain these labels.

The current fixed-height/optional State constructor remains unchanged; `replace(std::vector<Item>)` and its validation bool are preserved. New collections share these models through [Shared models](widgets.md#modeles-partages), without claiming they exist today.

`Selection::set` returns true only if a valid Binding receives a different effective snapshot; false for a no-op or invalidity. It removes duplicate selected keys, retaining their first occurrence, without dataset knowledge. active/anchor may identify a nonselected key (Primary navigation) or one absent from the dataset; the controller cannot validate them against a collection. The widget resolves dataset order and eligibility at gesture time. A throwing setter notification may follow the authoritative commit: restore guards, then propagate without replay. The controller does not claim to undo the commit after a callback starts.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

- Selection is a UI controller over a copied Binding; the widget does not retain Selection* after spec.
- selected contains unique keys ordered by logical dataset order; active is the keyboard cursor, anchor is the extension origin.
- Display only keys still present/enabled/non-header; external unknown keys do not automatically write the model. In Single mode, if external selected contains multiple keys, render only the first eligible key in dataset order without writeback; the next gesture canonicalizes to a single selection.
- A gesture publishes a canonical snapshot in one set, then a user callback only if changed.
- An external event does not trigger the gesture on_selection_change; an invalid Binding prevents writes/callbacks and retains the last readable state.
- New keys are copyable and equality-comparable, without requiring hashing; the non-template adapter assigns owned monotonic tokens. Preserve the currently supported types and encoding of the fixed path.
- Generation preparation distinguishes metadata copying from key resolution. Historically encodable keys (strings, integers, enums) allow an O(N log N) sorted index. An arbitrary user type comparable only by equality uses a linear-search fallback: deduplication, rematching, and parent resolution may cost O(N²). No hash is required and no O(N) preparation promise is made for this fallback. After resolution, scrolling operations use prepared tokens/indices and retain the bounds below.
- Validate the dataset/semantic snapshot before the generation; reordering retains tokens for present keys, removal then reinsertion creates a new token.
- No callback retains Item&, Key&, or Node* beyond its stack.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- A single click selects an item; Primary modifier (Cmd on macOS/Ctrl elsewhere) toggles; Shift extends from anchor, Primary+Shift adds a range.
- Up/Down/Home/End and PageUp/PageDown skip disabled/headers; Primary+arrows moves active without changing selected; Primary+Space toggles active.
- Primary+A selects all eligible items in Multiple mode; Single retains historical behavior.
- Typeahead on committed text compares labels with ASCII case-insensitive folding and exact non-ASCII UTF-8, a 700 ms buffer, and repeated-letter cycling after active; Escape clears the buffer. This target default differs from MyGo Unicode case folding and assumes no absent Unicode backend.
- Enter/double-click activate at most once through on_activate; historical Space on the single-selection path retains compatible activation, while multiple-selection Primary+Space takes priority.
- Opt-in reorder after a 4 DIP threshold: per-instance capture, preview, and autoscroll; up calls on_reorder with keys in their current order and a before key or nullopt for the end; the application modifies the dataset.
- Escape/PointerCancel cancel reorder without modifying the dataset; removing a draggable key cancels the operation.
- Read current state at release; read-only allows navigation but refuses writes/reorder. Interactive children retain priority over selection if they consume the event.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

- Current fixed path: finite positive float height, content height validated before replace; the same arithmetic formulas remain available.
- Target variable path: double cumulative sums and a height cache by key + measurement width; strictly positive estimate before actual measurement.
- Retain anchor `{key,inset double}` so measurements/insertion above do not move the first visible row.
- If the anchor is removed, choose the next survivor in old order, then the previous one, otherwise the start/end according to follow policy.
- Materialize viewport + overscan 2 on each side and focus/capture exception rows; no O(N) Components.
- Prefix-sum index maintained by dataset/height; ordinary scrolling O(log N + V), without a full key scan each frame.
- Zero/nonfinite height from the factory: minimum 1 DIP for a progress bound and diagnostic; maximum 4096 materializations per pass to avoid a loop with no space.
- Width change invalidates measured heights but retains the anchor; bounded measurement convergence, fill the viewport before publication without a frame containing gaps.
- Follow-end: active only if the user was at the end (1 DIP tolerance), new content retains the end; scrolling away suspends it, returning to the end resumes it.
- A sticky section header is visually detached without a second logical item; the next header pushes the previous one, while hit/clip remain consistent.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

Existing ListViewStyle retains its fields and default resolver. Explicit style extensions (sticky header, insertion indicator) in the same type or versioned options without assumed new Theme slots. Selected/active/focus are distinct; do not color disabled items as selected through an invalid snapshot. Offset alone requests paint/necessary materialization, not O(N) metadata copying. Per-instance owned cache.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

The normative ListView/ListItem target contract is [accessibility.md](accessibility.md); current headers provide virtual metadata, but complete hooks/native roles are not delivered. Preserve the `VirtualSemanticChildren::item_at` invariant without a factory or mutation. Variable height/multiselection require a **new** snapshot extension: shared immutable geometry/index and selected tokens, without silently changing existing fixed geometry. Reads of an older generation remain consistent; removed tokens are defunct.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Replace prepares keys/metadata/heights/token mapping before commit; false validation leaves the old generation intact. Factory/mount failure does not advance visible tokens or capture indices before commit. After commit, a required refresh remains durably dirty if it throws. A reorder callback may modify/replace the dataset; keep owned keys/generation and revalidate before the next access. Stop the typeahead timer/autoscroll while hidden/unmounted; nothing migrates to another instance.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[ScrollView](scroll_view.md), dynamic reconciliation, ListViewStyle, semantic snapshots, and common target models. Duplicate item keys: reject before commit; invalid fixed height constructor throws invalid_argument, geometry overflow makes replace false. Empty dataset = no active item, no activation, zero offset. Absent scroll key/out-of-range index = false without change. An off-viewport focus row is temporarily retained under exception-row rules; limits/recovery when removed are explicit.

`Key::PageUp` and `Key::PageDown` are target additions at the end of the current portable enum, with platform translation and tests. The current source does not define them. Typeahead uses committed-text InputEvent objects; no complete native IME support is assumed.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/list_view.hpp` and `src/list_view.cpp`.

list_view.hpp/cpp contain builder, state/controller declarations, and the non-template retained list core. virtual_list.hpp remains a compatible include, and VirtualListState remains public with its historical Item/Runtime/MetadataSnapshot aliases preserved or compatible adapters. Key/RowFactory templates adapt owned keys/Spec to type erasure; not only int/string instantiations. Extraction removes implementations from widgets_list_tabs.inc and detail virtual list where replaced, without breaking list_tabs_style.hpp or widgets.hpp includes.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `list_fixed_legacy`: current API, fixed geometry, overscan, Space/Enter, and duplicate .item.
- `list_variable_anchor`: height measurement/insertion/reorder above without a visual jump.
- `list_variable_resize`: width changes remeasure with the same key/inset.
- `list_multiple_ranges`: modifiers, active/anchor, disabled/header skipping, Primary+A.
- `list_reorder_keys`: before nullopt for end, single callback, cancellation/removal during drag.
- `list_follow_end`: append/height growth at end, scroll away, return to end.
- `list_sticky_header`: push-off and a single logical identity.
- `list_semantic_generation`: metadata pointer stable outside dataset changes; tokens not reused.
- `list_snapshot_no_factory`: semantic reads never call application callbacks.
- `list_replace_faults`: validation/allocation/factory/mount failures and refresh after commit.
- `list_zero_height_bound`: no infinite materialization.
- `list_invalid_binding`: no write/gesture callback after invalidation.
- `list_eager_typeahead_metadata`: only rows with metadata.label or Item.name participate; the historical overload remains compilable without inspecting Spec.
- `list_single_external_multiple`: first eligible dataset key visible, no automatic external correction.
- `list_generic_equality_fallback`: nonhashable user key accepted, preparation cost explicitly distinct from scrolling.
- `list_million_rows`: encodable integer keys, Comp counts O(V+overscan+exceptions), scrolling without full scan.

Create the future public example `examples/features/list_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
