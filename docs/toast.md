# Toast

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Controller for temporary messages and short actions, stacked at the bottom of the viewport. It opens neither a window nor a modal dialog and does not steal focus.

Absent from NativeUI. [Overlay](../include/nativeui/overlay.hpp) and [Dispatcher](../include/nativeui/dispatcher.hpp) are available, but no shipped native announcement service should be assumed.

MyGo: `ui/toast.go`, Context.Toast, Context.ToastAction, toast.life and buildToasts. Duration 4 s for a simple message/8 s with an action, deduplication by message, hover pause and announcement without focus. The target retains these uses with explicitly isolated IDs/handles.

## 2. Public API and composition

Proposed target API:

```cpp
struct ToastSpec {
    std::string message;
    std::string action_label;
    std::function<void()> action;
    std::optional<std::chrono::milliseconds> duration;
};
enum class ToastShowStatus { Shown, InvalidSpec, Unavailable };
struct ToastShowResult { ToastShowStatus status; ToastHandle handle; };
class Toast {
public:
    Toast(UI& ui, Dispatcher dispatcher);
    Toast(const Toast&) = delete;
    Toast& operator=(const Toast&) = delete;
    ~Toast() noexcept;
    ToastShowResult show(ToastSpec value);
    bool dismiss(ToastHandle handle);
};
```

Toast is also nonmovable. ToastHandle is opaque, with a weak owner and monotonic ID, valid() and bool comparability; it does not prolong the UI/controller lifetime. Default duration is 4 s, or 8 s with an action; an explicit duration must be strictly positive. An action without a label or a label without an action = InvalidSpec.

Proposed target example: `ui::Toast notices{uiInstance, dispatcher}; auto shown = notices.show({.message = "Document saved"}); (void)shown;`. The controller owns its messages while alive; destruction abandons them cleanly. Panels produce internal Spec objects without a false public spec() builder.

## 3. State, ownership and notifications

The controller owns messages/actions, handles, remaining time, timers and its stack overlay. UI/Dispatcher are borrowed through weak tokens; all mutable state is local.

Same message in this controller: replace the old entry and create a new ID/duration; the old handle becomes stale. Different messages stack, newest at the bottom. No global deduplication across UIs/controllers.

An action executes once after logical removal of its toast; dismiss/expiration are silent. External code does not rewrite State; explicit show is the only data entry point.

## 4. Interactions

A simple panel does not take focus; an action Button is reachable through Tab and activated through Enter/Space/pointer input. A toast's presence does not change current focus.

Pause the remaining duration during hover or focus within the toast (a target addition for keyboard access). Resume when the last trigger leaves; do not reset the remaining duration.

No dragging/wheel interaction or modal barrier; clicks outside the stack pass through to underlying controls. Escape does not arbitrarily erase all messages; dismiss is programmatic in v1.

## 5. Measurement and layout

Stack width is at most viewport−48 DIP, horizontally centered at the bottom with a 24 DIP margin and gap 8. The message wraps; the action button retains its width. A tiny viewport clips without overlap beyond bounds.

Required additive service extension: append `OverlayPlacement::ViewportBottomCenter` to the enum. Without an anchor, it positions the stack's natural bounds at the bottom of the viewport; existing placements without an anchor remain Center.

Natural stack content carries the 24-unit bottom and horizontal insets; OverlayEntry covers only these bounds, rather than an invisible full-window surface that intercepts every input. The single service reevaluates position on resize.

## 6. Presentation and invalidation

Private v1 style from palette/spacing; a readable surface, contrasting text and accented action. No public ToastStyle option without a planned configurator; visual rules are fixed here.

One-shot timer for each expiration and pause callbacks through Dispatcher; deadlines/remaining time use exactly its injected current_time, accessible to the controller through private Dispatcher→Toast friendship, without a second public clock or direct steady_clock::now. No continuous polling/per-frame loop. No required v1 fade; duration has no reduced-motion dependency.

Insertion/removal = structure/semantics; hover/focus = deadline/presentation, without rebuilding the stack or reannouncing the message. Multiple show calls do not create additional threads.

## 7. Accessibility

Target contract: Group with Text(message) and Button(action_label) when an action exists. SemanticRole::Status and native announcements are absent from the source API: do not claim shipped Announce or screen-reader support.

Prepare a semantic content change for each new message; a native live announcement is an explicit T068 dependency. Focus pause helps keyboard access even without a bridge.

No forced focus, IME or textbox. Semantically deduplicate an already displayed message after repaint; a new generation receives one structural publication.

## 8. Lifecycle and recovery

UI/main thread; timers and overlay are protected by owner/generation and canceled on dismiss/replacement/unmount. The destructor removes its entries/overlay and remains no-throw without invoking actions.

show prepares allocation/expiration before publication; if the scheduler rejects or throws, leave no immortal toast: exact rollback and Unavailable, or propagation after restoring invariants. No synchronous fallback for action/expiration.

Move the action snapshot out of the entry, then make logical removal terminal before user code; a started callback is never replayed after an exception. An action may show a new message without stale erasure; top-level destruction is deferred to the safe checkpoint.

## 9. Dependencies and edge cases

Depends on Overlay/Dispatcher/Button/TextService. Reuse existing OverlayState and invalidation; no second “notifications” stack independent of routing/focus.

Empty message = InvalidSpec. Zero/negative duration = InvalidSpec. Invalid Dispatcher/closing UI owner = Unavailable. Handles from another controller, stale or already dismissed = false.

Disabled/hidden view: remove the visual stack and timers, silently abandoning this controller's messages; reopening resurrects no old message/actions. Excessive panels are bounded/clipped, with no implicit unbounded queue: v1 capacity is 32, and the 33rd is rejected as Unavailable without evicting an accepted action.

## 10. Files and compatibility

Target: `include/nativeui/toast.hpp` and `src/toast.cpp`. The header contains public declarations; the `.cpp` contains a real retained kernel, measurement, layout, applicable events and rendering.

Source to extract or reuse: existing Overlay/Dispatcher; no source Toast implementation. ToastSpec/ToastHandle/show statuses stay in toast.hpp; the actual controller/stack/expiry belong in toast.cpp. The additive placement extension stays in the existing Overlay service and is tested without changing its other policies.

Essential template adapters stay in the header and delegate to the non-template kernel. Preserve historical includes through their collective headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. This API exposes no Pugl, Skia, OS or plugin SDK types.

This delivery is documentation: no extraction or CMake change is performed in this documentation phase.

## 11. Tests and acceptance criteria

Tests to implement with the component:

- `toast_durations`: 4 s/8 s and explicit durations expire under a manual clock.
- `toast_pause_focus`: hover/focus freeze time and resume with the exact remaining duration.
- `toast_dedup_stale`: the same message replaces the generation; the old handle cannot affect the new one.
- `toast_action_once`: click/keyboard followed by a reentrant or throwing callback does not replay the action.
- `toast_bottom_hit`: resize/bottom-center placement and input outside the stack remain correct.
- `toast_timer_reject`: full queue/exception before enqueue leaves no immortal toast.
- `toast_isolation_capacity`: two UIs, controllers, capacity 32 and teardown are isolated.

Create `examples/features/toast.cpp` and the `nativeui_example_toast` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, works headlessly and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering and coexistence of two independent UIs. Cover recovery from the faults above under ASan/UBSan when lifetimes are involved.

Acceptance: all named tests pass, no capture/registration survives unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive test was executed for this specification.
