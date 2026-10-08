# NativeUI collections and selection — current public API

This reference covers the collection value model present on current `main` and clarifies how it relates to the v1 retained state contract. It is not an assertion that all planned extensions in the [component catalog](widgets.md) are implemented. Cross-cutting subscription, callback and lifetime rules come from [State and binding](v1-state-and-binding.md).

## Public entry points

| Public header | Purpose |
| --- | --- |
| [`collection_model.hpp`](../include/nativeui/collection_model.hpp) | `SelectionMode`, `SelectionSnapshot<Key>`, `Selection<Key>`, `CollectionItem<Key>`, `TreeNode<Key>`, `ListRowHeights` |
| [`list_view.hpp`](../include/nativeui/list_view.hpp) | Retained `ListView<Key>` with optional-key single selection and a separate virtual-list constructor |
| [`grid_view.hpp`](../include/nativeui/grid_view.hpp) | `GridView<Key>` with `Selection<Key>`, cell factory and presentation options |
| [`tree_view.hpp`](../include/nativeui/tree_view.hpp) | `TreeView<Key>` with parent-key nodes, expansion state, row factory and selection policy |
| [`virtual_list.hpp`](../include/nativeui/virtual_list.hpp) | Dedicated virtual-list state and ownership contract |

These APIs use logical UI units, detached key/row values and retained UI state. They do not own native windows or provide plug-in-format, audio-thread or process-global selection services.

## Selection values, ownership and invalidation

`SelectionSnapshot<Key>` is owned data with an ordered `selected` vector and optional `active` and `anchor` keys. Equality compares all three fields. `Key` must support equality; using selection snapshots and binding values also requires the key/value types to support the copies required by their operations.

`Selection<Key>` wraps a `Binding<SelectionSnapshot<Key>>` from an owning `State`. Constructing the controller from a `State&` does **not** transfer ownership of the State. `binding()` returns another handle to the same source; `snapshot()` returns an owned copy, not a reference that may dangle. Destroying the owner makes `valid()` false and disables future writes, while the retained Binding control keeps the last committed snapshot readable.

`SelectionMode::Single` and `Multiple` are policies configured by the consuming collection widget. `Selection::set()` is a general value operation: it does not enforce Single mode, ensure that `active`/`anchor` belong to `selected`, check whether item keys are present, or automatically remove disabled keys. Such normalization, when needed, belongs to the view/application policy.

### Conditional `Selection::set()`

1. It snapshots the source revision, canonicalizes duplicate selected keys (keeping the first occurrence) and compares the candidate to the current snapshot.
2. A missing owner, changed revision, or equal candidate returns `false` without committing a new value.
3. Accepted changes use `Binding::set_if` and inherit synchronous observer notification, reentrancy, allocation and exception behavior from `State`.
4. The return value reports **synchronous acceptance**, not a guaranteed eventual outcome. A recursive write queued during another observer pass can still be pending when `set()` returns `false`.

Directly writing `State<SelectionSnapshot<Key>>` bypasses the controller's duplicate-key normalization. Applications that need canonical keys should route the relevant mutations through `Selection::set()`.

```cpp
#include <nativeui/collection_model.hpp>

ui::State<ui::SelectionSnapshot<int>> source{ui::SelectionSnapshot<int>{}};
ui::Selection<int> selection{source};

// Keeps keys 3 and 5 in order; no implicit repair of active/anchor.
const bool accepted_now = selection.set({
    .selected = {3, 3, 5},
    .active = 3,
    .anchor = 5,
});
const auto owned = selection.snapshot();
(void)accepted_now;
(void)owned;
```

This is a public-API illustration, not a claim that this snippet was compiled in the current documentation run.

## Item and tree models

`CollectionItem<Key>` stores a key, label, `enabled` and `section_header` flags. `TreeNode<Key>` additionally stores an optional parent key and `branch`. A missing parent identifies a root in the declared tree model. Neither value struct enforces uniqueness, acyclicity, parent existence or layout constraints by itself; use the consuming widget's implementation and tests for those guarantees.

`ListRowHeights` defaults to a 24-logical-unit estimate with variable-height policy enabled. It is configuration, not a promise about measured row height in a particular viewport or render backend.

## Which collection API to use?

| Need | Public composition API | Selection representation |
| --- | --- | --- |
| Static retained list or virtualized list | `ListView<Key>` | `State<std::optional<Key>>` / `Binding<std::optional<Key>>` or `VirtualListState<Key>` |
| Keyed two-dimensional cells | `GridView<Key>` | `Selection<Key>` / `SelectionSnapshot<Key>` |
| Hierarchical parent-key nodes | `TreeView<Key>` | `Selection<Key>` plus an independent expansion `State<std::vector<Key>>`/binding |

`ListView<Key>::item(key, content, enabled)` rejects duplicate keys via `std::invalid_argument`; calling `item()` after constructing from a virtual-list state rejects mixing two data-source modes via `std::logic_error`. Its current single optional-key binding must not be confused with `Selection<Key>` in the grid/tree APIs.

`GridView<Key>` and `TreeView<Key>` take a **borrowed** `Selection<Key>&` during composition to obtain their Binding. The mounted view and observer wiring therefore depend on the owning state lifetime, not a copy of the controller. Constructors also accept a `State` reference or Binding for the item/tree data source. These are retained UI-only APIs; construct/mutate on the UI thread.

```cpp
#include <nativeui/collection_model.hpp>
#include <nativeui/grid_view.hpp>
#include <nativeui/label.hpp>

ui::State<ui::SelectionSnapshot<int>> selection_state{ui::SelectionSnapshot<int>{}};
ui::Selection<int> selection{selection_state};
ui::State<std::vector<ui::CollectionItem<int>>> items{
    std::vector<ui::CollectionItem<int>>{{1, "One"}, {2, "Two"}}};

auto grid = ui::GridView<int>{items, selection}
    .selection_mode(ui::SelectionMode::Multiple)
    .cell([](const ui::CollectionItem<int>& item) -> ui::Spec {
        return ui::Label{item.label}.spec();
    })
    .spec();
(void)grid;
```

Check [the component implementation inventory](widgets_implementation.md) before relying on target-only extensions from older catalog pages. The two snippets above are illustrative; exact build evidence must come from repository snippet fixtures and CI rather than this document.

## Failure, performance and host boundaries

- Selection reads copy a snapshot and can allocate. Equality and callbacks are user-code execution points; exceptions are not suppressed by the value wrapper.
- A revision change during preparation refuses the stale write rather than overriding a newer selection. Observer exceptions use the `State` propagation/recovery contract.
- Keep state and retained views in one UI/main-thread domain. A plug-in editor closing does not imply that the owner-managed state must be destroyed, but destroying the owner intentionally invalidates its selection bindings.
- The model does not guarantee constant-time key lookup or zero-allocation selection updates. Stable keys are essential when retained views reorder or virtualize rows.

See also [Composition, layout and widgets](v1-composition-layout-and-widgets.md), [State and binding](v1-state-and-binding.md), and [Services, testing and limits](v1-services-testing-and-limits.md).
