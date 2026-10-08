# State and Binding in NativeUI 1.0

NativeUI provides [`State<T>` and `Binding<T>`](../include/nativeui/state.hpp) as its observable UI-value layer. Both are retained-UI abstractions: they are intended for the UI/main thread and are not synchronization primitives for audio, worker, or arbitrary cross-thread data flow.

The same state layer drives retained availability decorators in [`component_state.hpp`](../include/nativeui/component_state.hpp), while [`DirtyRegion`](../include/nativeui/invalidation.hpp) is the bounded logical invalidation accumulator used by the retained renderer.

This page documents behavior already merged on `main`. The current validated public/package surface on `main` is authoritative; T069 is no longer treated as a future freeze gate. The retained callback/lifecycle exception-safety work tracked by T125/T130 has also landed, so this page can state the final state/binding contract without preserving those historical blocker assumptions.

## State values

`State<T>` owns one current value. `T` must support equality comparison because writes equal to the current value are ignored.

```cpp
#include <nativeui/state.hpp>

ui::State<int> count{0};
count.set(1);
const int current = count.get();
```

`get()` returns the current value by const reference. `set()` is synchronous on the calling UI thread.

## Observing changes

`observe()` registers a callback and returns a move-only subscription. Destroying or resetting the subscription removes the observer.

```cpp
ui::State<int> count{0};

auto subscription = count.observe([](const int& value) {
    // Runs synchronously as part of count.set(...).
});

count.set(1);
subscription.reset();
```

Notification passes have deterministic mutation rules:

- one pass exposes one stable committed value;
- observers added during a pass start with a later pass, not the pass already in progress;
- an observer removed before its turn is skipped;
- recursive writes do not mutate the value visible to the current pass;
- recursive writes are coalesced so the latest pending value is used for the next pass;
- recursively writing the already-current value can cancel an earlier pending write.

These rules avoid cloning the complete callback list on every ordinary `set()` while keeping synchronous observation deterministic.

## Observer exceptions

An observer may throw to a direct C++ caller. The current value is already committed when observer notification starts.

If an observer throws:

- notification of the current pass stops;
- callbacks that had not started are not synthetically retried for that failed pass;
- a recursive pending write from the failed pass is discarded;
- NativeUI restores its notification bookkeeping and listener registry before rethrowing the original exception;
- exception cleanup invokes no additional observer callback;
- a later explicit `set()` starts a fresh notification pass.

Code that needs an application-level retry policy should implement it explicitly rather than assuming `State<T>` replays a partially completed notification pass.

## Subscription and source lifetime

Subscriptions do not keep a destroyed `State<T>` owner logically alive. They use lifetime-safe shared control internally and become inactive when the source owner is destroyed.

```cpp
auto subscription = some_state.observe(callback);
if (subscription.active()) {
    // The source is still alive and this listener is registered.
}
```

Destroying a `State<T>` invalidates its source, removes its subscriptions, and prevents later callbacks from being delivered through those subscriptions.

## Binding<T>

`Binding<T>` is obtained with `State<T>::binding()` and refers to the same state source.

```cpp
ui::State<int> count{0};
auto binding = count.binding();

binding.set(2);
const int value = binding.get();
```

A binding is copyable and reference-like. Copying it preserves source identity. Move construction/assignment intentionally preserves a valid source handle in the moved-from binding as well; there is no public empty-binding state created by move.

A `Binding<T>` retains the source control block, not the `State<T>` object itself. When the owning `State<T>` is destroyed:

- `binding.valid()` becomes `false`;
- `binding.get()` can still read the retained last value while the binding exists;
- `binding.set(...)` becomes a no-op;
- `binding.observe(...)` returns an inactive subscription.

This lets retained components hold a binding safely without extending the logical lifetime of the owning application state.

## Binding and notification semantics

`Binding<T>::set()` and `Binding<T>::observe()` use the same source and the same notification transaction as the corresponding `State<T>` operations. There is no second observer list or synchronization layer.

Consequently, writes through a binding follow the same equality, reentrancy, coalescing, source-lifetime, and throwing-observer rules described above.

## Availability decorators

[`component_state.hpp`](../include/nativeui/component_state.hpp) exposes three one-child retained decorators driven by borrowed State objects:

| Decorator | State | Effective behavior |
| --- | --- | --- |
| `Visibility` | `State<VisibilityMode>` or `State<bool>` | Visible, Hidden or Collapsed retained availability |
| `Enabled` | `State<bool>` | Enabled/disabled interaction and focus eligibility |
| `ReadOnly` | `State<bool>` | Monotonic read-only capability without generic focus/hit-test suppression |

These decorators intentionally differ from Binding-based components: each builder stores a raw pointer to the supplied State, and the Spec produced by `spec() &&` keeps that pointer for later component materialization. The State must therefore outlive **both the produced Spec and every retained wrapper materialized from it**; constructing a Spec and then destroying its State before mounting violates the API lifetime contract. NativeUI does not promote this borrow to shared ownership.

Construction owns/converts the child Spec but performs no subscription or Tree mutation. Observation begins when the retained wrapper mounts. State notifications run synchronously in the UI/main-thread domain and request availability reconciliation through the retained Tree; they do not simulate structural removal. These decorators are not synchronization primitives and are not audio/DSP real-time APIs.

### Visibility

With `State<VisibilityMode>`, the state value is used directly.

With `State<bool>`, true means Visible and false means Hidden by default:

```cpp
ui::State<bool> details_visible{true};

auto details = ui::Visibility{
    details_visible,
    ui::Label{"Advanced settings"}
};
```

Use `.mode(ui::VisibilityMode::Collapsed)` when false should also remove the subtree's layout contribution. Passing `Visible` as the false-mode is sanitized to `Hidden`, so false always makes the subtree unavailable. `mode()` only configures the bool-State form; a `State<VisibilityMode>` already supplies the complete mode and ignores that fallback setting.

