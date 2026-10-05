# OutlineView<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

OutlineView displays a hierarchy as virtualized rows. It complements [TreeView](tree_view.md) for large datasets and uses [ListView](list_view.md) anchoring. NativeUI currently has no Outline.

MyGo `ui/outline.go`: `OutlineState`, `flatten`, `Outline`, `setOpen`, `keys`, `closed`. Keys and openness survive visibility changes in the listing. The port must preserve logical identities without constructing a Component per node.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
template<class Key>
OutlineView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
            Binding<std::vector<Key>> expanded);
template<class Key>
OutlineView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
            State<std::vector<Key>>& expanded);
OutlineView&& row(std::function<Spec(const TreeNode<Key>&)>) &&;
OutlineView&& row_heights(ListRowHeights) &&;
OutlineView&& selection_mode(SelectionMode) &&;
OutlineView&& on_activate(std::function<void(const Key&)>) &&;
OutlineView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
OutlineView&& style(OutlineViewStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::TreeNode<int>>> nodes{{
    {1,std::nullopt,"Root",true,true},{2,1,"File"}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
ui::State<std::vector<int>> expanded{{1}};
auto outline = ui::OutlineView<int>{nodes,selection,expanded}
    .row_heights(ui::ListRowHeights{.estimate=28.0,.variable=true});
```

TreeNode model defined in [TreeView](tree_view.md), Selection/row heights in ListView. Default Single, estimate24/variable, overscan2. Target controller `OutlineState<Key>` in this pair: `scroll_to_key(key,ScrollAlignment=Nearest) -> bool`, `visible_rows()`, `item_at_visible_index(index) -> optional<Key>`, `depth(key) -> optional<size_t>`; `.state(OutlineState<Key>&)` is opt-in, and the view copies its safe control token.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Retain owned graph metadata and flattened visible rows `{key,parent_key,depth,branch}`, immutable per generation. Expansion state is distinct from selection; unknown keys ignored without rewriting. Selection order follows visible preorder. Per-instance scroll/focus controller and copied Binding; key tokens follow a node present in the dataset even if its branch is closed, unlike actual removal. Invalid Binding snapshots refuse mutation/synthetic callbacks.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Right/Left/recursive toggle navigation identical to TreeView; Up/Down/Page/Home/End and typeahead follow ListView. Clicking an arrow does not activate the row; double-clicking a label does not implicitly open it unless the application chooses that action. Multiple-selection modifiers defined by ListView. Focus/active descendant hidden by closing returns to its parent; gestures canonicalize selection as TreeView does. ScrollView wheel, no permanent capture; safe capture/press teardown.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Graph validation/indexing at a new generation: O(N log N) with encodable keys and a sorted index, up to O(N²) with equality-only keys, under the shared [ListView](list_view.md) contract; resolved graph traversal O(N). flatten is O(number of open nodes visited). Visual Components O(V+overscan+focus/capture exceptions) and scrolling O(log visible count+V). Height cache by key+width, double cumulative sums. Expansion above preserves anchor key/inset; if the anchor becomes hidden, use a visible parent, then the next survivor. Collapsed metadata instantiates no rows. Hidden scroll_to_key returns false without forcing ancestors open; the application explicitly commands expanded. Height/depth overflow rejected before commit.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

OutlineViewStyle applies TreeView indentation/chevron and the list row recipe with explicitly local gap/padding. Expansion animation may rotate the chevron for 150ms, without unbounded animation of all virtual rows. Width changes remeasure while retaining the anchor; selected-row paint does not rebuild the graph. Per-instance timers stop while hidden/unmounted.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Provisional Group/Custom for absent tree semantics; a neutral level/parent/action extension is required. Full logical data-only metadata; virtual reads call neither the row factory nor application flatten code. Variable bounds calculated on a shared geometry generation. Closing hides accessible nodes without reusing a token for another key; dataset removal then reinsertion gets a new token.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare flattened generation and prefix cache before commit; an invalid graph retains the old generation. Throwing RowFactory/measure: the window retains the old consistent materialization or durable dirty work, without publishing a gap. Closing under focus/capture resolves stable identity at the checkpoint. An expansion callback may reorder or remove nodes; revalidate generation and retain owned origin keys. Controller destruction invalidates operations safely.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[TreeView](tree_view.md) graph model, [ListView](list_view.md) variable engine, [ScrollView](scroll_view.md). The source is not the retained Tree runtime. Cases: a million nodes without all being open, cycles/orphans, great depth, a branch node with no children, disabled rows, and invalid expansion. No implicit network lazy loading in children queries; the application supplies subsequent snapshots.

`Key::PageUp` and `Key::PageDown` are target additions at the end of the current portable enum, with platform translation and tests. The current source does not define them. Typeahead uses committed-text InputEvent objects; no complete native IME support is assumed.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/outline_view.hpp` and `src/outline_view.cpp`.

outline_view.hpp exposes builder/OutlineState and key/factory adapters; outline_view.cpp contains flatten/index mapping, disclosure input, variable window, measure/layout/paint. The virtual list core is shared, not copied. TreeNode/Selection are defined once; no colliding public Tree class.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `outline_virtual_count`: large tree, Comp count bounded by viewport.
- `outline_expand_anchor`: opening/closing above without a jump.
- `outline_hidden_scroll_key`: false, without implicit expansion.
- `outline_focus_collapse`: ancestor recovery for single/multiple selection.
- `outline_variable_indent`: wrapping changes with depth and width.
- `outline_graph_fault`: cycles/orphans/invalid replacement without partial commit.
- `outline_tokens_fold_remove`: stable folding, new token on removal/reinsertion.
- `outline_factory_reentrant_throw`: owned current snapshot and safe recovery.
- `outline_controller_lifetime`: destroyed state, inert stale callbacks/scroll.

Create the future public example `examples/features/outline_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
