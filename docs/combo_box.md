# ComboBox<T>

**Status: existing — extraction required.**

[Component catalog](widgets.md)

Sources reviewed: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Select one exclusive value from a list of options. The control does not allow typing or filtering; EditableComboBox is dedicated to that function.

NativeUI: [combo_popup.hpp](../include/nativeui/combo_popup.hpp), ComboBoxOption<T>, ComboBox<T>, and templated anchor/popup cores. Selection, provider, and session snapshot already exist.

MyGo: `ui/widgets.go`, `Select`; `ui/base.go`, `SelectBase[T]`, `SelectParts`. This correspondence with Select avoids claiming that MyGo's editable Combobox is already ported.

## 2. Public API and composition

Current API to preserve; the following declarations are in `namespace ui`.

```cpp
template<class T>
struct ComboBoxOption { T value; std::string label; bool enabled{true}; };
template<class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class ComboBox {
public:
  using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>() >;
  ComboBox(Binding<T> selection, std::vector<ComboBoxOption<T>> options);
  ComboBox(State<T>& selection, std::vector<ComboBoxOption<T>> options);
  ComboBox(Binding<T> selection, OptionsProvider provider);
  ComboBox(State<T>& selection, OptionsProvider provider);
  ComboBox&& placeholder(std::string value) &&;
  ComboBox&& style(ComboBoxStyle value) &&;
  ComboBox&& item_style(MenuItemStyle value) &&;
  Spec spec() &&;
};
```

Example using the current API:

```cpp
ui::State<int> quality{1};
auto quality_box = ui::ComboBox<int>(quality,
    std::vector<ui::ComboBoxOption<int>>{{1, "Normal", true}, {2, "High", true}})
    .placeholder("No quality").spec();
```

Preserve constraints, overloads, and style types exactly. The current provider is called once by the constructor for initial_options, then on opening; never during paint.

Typed T adapters remain in the header; the future .cpp receives opaque indices and required copy/get/set/equals/observe closures without imposing a variant of allowed types.

## 3. State, ownership, and notifications

Binding<T> owns selection. Button options and the popup snapshot are owned. The displayed label is found in display_options; an unknown value displays placeholder without writing.

The popup initially highlights the enabled selection if present, otherwise the first enabled option. Moving highlight does not publish selection; commit only on choice.

Opening refreshes the provider snapshot and already updates display_options before the overlay command, according to the source; cancellation does not restore old labels or change selection.

State<T>& overloads are converted to Binding and retain no raw borrow. After State destruction, Binding::valid() becomes false, get() retains the last readable value, set() is ignored, and observe() remains inactive. No implicit destruction notification: check valid at each dispatch/checkpoint to stop mutations and user callbacks for the vanished model. Binding does not extend the lifetime of application models captured by a closure.

External observation invalidates presentation without simulating a user gesture. Synchronous State notifications: stable snapshot, additions on the next pass, removals skipped, and recursive writes coalesced. After an exception, the published value remains, the rest of that notification pass is interrupted, and dispatch must remain reusable.

## 4. Interactions

Click on release, first Enter press, Space release, or Down opens. The opening key remains suppressed until KeyUp to avoid accidental choice.

Popup: Up/Down cycle through enabled options, Home/End choose first/last; Enter/Space confirm. Clicking an enabled item selects on release; ComboBoxOption has no separator.

Escape, Tab, and outside click close; focus returns to the anchor or follows focus runtime according to the reason. ReadOnly closes/blocks the popup and prevents selection; Disabled removes interaction.

The button wheel does not change selection. No implicit typeahead or TextInput editing: these behaviors require EditableComboBox.

## 5. Measurement and layout

The button measures current label plus twice horizontal_padding, minimum_width, and control_height from ComboBoxStyle; no global maximum width computed by provider in paint.

The popup measures snapshot options in logical coordinates; the retained overlay service (OverlaySpec/OverlayHandle, detail::OverlayService) anchors it below the button and bounds the viewport. Very long lists require a scrollable panel in a future enhancement.

Preserve current geometry and component count during extraction; do not claim virtualization is delivered. Selection changes are observed and invalidate layout/paint because label width may change.

## 6. Presentation and invalidation

ComboBoxStyle for the anchor, MenuItemStyle for rows. Local highlight and displayed selection are distinct; an external choice does not simulate a click.

