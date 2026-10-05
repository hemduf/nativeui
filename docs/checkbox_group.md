# CheckboxGroup

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Compose an aggregate checkbox and independent options with derived unchecked/checked/mixed state. Usage: select all permissions in a group.

NativeUI provides Checkbox and SemanticCheckedState::Mixed in [semantics.hpp](../include/nativeui/semantics.hpp) but no aggregate group.

MyGo: `ui/feedback.go`, `CheckboxGroup`, `paintCheckbox`; it aggregates bools from boxes constructed under a parent box. Target: explicit binding collection to avoid an implicit context registry.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
struct CheckboxGroupItem {
  std::string key;
  std::string label;
  Binding<bool> checked;
  bool enabled{true};
  bool read_only{false};
};
class CheckboxGroup {
public:
  CheckboxGroup(std::string label, std::vector<CheckboxGroupItem> items);
  CheckboxGroup&& style(CheckboxGroupStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<bool> mail{true};
ui::State<bool> calendar{false};
auto group = ui::CheckboxGroup("Notifications", {
    {"mail", "Mail", mail.binding(), true, false},
    {"calendar", "Calendar", calendar.binding(), true, false}}).spec();
```

Target CheckboxGroupStyle owns CheckboxStyle for parent/children, indentation, gap, and a mixed mark. CheckboxGroupItem and the aggregate bool remain group subtypes.

Do not offer Binding<bool> for the parent: its three-state value is derived. Application notifications come from child bindings, without a fictitious second aggregate callback.

## 3. State, ownership, and notifications

Each item owns a copied Binding<bool>. The items vector is an immutable snapshot for the mounted generation, with no item setter/provider in this v1; unique/non-empty keys identify children within that generation. Changing order/label/items requires replacing Spec at a checkpoint and cancelling the previous generation; only bools remain live-observable through RAII.

Aggregate over all items: empty=Unchecked; all true=Checked; some true=Mixed; otherwise Unchecked. Disabled/read_only remain visually counted because they are actual values.

Parent activation: choose false if the aggregate is Checked, otherwise true, then write only enabled, non-read_only children with valid Binding in snapshot order. An immutable child may leave the parent Mixed; its last readable bool remains counted.

Bulk writing is not atomic across States. Each set notifies immediately; on exception, already published values remain, subsequent values are not written, and the parent recomputes its aggregate at the checkpoint. No rollback or callback retry.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

Parent and child boxes activate on pointer or Space release; Enter follows Checkbox behavior. The parent box is focusable only when at least one child can change.

Tab visits the parent and then available children; this group does not replace boxes with one roving stop. Arrows do not perform bulk operations.

Inherited ReadOnly prevents all writes but exposes the aggregate; disabled children remain visually present. PointerCancel cancels the parent request before the first set.

An external write or item removal during set is handled through a binding snapshot; check the live generation before each new write, stopping if the group has been removed.

## 5. Measurement and layout

Column: parent box then an indented child column; width is maximum row width plus indentation, height is the sum of measurements and gaps.

The mixed indicator occupies exactly the same box as checked; no layout shift during aggregation. Labels follow Checkbox and logical coordinates.

Narrow constraint: clip labels and preserve indicators; permissions never disappear automatically for lack of space.

## 6. Presentation and invalidation

Parent uses a checkmark for Checked, a horizontal stroke for Mixed, and nothing for Unchecked. The mark follows checkbox colors and local stroke width.

A bool change repaints parent and child; changes to text/order/metric style require layout. Observation never calls set to align the aggregate value.

The parent may remain Mixed after “check all” if disabled children remain false; keep presentation faithful to values rather than displaying artificial success.

## 7. Accessibility

Target: Group named by label, parent Checkbox with Mixed/Checked/Unchecked checked state, and named Checkbox children. Parent Toggle is absent when no value is mutable.

Parent availability is based on its actions rather than the aggregate; an empty group describes an optionless list and presents no active button.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Every parent activation prepares a key/binding/mutability snapshot and a single target value before the first set. Release capture before notifications; retain no Child Node.

An observer may remove a child, rebuild the group, or throw. Already written data is authoritative; recompute the aggregate from the new model without blindly finishing the old batch.

Unmounting removes all subscriptions, including those for replaced items; destruction never launches an “uncheck all” action.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [checkbox](checkbox.md), [column](column.md), State, availability, and mixed semantics. Add no global collective notification service.

Duplicate keys are rejected before publication. Duplicate labels are allowed; the same Binding in two items is allowed and visited in order, with the second identical set not notifying again according to State.

Empty/all-immutable/all-invalid collection: no writes or mutation capture. Replacing items requires a new Spec at a checkpoint; no item synchronization or preservation of transient focus between generations is promised.

## 10. Files and compatibility

Target: `include/nativeui/checkbox_group.hpp` and `src/checkbox_group.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

CheckboxGroupItem and CheckboxGroupStyle remain with the parent; child boxes reuse the Checkbox core, while checkbox_group.cpp contains aggregation, batching, and layout.

Register `src/checkbox_group.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`checkbox_group_aggregate`: empty/off/all/mixed produce exact states without any mount-time write.

`checkbox_group_mutable_only`: parent changes only enabled mutable children; persistent mixed state remains faithful.

`checkbox_group_order_failure`: a middle observer throws: initial values remain, later values stay unchanged, and the group recovers.

`checkbox_group_remove_during_bulk`: removal/reconstruction during set stops old writes.

`checkbox_group_keys`: reordering preserves identity/focus; duplicate keys rejected.

`checkbox_group_semantics`: parent Mixed and actually available actions; children not duplicated.

Add `examples/features/checkbox_group.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.