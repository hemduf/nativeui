# TreeView<Key>

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

TreeView composes a moderate-size hierarchy with disclosures, keyed selection, and parent/child navigation. NativeUI `Tree` is the retained runtime, not a navigation widget; the new name avoids collision. Foundations: [dynamic.hpp](../include/nativeui/dynamic.hpp) and [component_tree.hpp](../include/nativeui/component_tree.hpp).

MyGo `ui/tree.go`: `Tree`, `TreeItem`. MyGo builds open nested items and navigates in visible order. The target defines a stable data snapshot shared with [OutlineView](outline_view.md), which adds virtualization.

Version studied: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions indicate the present; the following requirements form the target contract.

## 2. Public API and composition

One common target model in `collection_model.hpp`:

```cpp
template<class Key> struct TreeNode {
    Key key;
    std::optional<Key> parent;
    std::string label;
    bool enabled=true;
    bool branch=false;
    bool operator==(const TreeNode&) const=default;
};
template<class Key>
TreeView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
         Binding<std::vector<Key>> expanded);
template<class Key>
TreeView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
         State<std::vector<Key>>& expanded);
TreeView&& row(std::function<Spec(const TreeNode<Key>&)>) &&;
TreeView&& selection_mode(SelectionMode) &&;
TreeView&& on_activate(std::function<void(const Key&)>) &&;
TreeView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
TreeView&& style(TreeViewStyle) &&;
Spec spec() &&;
```

Future example:

```cpp
ui::State<std::vector<ui::TreeNode<std::string>>> nodes{{
    {"src",std::nullopt,"Sources",true,true}, {"main","src","main.cpp",true,false}}};
ui::State<ui::SelectionSnapshot<std::string>> chosen{{}};
ui::Selection<std::string> selection{chosen};
ui::State<std::vector<std::string>> expanded{{"src"}};
auto tree = ui::TreeView<std::string>{nodes,selection,expanded};
```

Rows default to Label{node.label}. Defaults: Single, indentation 16 DIP, minimum row 24 DIP; owned callbacks/factory. Flat dataset with nullable parent; sibling order = snapshot order. `branch=true` permits an expandable empty folder; a parent with children is a branch even if the flag is false.

All signatures belong to namespace `ui`. The builder is consumed by `Spec spec() &&`; children become owned `Spec` objects. Proposed declarations do not claim to be an already-delivered API. Signature blocks are fragments of members of the described type, not complete programs; `Key` or `T` corresponds to that type's template parameter where it exists.

## 3. State, ownership, and notifications

Copied/validated nodes snapshot; unique key, parent referring to a present key or nullopt. External expansion ordered by dataset order with unique keys; unknown keys ignored visually without automatic rewrite. Selection consumed through a copied binding, identities by key. A toggle produces canonical expanded, callback after an effective write; external updates do not trigger gesture callbacks. Invalid dataset Binding = last readable generation; invalid selection/expanded prevent their mutation.

The component, its state, and its notifications are confined to the UI/main thread. Genuine historical direct borrows of `State<T>&` must remain alive; constructors delegating to `state.binding()` retain the safe control block, not State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored, and `observe()` is inactive; there is no automatic destruction notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Chevron click toggles expansion without activation; row click chooses; Enter/double-click activate through callback. Up/Down/Home/End in visible order; Right opens, then on an open branch moves to the first child; Left closes, then moves to the parent. Selection modifiers follow ListView. Alt pointer toggles recursively; recursive keyboard expansion through a portable policy (Option on macOS/Shift elsewhere). Escape cancels press/buffer, never rolls back external expanded. Roving focus on one row; Tab enters/exits the group; child controls retain their focus if configured.

Routing respects inherited availability, clipping, and runtime capture. No global shortcut or audio parameter access should be added for this component.

## 5. Measurement and layout