Row style changing row_height requires panel remeasurement; color-only hover requires paint. Disabled options have distinct appearance and action availability.

A null provider produces an empty list. Fonts and characters follow TextService with common UTF-8 repair.

## 7. Accessibility

Target: ComboBox, accessible name/label supplied by composition, text_value=current label, expanded state; named popup and selectable items.

The current file has no semantics override in ComboBox cores. Session IDs are backend-neutral; T options must not leak to a native bridge as application pointers.

SemanticInfo and SemanticAction are the backend-neutral interfaces already available. The role specified here is a target contract: its presence in the enum does not prove that the current component publishes it.

Native bridges remain deferred (T068). No VoiceOver, UIA, or AT-SPI results are claimed; verify the headless snapshot independently of the future bridge.

## 8. Lifecycle and recovery

Prepare snapshot/session before publishing the overlay command; provider/construction failure must leave the anchor closed and reusable. Restore suppress_until_key_up if no handle was validated.

Logically close the popup, then copy Binding/value before set. If an observer removes the anchor, check the token and do not access this after publication.

Unmounting disables runtime and subscriptions, removes pending commands, and closes the overlay. A late command becomes a safe no-op; queue rejection does not force synchronous opening.

All state and routing remain confined to the UI/main thread. Subscriptions and captures are released per instance; no global mutable registry carries interactions.

Callbacks are owned and copied before invocation. Restore captures, flags, and identity before publishing a value or calling the application. A callback that has started and throws is never replayed; direct C++ exceptions may propagate after invariants are restored.

Subtree removal follows safe reconciliation. Destruction of the UI/window owner from a callback must go through a deferred safe point; synchronous owner destruction is not guaranteed to be safe.

Destruction and unmounting are no-throw. Deferred invalidators carry a weak owner token and a monotonic identity; after removal they become inert without retaining a Node or borrowed context.

## 9. Dependencies and edge cases

Dependencies: State, OverlayCommandSource, OverlayAnchorPolicy, styles, and ThemeBinding; [popup_menu](popup_menu.md) shares non-template panel infrastructure.

Empty/all-disabled list: open a panel without selectable items according to current behavior; Escape/Tab close it. Binding is never replaced by the first option.

The source does not reject duplicate T values: preserve this during extraction, displaying the first matching label; do not silently introduce new validation.

Removing an option after opening does not change the session snapshot. An external action removing the anchor closes the session; an index identity in a new snapshot must never receive the old choice.

## 10. Files and compatibility

Target: `include/nativeui/combo_box.hpp` and `src/combo_box.cpp`. The header exposes public declarations and only the necessary template adapters; the .cpp must contain a real retained core, interactions, measurement, and rendering, and must never be an empty file.

Preserve combo_popup.hpp as the historical facade. ComboBoxOption<T>/ComboBox<T> and template signatures remain in combo_box.hpp; type-erase only the retained/popup core into combo_box.cpp.

No explicit int/string instantiation list: typed closures cover user types. The owning core handles synchronization of display_options and the session.

Register `src/combo_box.cpp` in NativeUI::Core during implementation. Preserve historical aggregate includes as compatible entry points; no Pugl, Skia, OS, or plugin types in the public API.

This page specifies future work; extraction, C++ additions, and CMake changes are not performed in this documentation batch.

## 11. Tests and acceptance criteria

Tests required during implementation; this documentation reports no execution results.

`combo_box_unknown`: an unknown value displays placeholder without set at mount or paint.

`combo_box_provider`: initial and opening calls verified; no execution from paint.

`combo_box_choice_cancel`: navigation writes nothing; confirmation writes once; Escape/outside click write nothing.

`combo_box_disabled_empty`: disabled options skipped and empty panel closable without invented selection.

`combo_box_typed_core`: user T with copy/equals compiles into the .cpp core; historical signatures preserved.

`combo_box_failure_remove`: provider/enqueue/observer throw or remove the anchor; session and key suppression recover.

Add `examples/features/combo_box.cpp`, compilable by a public consumer, with a `--self-test` mode that verifies the transitions above without an interactive window.

Acceptance: behavioral and recovery cases pass, headless rendering is compared against stable geometry, two instances are independent, historical includes compile, and new sources are warning-free.