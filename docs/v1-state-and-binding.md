# State and Binding in NativeUI 1.0

NativeUI provides `State<T>` as an observable UI value and `Binding<T>` as a reference-like handle to one `State<T>` source. Both are retained-UI abstractions: they are intended for the UI/main thread and are not synchronization primitives for audio, worker, or arbitrary cross-thread data flow.

This page documents behavior already merged on `main`. T069 still owns the final 1.0 public-header/API inventory and naming freeze; this page does not widen that future inventory or define the separate retained callback/lifecycle exception contracts being closed by T125/T130.

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

## Freeze boundary

This document deliberately does not define:

- a cross-thread or real-time synchronization contract for state;
- automatic retry of arbitrary callbacks after an exception;
- the final exhaustive list of 1.0 public headers, overloads, or compatibility aliases owned by T069;
- retained input/reconciliation exception behavior owned by T125;
- component lifecycle/layout/paint/native teardown exception behavior owned by T130.

Those boundaries keep the already-merged `State<T>` / `Binding<T>` value and lifetime contract documented without prematurely freezing unrelated active pre-1.0 work.