Preorder traversal of open branches, indentation depth*16 DIP and 16 DIP chevron. TreeView materializes logically visible rows (all open branches), without an O(viewport) contract. Use Outline for large trees. Measure natural row and maximum width + indentation; internal ScrollView for overflow. Checked depth multiplication, finite/nonnegative rectangles. A reduced window wraps/clips content according to the row factory.

Sizes and positions are NativeUI logical coordinates. The backend performs scale factor conversion exactly once; the component handles no native screen coordinates.

## 6. Presentation and invalidation

TreeViewStyle contains padding, indentation, chevron, and active/selected/hover/focus row styles. Chevron indicates expanded even without animation. Collapsed descendants are excluded from layout/input; keyed content state is retained while still mounted under the chosen retained policy: here, closing unmounts visual descendants and models are externalized. Do not retain closed Components merely to claim virtualization.

Invalidation distinguishes pixel changes from metric changes. An identical effective state is a no-op; the component does not force a whole-window repaint when its bounds suffice.

## 7. Accessibility

Tree/TreeItem are absent from current roles: target Group/Custom with name, selected, expanded, and eligible Focus/Select/Expand/Collapse/Activate actions. Level/parent/index-in-set require an explicitly future neutral extension, not a native pointer. Owned read snapshots; visible preorder and new IDs after unmount.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to deferred T068; this specification validates neither VoiceOver, UIA, nor AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group`, or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare parent relationships, cycle detection, and visible order before commit. Invalid initial dataset: invalid_argument on consumption; invalid replacement retains the last accepted generation with a diagnostic, and the next valid generation recovers. Closing a branch containing active/focus: active returns to an ancestor; a single-selection gesture selects the ancestor, while multiple selection removes now-hidden descendants from its canonical selection. External closing does not automatically rewrite Selection, but runtime focus recovers. Reentrant callbacks are revalidated by key/generation.

Subscriptions, invalidators, and captures belong to each instance and are released through RAII. A started callback that throws is never automatically replayed; invariants are restored before C++ propagation. Unmounting is no-throw and triggers no application destruction callback. UI owner destruction from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

[ListView](list_view.md) models and keyboard policy, ScrollView, dynamic lifecycle. Cycles, duplicate keys, absent parent: invalid; a multiroot forest and empty data are valid. Test a new subtree during a callback, removed branch, removed/reinserted key, and unknown expansion. No recursive child callback requested from an accessible snapshot.

Reuse the `Tree`, focus, availability, and invalidation services; do not create competing local copies. IME limits remain those of [DESIGN.md §17.4](../DESIGN.md): native committed-text transport is available; full preedit/IME and candidate rectangle transport are deferred. Do not equate this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/tree_view.hpp` and `src/tree_view.cpp`.

tree_view.hpp templates adapt TreeNode/Selection, row factory, and key equality; tree_view.cpp contains the valid graph, traversal, disclosure, input, measurement, and painting of the non-template core. Reuse collection_model.hpp; never redefine ui::Tree. All newly exposed public Component classes remain declared in this header.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained measurement/layout core and, where applicable, input/paint; no empty file or central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. No Pugl, Skia, OS, plugin, or automation type enters public signatures.

## 11. Tests and acceptance criteria

- `tree_parent_child_keyboard`: Right/Left, preorder, and disabled skipping.
- `tree_forest_empty_branch`: forest, empty dataset, empty branch.
- `tree_key_identity`: sibling reorder retains models/focus by key.
- `tree_cycle_orphan_duplicate`: reject initially, retain the old generation afterward.
- `tree_recursive_expansion`: modifiers without a cycle loop.
- `tree_collapse_focus_selection`: gesture/external distinction and ancestor focus.
- `tree_dataset_during_input`: safe reentrant removal/replacement.
- `tree_expansion_throw`: single callback, restored guards.

Create the future public example `examples/features/tree_view.cpp`; `--self-test` runs this page's assertions, then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances, and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation verifies sources, signatures, and links; it reports no execution of these tests. Acceptance: all named cases pass, no historical API regressions, no global mutable dependency, and no NativeUI warnings.
