# ForEach<T>

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`ForEach<T>` constructs a dynamic child list whose identities follow keys, but defines neither layout nor virtualization. Source: [dynamic.hpp](../include/nativeui/dynamic.hpp), `ForEach<T>`, `ForEachComponent<T>`; [dynamic_key.hpp](../include/nativeui/detail/dynamic_key.hpp), `encode_dynamic_key`.

MyGo uses Element keys in immediate composition; no equivalent independent catalog component exists. The target preserves NativeUI key constraints and transactional recovery.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Current API:

```cpp
using Items = std::vector<T>;
template<class KeyFunction, class ChildFunction>
ForEach(Binding<Items> state, KeyFunction key, ChildFunction child);
template<class KeyFunction, class ChildFunction>
ForEach(State<Items>& state, KeyFunction key, ChildFunction child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<std::vector<int>> ids{{1,2,3}};
auto items = ui::ForEach<int>{ids, [](int id){return id;},
    [](int id){return ui::Label{std::to_string(id)};}};
```

The factory takes `const T&` and returns Spec or a builder convertible through make_spec. Current keys: string/string_view-convertible types, signed/unsigned integral types and enums; other types are rejected at compile time. Preserve the current deduction guides.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The Items Binding is owned; functions adapted into std::function are owned. The encoded key is copied with s:/i:/u: prefixes rather than using T’s address or an implicit index. Identity survives reordering. The same key preserves the retained subtree: changing displayed data must be bound, as a factory does not promise child replacement for every value.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus of its own; generated wrappers retain their interactions. Removing a key recovers focus/capture through Tree. Addition/reordering generates no click. Child-content validation remains local. No selection shortcuts or virtualized wheel handling.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

The dynamic host currently measures the maximum child size on each axis and layers children within its bounds. Do not document it as an automatic Column. Use the appropriate composition/layout models to arrange items. Dataset mutations trigger structure/layout at the checkpoint.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting; Tree manages child paint. Identical keys/order do not require rebuilding recipes; internally bound updates invalidate their own widget. Factories must not draw or change the global theme.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper `None`, with children in the accepted logical order. Duplicate keys do not expose two semantic nodes with the same identity. Removing/reinserting the same key after destruction receives a new live ID without resurrecting a stale proxy.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Validate all snapshot keys before commit. Duplicates: avoid ambiguous initial publication, preserve the runtime diagnostic/quarantine policy and allow a subsequent valid generation. A throwing factory/key-function restores guards, publishes no partial children and does not automatically replay a started callback.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Dynamic source, encode_dynamic_key and safe reconciliation. Cases: empty content, an encoded duplicate, enum type, data changes with the same key, reorder/removal within a callback and a factory changing the dataset. Read a stable snapshot per pass; do not retain `const T&` after the callback.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/for_each.hpp` and `src/for_each.cpp`.

for_each.hpp retains key/factory adaptation and the typed Binding connection; for_each.cpp contains the non-template dynamic host/source, identity strings and requested reconciliation. No restriction to explicitly instantiated int/string types. dynamic.hpp and preservation-by-key behavior remain compatible.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `foreach_key_types`: signed/unsigned/string/enum keys are encoded distinctly.
- `foreach_reorder_identity`: focus/state follows the key rather than the index.
- `foreach_equal_key_data`: the child is preserved and its internal Binding updated.
- `foreach_duplicate_recovery`: invalid dataset followed by a new valid generation.
- `foreach_factory_throw`: no partial mount; recovery without replay.
- `foreach_reentrant_replace`: a coherent snapshot even when the factory changes state.
- Preserve existing dynamic recovery and quarantine epoch tests.

Create the future public example `examples/features/for_each.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
