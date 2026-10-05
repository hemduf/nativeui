# Dialog

**Status: existing — enhancements required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Portable modal dialog controller with a body composed from Spec and identified actions. Add the alert variant in the same file pair, without a second AlertDialog component.

The current implementation is in [dialog.hpp](../include/nativeui/dialog.hpp): `Dialog`, `DialogSpec`, `DialogAction`, show/active/close. One Dialog generation per UI; a single [Overlay](../include/nativeui/overlay.hpp) service handles modal focus, the barrier and presentation.

MyGo: `ui/widgets.go`, `Modal`; `ui/base.go`, `DialogBase`; `ui/feedback.go`, `AlertDialog`. MyGo Modal dismisses on outside clicks; Alert blocks outside clicks and infers Cancel from text. NativeUI already blocks outside clicks; preserve its explicit Cancel role, independent of language.

## 2. Public API and composition

Exact current API: `explicit Dialog(UI&)`, `Completion=std::function<void(DialogResult)>`, `show(DialogSpec, Completion)->DialogShowResult`, `active() const noexcept`, `close()->bool`, noexcept destructor; a noncopyable, nonmovable controller. Do not invent a `spec()` builder.

Verified existing example; uiInstance is an already mounted UI:

```cpp
ui::Dialog dialog{uiInstance};
ui::DialogSpec request;
request.title = "Confirm";
request.body = ui::Label{"Apply changes?"}.spec();
request.actions = {
    {"cancel", "Cancel", true, ui::DialogActionRole::Cancel},
    {"apply", "Apply", true, ui::DialogActionRole::Default}
};
auto shown = dialog.show(std::move(request), [](ui::DialogResult) {});
(void)shown;
```

Proposed target additions: `AlertDialogSpec { std::string title; std::string message; std::vector<DialogAction> actions; }` and `DialogShowResult show_alert(AlertDialogSpec, Completion)`. This helper creates a DialogSpec and its message body; it does not infer roles from labels.

Append `std::optional<DialogStyle> style` and `std::string description` to DialogSpec, preserving the historical title/body/actions/backdrop_color fields and their defaults. DialogStyle owns width/padding/gaps/palette; no existing Theme slot is assumed.

## 3. State, ownership and notifications

Dialog borrows UI and holds weak state per generation. UI must remain alive while direct controller operations are usable; after teardown, show returns Unavailable and close returns false.

show owns body/actions/Completion; no parallel State<bool>. Preserve the current Shown/Busy/InvalidSpec/Unavailable results; completion describes Action(id) or Dismissed, exactly once upon effective closure.

Action data is an immutable snapshot for the session. An external body model through Binding is valid, but actions are not implicitly replaced during a press. close preserves the first requested result during recovery.

## 4. Interactions

Outside clicks and the backdrop consume pointer input without closing. Tab/Shift+Tab stay within the modal scope; initial focus goes to an enabled Default, otherwise the first available descendant, otherwise the panel.

Enter reaches Default only if the focused descendant ignored it; TextInput/TextArea may consume it. Escape = the enabled Cancel action, otherwise Dismissed. Disabled buttons activate neither through pointer input nor semantic actions.

Programmatic close = Dismissed. UI deactivation closes without completion under the current contract. PointerCancel cancels child gestures; no panel dragging or global scrolling, only the body's ScrollView.

## 5. Measurement and layout

Historical path: viewport margin 24, maximum width 560, padding 20, section gap 12 and action gap 8, in logical units. The panel is centered; its width/height are bounded by the viewport.

Title and actions remain outside scrolling; the body is in a vertical ScrollView owned by the panel. A tiny viewport prioritizes chrome and reduces the body to zero without negative height.

Enhanced contract: an excessively wide action row wraps into multiple lines in visual order, without changing roles/default focus traversal. No button overflows the panel; the body remains the only scrollable content.

## 6. Presentation and invalidation

