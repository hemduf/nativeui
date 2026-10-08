# ToggleGroup

**Status: new — implementation required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Group independent actions into joined segments: bold/italic/underline. The group is a Tab stop and a visual unit, without exclusive selection state.

NativeUI provides Row, RadioGroup, and focus services but no such container. Read the contracts in [widgets.hpp](../include/nativeui/widgets.hpp) and [focus.hpp](../include/nativeui/focus.hpp).

MyGo: `ui/toggle.go`, `ToggleGroup`, `segmentTrack`. MyGo's temporary style context is replaced by a per-instance retained scope.

## 2. Public API and composition

Proposed target API, not implemented; the following declarations are in `namespace ui`.

```cpp
class ToggleGroup {
public:
  ToggleGroup(std::string label, std::vector<Spec> controls);
  ToggleGroup&& style(ToggleGroupStyle value) &&;
  Spec spec() &&;
};
```

Example using the proposed target API:

```cpp
ui::State<bool> bold{false};
ui::State<bool> italic{false};
std::vector<ui::Spec> items;
items.push_back(ui::ToggleButton("B", bold).spec());
items.push_back(ui::ToggleButton("I", italic).spec());
auto group = ui::ToggleGroup("Style", std::move(items)).spec();
```

Target ToggleGroupStyle: track padding, gap, radius, fill/border, and segmented patches for Button/ToggleButton. Style propagation uses a local scope; it requires no new global Theme slot.

controls are owned Specs: Button and ToggleButton are the primary controls; non-interactive content is allowed as decoration, and nested controls keep their own focus management.

## 3. State, ownership, and notifications

The group has no selection Binding; each ToggleButton holds its bool. The group keeps only the identity of the last active focusable child for roving Tab.

Specs and label are owned. The controls vector is immutable in a mounted generation; replacing Spec creates a new generation and cancels old actions/focus. Retained identities serve that generation's children; do not identify the active control by its “B” label.

Removing the active control moves the roving target to the next available control, then the previous; an empty group requests no focus and publishes no value.

## 4. Interactions

Tab enters at the last available active control, or the first on initial access; a single stop. Shift+Tab exits according to the runtime.

Left/Right and Up/Down move to the next/previous focusable control, wrapping; Home/End choose first/last. Skip disabled and invisible controls.

Arrows do not toggle bools; Space/Enter and clicks are delegated to the control. The group does not consume the wheel or capture an external drag.

An embedded TextInput retains its editing keys: intercept only group-navigation commands not handled by the child. Propagate ReadOnly to value controls.

## 5. Measurement and layout

Horizontal Row, with segments stretched to a common height; intrinsic width is the sum of children, gaps, and track padding. Under narrow constraints, clip or let the parent scroll without silently hiding a control.

The group creates no overflow menu: that policy belongs to Toolbar. Dimensions remain logical; segment widths may differ according to their labels.

Track style changes affecting padding/gap/typography require layout; a child's selection change remeasures the group only if its metrics change.

## 6. Presentation and invalidation

Subtle track with a shared outer border; visually joined segments and local selected relief. Style scope does not cross another ToggleGroup's boundary.

Visible focus on the active control, rather than a second ambiguous ring around the whole group. A disabled child does not automatically disable its neighbors.

Child invalidation propagates through retained services; do not rebuild every segment for a changed bool.

## 7. Accessibility

Target: Group named by label; each Button/ToggleButton publishes its action and value. No RadioGroup role or unique selected state, because multiple pressed values are allowed.

Snapshot child order matches visual order. Decorative elements do not become stops or actions.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Register and remove focus participants with RAII; disappearance of an armed segment cancels its capture before updating roving focus.

A child action may remove the group; the group must not request focus after a callback without rechecking owner identity. Child mounting failure does not publish a partial list.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: [row](row.md), [focus_scope](focus_scope.md), [style_scope](style_scope.md), [button](button.md), [toggle_button](toggle_button.md). Reuse shared focus; no parallel service.

All children disabled: group is not navigable; enabling restores the first target. Two groups with identical labels keep focus and styles isolated.

Do not add exclusivity or an aggregate Changed callback: independent bools and their observers suffice.

## 10. Files and compatibility

Target: `include/nativeui/toggle_group.hpp` and `src/toggle_group.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Any variadic construction adapters remain in the header; the .cpp contains horizontal arrangement, visual scope, and the focus-participation contract.

Register `src/toggle_group.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`toggle_group_roving`: one Tab stop and wrapping arrows/Home/End; skip disabled controls.

`toggle_group_values`: navigation changes no bool; activating one segment leaves the others untouched.

`toggle_group_remove_focus`: removing the current segment chooses a valid target and cancels an armed gesture.

`toggle_group_style_scope`: segmented presentation remains local; an external button keeps its style.

`toggle_group_empty_resize`: empty/all-disabled group and reduced width keep focus/clipping consistent.

`toggle_group_child_throw`: mount/callback failure restores focus/scope to a recoverable state.

Add `examples/features/toggle_group.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.