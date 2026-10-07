# SegmentedControl<T>

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Choose one exclusive value among segments: List/Grid mode or a view range. The group retains a stable application selection rather than an index imposed at mounting.

NativeUI offers RadioGroup<T>/RadioButton<T> in [widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc) but no visual SegmentedControl.

MyGo: `ui/toggle.go`, `SegmentedBase`, `Segment`, `Segmented`. MyGo clamps its index at construction; the NativeUI target uses T values and never rewrites external state merely to render.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
template<class T>
struct SegmentOption {
  T value;
  std::string label;
  bool enabled{true};
};
template<class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class SegmentedControl {
public:
  SegmentedControl(std::string label, Binding<T> selected,
                   std::vector<SegmentOption<T>> options);
  SegmentedControl(std::string label, State<T>& selected,
                   std::vector<SegmentOption<T>> options);
  SegmentedControl&& style(SegmentedControlStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<int> mode{0};
auto mode_picker = ui::SegmentedControl<int>(
    "View", mode, {{0, "List", true}, {1, "Grid", true}}).spec();
```

Target SegmentedControlStyle: track padding/gap/radius/border, segment ButtonStyle, and a selected patch. T is not limited to int/string; typed value/equals/set/observe adapters are type-erased for the non-template core.

Icon variants remain options of the same component; they may be added through a per-option content factory while preserving label as the name, without a standalone public Segment component.

## 3. State, ownership, and notifications

Binding<T> holds selection; T options are owned and compared for equality. A value absent from the options is not corrected at mounting: no segment is selected, but the first available segment may receive focus.

The options vector is an immutable generation snapshot; replacing options requires a new Spec at the checkpoint, with no in-place options setter in this v1. A disabled selected value remains visibly selected but cannot be chosen by the user. External selection changes move the Tab entry target without stealing focus already in another control.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

A click released on an enabled segment selects its value; selecting the current value does not republish. PointerCancel cancels the action.

Tab is one stop on the selected available segment, otherwise the last active/first available one. Left/Right or Up/Down and Home/End move focus and select simultaneously; arrows wrap and skip disabled segments.

Space selects on release and Enter on the first press; activation repeats are suppressed. ReadOnly permits reading/focus but arrows publish no selection.

Wheel is ignored; dragging outside the segment cancels without turning the control into a slider. Reduced width does not change the choice.

## 5. Measurement and layout

Horizontal track; natural width is the sum of segments and paddings. Common height equals the largest child measurement; keep variable widths by label instead of imposing uniformly truncated text.

Narrow space clips visually according to the parent; each hit test uses the actually arranged rectangle, with no selection of an invisible segment. No implicit overflow.

Selection preserves size for identical styles; a selected patch with different metrics explicitly invalidates layout.

## 6. Presentation and invalidation

Selected has a distinct face within the track; momentary pressed and persistent selected are separate states. Focus remains visible on the active segment.

Styles come from the theme and component style; no mutation of neighboring widgets' ButtonStyle. SegmentOption.label and icon use the same resolved colors.

Selection changes invalidate the two affected faces rather than the whole collection when metrics are unchanged.

## 7. Accessibility

Target: named Group, with RadioButton children exposing selected/checked and Select when mutable. Current SemanticRole does not contain RadioGroup: do not claim this role is delivered.

Exactly one selected segment by equality; a disabled option remains exposed with enabled=false. An unknown value leaves all children unselected.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

The non-template core owns retained segments and adapters by value; no T reference to a temporary row.

Commit selection after ending capture and establishing consistent focus state; an observer removing the group or throwing must not cause a second set or subsequent access to a destroyed child.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [radio_button](radio_button.md), [toggle_group](toggle_group.md) for presentation, shared focus, State/ThemeBinding.

Duplicate option values are forbidden: invalid_argument before Spec publication to avoid ambiguous choice identity. Duplicate labels are allowed when values differ.

Empty/all-disabled options: no user choice; preserve the external value. After replacing options, a removed value remains in Binding and loses only its selected face.

## 10. Files and compatibility

Target: `include/nativeui/segmented_control.hpp` and `src/segmented_control.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

SegmentOption<T>, State/Binding constructors, and any deduction guides remain in the header; selected/observe/select are template adapters. The .cpp contains the non-template segmented state machine.

Existing RadioGroup<T> remains compatible; SegmentedControl renames neither Toggle nor Switch<T>.

Register `src/segmented_control.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`segmented_control_selection`: clicks and navigation select one value; the same choice causes no notification.

`segmented_control_unknown_disabled`: unknown or disabled state is not rewritten by mount/paint.

`segmented_control_empty_duplicates`: empty/all-disabled options cause no mutating focus; duplicate values are rejected.

`segmented_control_remove_option`: an option removed during a press receives no stale commit.

`segmented_control_generic_type`: a copyable/equality-comparable user type compiles with the .cpp core without listed instantiations.

`segmented_control_observer_throw`: observer removal/reentrancy and exceptions leave the next choice possible.

Add `examples/features/segmented_control.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.