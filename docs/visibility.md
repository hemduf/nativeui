# Visibility

Status: **existing — enhancements required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`Visibility` propagates availability through its subtree without changing mounted identity. The wrapper exists in [component_state.hpp](../include/nativeui/component_state.hpp), classes `Visibility` and `detail::VisibilityComponent`; modes are in [component_base.hpp](../include/nativeui/component_base.hpp).

It currently borrows `State<VisibilityMode>` or `State<bool>` and has no Binding constructor. MyGo expresses these states through Element flags; there is no separately documented equivalent family. The enhancement adds the same safe Binding access as other wrappers.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API to preserve:

```cpp
template<class Child> Visibility(State<VisibilityMode>& state, Child&& child);
template<class Child> Visibility(State<bool>& visible, Child&& child);
Visibility&& mode(VisibilityMode value) &&;
Spec spec() &&;
```

Verified existing example:

```cpp
ui::State<bool> visible{true};
auto panel = ui::Visibility{visible, ui::Label{"Details"}}
    .mode(ui::VisibilityMode::Collapsed);
```

Target: identical overloads taking `Binding<VisibilityMode>` and `Binding<bool>`. State overloads delegate to binding; preserve mode, which is useful only for the bool form. False + mode Visible is sanitized to Hidden.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

- `Visible`: the subtree is painted/targetable according to its other states.
- `Hidden`: no paint/input/semantics, but layout space is preserved.
- `Collapsed`: no space, paint/input/semantics.
- Children remain mounted: `If` provides structural removal when desired.
- Subscriptions observe the mode and invalidate availability; unchanged state changes nothing.
- Children cannot restore Visible when an ancestor is unavailable.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

When hiding, Tree recovers focus/hover/capture and handles canceled gestures. Reappearance synthesizes no pointer-down or activation. The wrapper has no focus or confirmation of its own. Child interactions remain unchanged while Visible and available.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

A transparent wrapper, using the child’s measurement/minimum and the same bounds. Hidden preserves metrics; the runtime removes Collapsed from allocation without leaving a phantom gap. A mode transition may invalidate layout only if its effect on metrics changes.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting; the old visible area must be cleared when hiding. Descendants classify their invalidations; the wrapper must not impose a global repaint. Descendant animations are suspended while invisible and resume with coherent state without replaying actions.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

`None` for the wrapper. Hidden/Collapsed and their descendants are absent from the snapshot; reappearance preserves retained identities that are still alive. No artificial value-change announcement; publish structure/focus according to the runtime.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

The new Binding must remain inert when its owner has expired; remove retained State pointers in the target without changing public signatures. Removal during notification respects Tree checkpoints. After an invalidation failure, retain durable effective-mode/dirty state so recovery can finish at the next checkpoint.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on availability recovery and [If](if.md) for a structural alternative. Cases: bool false/mode Visible, Hidden/Collapsed ancestors, focused text selection, captured panning when hiding and state expiration. No independent “visibility manager” service.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/visibility.hpp` and `src/visibility.cpp`.

Extract builder/runtime into visibility.hpp/cpp; keep component_state.hpp as a compatible facade. The .cpp contains non-template availability runtime and the owned Binding. Preserve enums and `detail` types without turning them into new public services.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `visibility_modes_space`: Hidden preserves size; Collapsed removes it.
- `visibility_bool_sanitize`: false+Visible produces Hidden.
- `visibility_inherited`: a Visible child cannot override a Hidden ancestor.
- `visibility_capture_focus`: hiding cancels capture and recovers focus.
- `visibility_binding_expired`: expiration without a stale State*.
- `visibility_fault_reconcile`: throwing invalidation followed by usable state.

Create the future public example `examples/features/visibility.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