The new DialogStyle provides typography/chrome/layout overrides; defaults preserve the current palette and geometry except for corrected overflow cases. Historical Backdrop_color retains its explicit priority.

The alert variant constructs title/message and actions in the same panel. Default is visually accented; Cancel/Normal keep standard chrome, without a “last button = default” rule.

Theme and body Binding invalidation follow existing services. Opening/closing use Overlay structural invalidation; no animation loop or second modal stack.

## 7. Accessibility

Target contract: Dialog named by title, with an explicit description or alert message; Group/actions/body retain their roles. SemanticRole::AlertDialog is absent: use Dialog with a description; a dedicated extension would be a separate contract.

Modal hides navigation to background descendants in the target semantic tree; visual action order must not be confused with Default's focus priority.

Native T068 bridges are deferred. Body inputs use committed text; native preedit/candidate rectangles remain deferred under DESIGN17.4, without implicit additions through Dialog.

## 8. Lifecycle and recovery

show prepares content/callbacks before acquiring a slot. Failed publication releases the generation and overlay; failed close/reconciliation preserves the handle/generation/result for an exact retry rather than an artificially Busy session.

Closure from dispatch is deferred entirely to the retained checkpoint, without synchronous fallback on enqueue failure. The slot and local state are terminal before completion; a completion that throws is never replayed.

Current behavior to preserve: the destructor's active close may invoke completion; it first invalidates retained callbacks, forces terminal cleanup and contains all exceptions. UI deactivation/teardown abandon without completion. No global state; top-level destruction only at boundaries proven safe.

## 9. Dependencies and edge cases

Depends on existing Overlay, FocusScope, ScrollView, Button, Label/TextService and DialogState. Do not replace UI::dialog_state_ or its checkpoint flow.

Body without a factory = InvalidSpec. Empty/duplicate ID, multiple Default or Cancel actions = InvalidSpec before acquiring the slot. Zero actions are allowed; Escape/close remain operational.

Empty title/message are allowed. show_alert supplies a valid body Spec even with an empty message. Two Dialog controllers in one UI share the Busy slot; two UIs remain independent. Disabled actions and destruction in completion are required cases.

## 10. Files and compatibility

Target: `include/nativeui/dialog.hpp` and `src/dialog.cpp`. The header contains public declarations; the `.cpp` contains a real retained kernel, measurement, layout, applicable events and rendering.

Source to extract or reuse: existing `dialog.hpp` and detail/dialog_state.hpp. Preserve all public declarations, structs/enums and historical dialog.hpp includes. Move the actual controller, panel/chrome and callbacks into dialog.cpp; composition helpers must not become a heavy header.

Essential template adapters stay in the header and delegate to the non-template kernel. Preserve historical includes through their collective headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. This API exposes no Pugl, Skia, OS or plugin SDK types.

This delivery is documentation: no extraction or CMake change is performed in this documentation phase.

## 11. Tests and acceptance criteria

Tests to implement with the component:

- `dialog_legacy_results`: preserve Shown/Busy/InvalidSpec/Unavailable and Action/Dismissed.
- `dialog_default_enter`: Enter consumed by a descendant does not trigger Default.
- `dialog_alert_roles`: explicit roles are independent of translated labels.
- `dialog_action_wrap`: long actions wrap in a tiny viewport without overflow.
- `dialog_close_fault`: reconciliation that throws preserves the first result and retry owner.
- `dialog_destructor_completion`: no-throw cleanup contains a throwing completion and releases the slot.
- `dialog_reentrant_show`: completion can open the next dialog without closing its generation.

Create `examples/features/dialog.cpp` and the `nativeui_example_dialog` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, works headlessly and returns a nonzero code on the first failure.

Reuse `examples/features/t063_dialog.cpp` and current Dialog transaction tests; add alerts, semantics and overflow without weakening recovery evidence.

Acceptance: all named tests pass, no capture/registration survives unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive test was executed for this specification.
