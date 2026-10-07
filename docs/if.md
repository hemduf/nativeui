# If

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`If` structurally inserts or removes a child according to a bool. Source: [dynamic.hpp](../include/nativeui/dynamic.hpp), `If`, `detail::IfComponent`, `DynamicChildrenSource`.

No independent MyGo family: MyGo rebuilds imperatively under a condition. The NativeUI component differs from Visibility: false unmounts the child and true constructs a new retained instance.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> If(Binding<bool> state, Child&& child);
template<class Child> If(State<bool>& state, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<bool> detailed{false};
auto branch = ui::If{detailed, ui::Label{"Details"}};
```

Preserve these overloads and the internal logical key `if:true`. The child Spec is kept as an owned recipe rather than a Component instance reused after unmounting.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The bool Binding is copied and the Spec recipe is shared and immutable. Observe marks structure dirty; Tree reconciles at the safe checkpoint. Repeated true retains the current identity; false removes mounted nodes. Local state of a destroyed child is not retained: externalize it if necessary.

- The recipe is not executed for an initially false branch.
- Returning to true after unmounting recreates Component objects rather than their external models.
- A true/false sequence before the checkpoint adopts the latest authoritative value without manufacturing a user action.
- An application that wants to preserve the mounted instance chooses Visibility rather than If.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No focus/input of its own. Removal during focus/capture uses Tree mechanisms before disappearance. Returning to true synthesizes no activation or gesture. Tab follows only present descendants. Gesture cancellation belongs to the runtime rather than an on_false callback.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Empty = zero Size; true reports the sole child’s metrics and places it within the bounds. The parent receives structure/layout invalidation on change. A hidden child remains mounted, but If false has no phantom metrics.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting. Clear the old region on removal. Construction/reconciliation must not trigger a repaint during active paint; execution occurs at the safe checkpoint. An unchanged effective bool does not unnecessarily remount the subtree.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper `None`. A false child is absent from the snapshot; returning to true receives new node IDs and old proxies remain defunct. Sibling semantic order is preserved.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

A factory/mount exception leaves the old structure coherent or durable runtime recovery pending, without a partially published child. Do not automatically repeat a mount callback that has already started. An additional toggle after failure must be able to recover according to the existing dynamic-composition quarantine.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Reuse dynamic reconciliation, lifecycle transactions, availability recovery and [Visibility](visibility.md). Cases: initially false, rapid toggles before the checkpoint, reentrant removal in a child callback, a throwing factory and owner destruction before deferred invalidation.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/if.hpp` and `src/if.cpp`.

if.hpp exposes the builder and child adapter; if.cpp contains the dynamic bool host and its key/child source. Preserve the dynamic.hpp include and single declarations of shared utility types. Do not reimplement the Tree transaction in this component.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `if_initial_false`: no child mount/paint/focus.
- `if_toggle_lifecycle`: exact mount/unmount counts and a new ID on reinsertion.
- `if_reentrant_remove`: removal during input without UAF.
- `if_factory_fault`: no partially published child; the next toggle is recoverable.
- `if_multiple_pending`: final state at the checkpoint without duplicate action.
- Preserve `dynamic_composition_tests` and their recovery scenarios.

Create the future public example `examples/features/if.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
