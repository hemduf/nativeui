# CommandScope

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`CommandScope` handles portable commands bubbling from descendants without intercepting ordinary input. Source: [command.hpp](../include/nativeui/command.hpp), `CommandCallback`, `CommandScope`, `CommandScopeComponent`.

MyGo `ui/scope.go`, `overlayShortcut`, covers some scope commands. NativeUI already provides portable routing; add no Router dependency or system accelerator.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
using CommandCallback=std::function<EventResult(Command)>;
template<class Child> CommandScope(CommandCallback callback, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
auto scope = ui::CommandScope{
    [](ui::Command){return ui::EventResult::Ignored;},
    ui::Button{"Action", []{}}};
```

The callback is owned and returns Handled/Ignored. Preserve the public CommandScopeComponent and `input(const InputEvent&, InputContext&)`.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The instance holds the callback and child Spec without an additional binding. The application retains any captured model for the required lifetime. No mutable process-global shortcut map; a scope does not change another UI’s commands.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

- InputType::Command only; Command::None is ignored.
- Without a callback: Ignored.
- Callback Handled stops propagation; Ignored continues it according to Tree.
- Ordinary pointer/key input passes to descendants.
- No focus or capture of its own; confirmation/cancellation are commands only if existing routing provides them.
- An inner scope can handle the command before the outer scope.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Minimum/preferred size comes from the first child, with identical bounds; the wrapper creates neither padding nor a focus target. Its routing does not depend on its painted area; descendants retain their coordinates.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

No painting or invalidation for a callback that ignores a command. If a command changes State, the affected widget invalidates its state. No implicit global “command executed” notification.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Role `None`. Accessible actions go to the control that advertises them; CommandScope does not replace them with key events. An application command does not automatically receive an additional semantic action.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

A callback may request scope removal through reconciliation without access to the moved callback after invocation. An exception must restore the dispatch stack before propagating/being converted at the appropriate boundary, without retry. Disconnect deferred tasks that capture the owner.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on the Command enum, EventResult and Tree bubbling. Cases: an empty callback, None, nested scopes, child/scope removal during a command, a throwing callback and a request to close the UI. Do not retain InputContext or activate a global owner.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/command_scope.hpp` and `src/command_scope.cpp`.

command_scope.hpp/cpp retain public declarations and the non-template portable handler; command.hpp re-exports them. No change to CommandCallback, types or EventResult return ownership.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `command_scope_none`: ignored without invocation.
- `command_scope_non_command`: pointer/key passthrough.
- `command_scope_bubble`: inner Ignored reaches outer; Handled stops propagation.
- `command_scope_owned_callback`: the closure lives long enough.
- `command_scope_reentrant_remove`: a safe removal request.
- `command_scope_throw`: the next command is dispatchable; invocation is not replayed.

Create the future public example `examples/features/command_scope.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