Hidden keeps layout participation but removes paint/normal interaction/focus eligibility. Collapsed additionally removes layout contribution. Neither state unmounts the retained child. The wrapper itself paints no pixels and forwards one-child layout; descendants resolve their own enabled/read-only presentation against effective inherited availability.

### Enabled

A disabled subtree remains retained, measured and painted but is unavailable for normal focus and interactive targeting. Effective enabled state is monotonic through ancestry: a child cannot opt back into enabled below a disabled ancestor.

### ReadOnly

Read-only is also monotonic through ancestry, but unlike disabled it does not generically remove focus or pointer targeting. Editable/value controls enforce mutation policy while retaining non-mutating behavior such as selection, navigation or copy when their widget contract supports it.

### Builder/materialization contract

`Visibility`, `Enabled` and `ReadOnly` are rvalue-style declarative builders. Their `spec() &&` methods consume the child specification and retain the borrowed State pointer for later materialization. They do not return a live component reference and do not extend application State lifetime.

Component creation, State observer registration and later availability reconciliation occur in the owning Tree/UI lifecycle. Allocation or lifecycle exceptions follow the normal retained rollback/propagation contract; there is no decorator-specific error code or fallback State. State callbacks can run synchronously as part of `State::set()`, so application code must respect the same reentrancy rules described for State observers above.

## DirtyRegion and paint invalidation

[`DirtyRegion`](../include/nativeui/invalidation.hpp) accumulates dirty rectangles in logical coordinates. It coalesces overlapping/touching regions and retains at most `DirtyRegion::kMaxRects` (8) disjoint rectangles. A ninth fragment collapses the set to one bounding rectangle, keeping repaint bookkeeping bounded.

```cpp
ui::DirtyRegion dirty;
const ui::Rect viewport{0, 0, 800, 600};

if (auto exposure = dirty.add({20, 20, 100, 40}, viewport)) {
    // exposure is the merged logical region that newly needs presentation.
}
```

`add()` clips to the supplied bounds, returns `std::nullopt` for empty/already-covered input and performs no allocation after successful DirtyRegion construction. `rects()` returns a borrowed vector reference that is invalidated by later mutation/assignment/destruction.


## Generic edit sessions

[`edit.hpp`](../include/nativeui/edit.hpp) adds explicit user-edit lifetimes around `Binding<T>` without adding plug-in parameter IDs, normalization, host-automation queues, or audio-thread semantics.

`EditSource` labels pointer, keyboard, wheel, or accessibility-originated edits. `EditCallbacks<T>` is owned by the session. The `const T&` supplied to `change` is borrowed only for that callback; copy it if it must survive. Callback captures otherwise follow normal C++ ownership rules.

For a changed value the synchronous order is: begin, State commit and State observers, change with the effective committed value, then exactly one terminal end or cancel. Equal values emit no change. Direct State/Binding writes emit no edit callbacks.

Cancellation is not rollback: the latest committed State value remains. Reentrant begin/update/set/finish attempts are rejected. Reentrant end/cancel requests are deferred until the current callback returns, with cancel taking precedence. A throwing begin/change/State observer cancels once and rethrows the original error; terminal callbacks are not retried. Destructor cancellation is no-throw.

`set(value, source)` is the discrete-command form. `finish(value)` commits a final value and ends while retaining internal lifetime state so observer-driven owner destruction is safe. Destroying the originating State invalidates future mutation and causes an active session to cancel when it next observes that invalid source.

```cpp
ui::State<double> gain{0.5};
ui::EditSession<double> edit{
    gain.binding(),
    {
        .begin = [](ui::EditSource) {},
        .change = [](const double& committed, ui::EditSource) {
            (void)committed; // borrowed only for this callback
        },
        .end = [](ui::EditSource) {},
        .cancel = [](ui::EditSource) {},
    }};

edit.begin(ui::EditSource::Pointer);
edit.update(0.7);
edit.finish(0.8);

// Model synchronization does not create an edit lifetime.
gain.set(0.25);
```

Edit sessions are UI/main-thread operations. They may allocate and invoke application callbacks, so they are neither cross-thread synchronization nor audio/DSP real-time APIs.

## Threading boundary

`State<T>` and `Binding<T>` do not add mutexes, atomics, or cross-thread scheduling. Their values and listener registries are UI/main-thread state.

When data originates on another thread, use an explicitly reviewed bridge appropriate to that producer (for example, atomics, queues, immutable snapshots, or the NativeUI dispatcher where applicable) and perform `set()` / `observe()` interaction in the UI domain. In particular, do not use `State<T>` or `Binding<T>` directly as audio-thread transport.

## Ownership guidance

A practical ownership split is:

- application/controller code owns `State<T>` values;
- retained UI components receive `Binding<T>` handles when they need read/write access to the same source;
- observers keep their `Subscription` for exactly as long as they need notifications;
- cross-thread producers hand data into the UI domain before mutating state.

Legacy component entry points that accept `State<T>&` remain source-compatible where the corresponding migration was delivered; internally, migrated writable stateful components can retain bindings instead of borrowing the `State<T>` object lifetime.

## Scope boundary

This document deliberately does not define:

- a cross-thread or real-time synchronization contract for state;
- automatic retry of arbitrary callbacks after an exception;
- an exhaustive inventory of every public header, overload or compatibility alias outside the state/binding surface;
- renderer, platform or retained-tree implementation mechanisms that remain private even though their exception-safety contracts have landed.

The current `main` public/package surface and exact-head validation remain authoritative. This chapter documents the supported `State<T>` / `Binding<T>` value, lifetime, reentrancy and exception behavior without widening unrelated APIs.
