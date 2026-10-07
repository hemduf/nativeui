# Collapsible

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

A disclosure header opens/closes content. NativeUI has no such builder; [Visibility](visibility.md), [If](if.md) and focus provide the foundations.

MyGo `ui/collapsible.go`: `CollapsibleBase`, `Collapsible`, `disclosureArrow`. The target proposes a complete public recipe, keeping content mounted to preserve bindings and identities; an explicit policy allows unmounting.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
enum class DisclosureContentPolicy { Retain, UnmountWhenClosed };
template<class Child> Collapsible(std::string title, Binding<bool> open, Child&& child);
template<class Child> Collapsible(std::string title, State<bool>& open, Child&& child);
Collapsible&& content_policy(DisclosureContentPolicy) &&;
Collapsible&& on_change(std::function<void(bool)> callback) &&;
Collapsible&& style(CollapsibleStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> open{false};
auto section = ui::Collapsible{"Advanced", open, ui::Label{"Settings"}};
```

Defaults: closed according to the binding, Retain policy, a 150 ms visual animation that respects reduced motion; an empty title is permitted if an accessible name is supplied through style/the target header composition. The header is an internal part rather than a standalone file.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The external bool is authoritative; a gesture writes to Binding and calls on_change only after an effective write. An external application write adjusts content without a user on_change callback. Retain keeps the child mounted but collapsed; UnmountWhenClosed removes its structure and mounts a new identity when opened.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Clicking the header/chevron or pressing Enter/Space toggles once on release; dragging outside the hit target cancels the press without a write. Left closes and Right opens if the state changes; Escape cancels a press but does not close an already open section. No wheel input. Header focus is preserved; if content holds focus when closed, return focus to the header at the safe checkpoint. Read-only blocks toggling without making text unreadable.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

The header measures its label/chevron, with content below using the style gap. Closed: header-only metrics and no targetable content area. During animation, content is clipped and becomes non-focusable as soon as closing begins; layout may animate height within a finite bound. Reduced motion jumps to the final height. Parent width constrains descendants.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

CollapsibleStyle holds gap/padding/chevron/radius and hover/pressed/focus states. Chevron rotation communicates state but is not the only available indication. Animation is per instance and stopped when hidden/unmounted; an unchanged open value does not restart the timeline. Do not claim a Theme slot already exists.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

The header targets `Custom` or `Button` with name, expanded state and Focus/Expand/Collapse actions. The Disclosure role does not exist yet. Closed content is absent from the actionable snapshot according to availability; the header/panel relationship is a future neutral extension rather than a shipped native mapping.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Prepare the new state before writing; after a reentrant callback closes/opens the section, read the authoritative value again without looping through callbacks. Retain avoids remounting during animation. Under the Unmount policy, a throwing factory/mount uses a Tree transaction without a phantom child. Weak timer/pending invalidation is canceled at teardown.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Bool Binding, [Visibility](visibility.md), [If](if.md), clipping and focus. Cases: empty content, a long title, closing during text editing, expired state, rapid open/close and on_change exceptions. Do not port Local/MyGo’s global c.context.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/collapsible.hpp` and `src/collapsible.cpp`.

collapsible.hpp declares policy, style, builder and callbacks; only child conversion is templated. collapsible.cpp contains the header/panel, animation/measurement/layout/input/paint and adaptation to the existing Tree service. No empty .cpp wrapper around Visibility.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `collapsible_keyboard_pointer`: Enter/Space/click produce exactly one toggle.
- `collapsible_retain_vs_unmount`: identity/mount counters follow the contract.
- `collapsible_close_focus`: content focus returns to the header.
- `collapsible_rapid_animation`: coherent reversals and reduced motion.
- `collapsible_closed_hit`: the child is no longer targetable as soon as closing begins.
- `collapsible_reentrant_change`: the final external value is authoritative; the callback is not replayed.
- `collapsible_hidden_timer`: no invalidation after unmount.

Create the future public example `examples/features/collapsible.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
