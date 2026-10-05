# Popover

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Decorator for an anchor with a composed panel, controlled by an opening Binding<bool>. Nonmodal by default, with explicit focus management; serves DateInput/ColorWell/other short panels.

No public Popover in NativeUI. [OverlaySpec](../include/nativeui/overlay.hpp) and retained overlay services provide the NodeId anchor, Auto/flip/clamp, Escape/outside handling and lifetime; they are the only stack.

MyGo: `ui/widgets.go`, `Popover` and `stylePanel`; `ui/base.go`, `PopoverBase`. Panel below the anchor, at least as wide as the anchor, closed by outside clicks/Escape. MyGo subcomponents use PopoverBase when independent width is desired.

## 2. Public API and composition

Proposed target API:

```cpp
class Popover {
public:
    template<class Anchor, class Content>
    Popover(Binding<bool> open, Anchor&& anchor, Content&& content);
    template<class Anchor, class Content>
    Popover(State<bool>& open, Anchor&& anchor, Content&& content);
    Popover&& placement(OverlayPlacement value) &&;
    Popover&& match_anchor_width(bool value = true) &&;
    Popover&& focus_on_open(bool value = true) &&;
    Popover&& on_close(std::function<void()> callback) &&;
    Popover&& style(PopoverStyle value) &&;
    Spec spec() &&;
};
```

Defaults: Auto placement, match_anchor_width=true, focus_on_open=false, Escape/outside closure enabled. PopoverStyle describes surface/border/radius/padding/max_size; templates convert children into Spec immediately, with a non-template kernel.

Proposed target example:

```cpp
ui::State<bool> open{false};
auto view = ui::Popover{open,
    ui::Button{"Options", [&] { open.set(!open.get()); }},
    ui::Label{"Advanced settings"}}
    .focus_on_open().spec();
```

## 3. State, ownership and notifications

External Binding<bool> owns the opening intent; the State overload is converted immediately. After source destruction: valid=false and the last value remains readable, set is ignored/observe inactive without automatic notification; revalidate before access/entry and close silently at that checkpoint. The component owns the anchor/content Spec and an overlay handle. Store no pointer to the Go Element or retained node; only a NodeId with a safe lifetime.

Successful publication follows open=true; user closure writes false then calls on_close exactly once. An external false write closes without a user on_close. If show is unavailable/fails, restore open=false without a callback and allow the next opening.

True received while the anchor is Hidden/Disabled/unmounted is rejected and reset to false. Content has one instance per open period; each reopening reconstructs local state without duplicating the anchor.

## 4. Interactions

No trigger is invented: Anchor chooses its own events. The example Button toggles open. The decorator does not intercept another anchor click or duplicate the action.

Escape closes the relevant topmost overlay if descendants ignored it. An outside click closes and is consumed under the current policy to prevent click-through; the panel routes its controls normally.

focus_on_open=true enters the first available descendant; otherwise focus remains on the anchor. On closure, restore previous/anchor focus only if available. Nonmodal does not trap Tab; navigation outside the panel does not automatically close it.

## 5. Measurement and layout

The decorator measures/layouts only Anchor, without reserving space for open content. The panel is measured within the viewport with style padding and bounded maximum_size.

Match anchor width gives a minimum width equal to the anchor's, bounded by the viewport; false preserves natural size. Flip/clamp/placement are entirely delegated to Overlay.

Resizing/scrolling follow the anchor; removal/Hidden/Collapsed/Disabled closes the panel without a centered fallback. Oversized content must explicitly compose ScrollView within its bounds.

## 6. Presentation and invalidation

New PopoverStyle, with existing palette/spacing defaults. No modal backdrop or native OS shadow; portable Painter chrome.

open/close = Overlay structural invalidation; content Binding follows its own invalidators; style/anchor metrics = layout. Do not remount the anchor when repainting the popup.

No required animation. Trigger states do not alter the surface without an explicit option. Transparent color preserves the panel hit area.

## 7. Accessibility

Target contract: the anchor retains its role and receives expanded/open; the panel is a Group named from its content or trigger. No Role::Popover is claimed to exist.

Focus and actions remain with descendants. The tree exposes one open content instance; panel structure is not copied into the anchor description.

T068 bridges are deferred. Any input in the content retains the committed/preedit limitation under DESIGN17.4; Popover adds no IME services.

## 8. Lifecycle and recovery

UI/main thread; RAII subscriptions and a weak handle to a specific generation. Unmounting first invalidates callbacks and closes its overlay without an application callback.

Prepare Spec/chrome before publication. Failed closure/invalidation retains the handle for an exact retry; a reentrant open=true subscription during close creates a new generation only after the old one closes.

Take the on_close snapshot after the terminal transition; an exception does not replay it. Anchor removal during input uses the retained checkpoint; top-level destruction is deferred. No new Tree/Focus/Overlay registry.

## 9. Dependencies and edge cases

Depends on Overlay/focus services and Binding, not DialogState. The bool model does not become a process-wide state machine.

Anchor or Content Spec without a factory = invalid_argument before mounting. Invalid open Binding: the last value remains readable, but set is ignored and observe inactive, without automatic destruction notification. On the next access, close the panel without a user on_close and do not claim to have written false.

Cover reopening, rapid external toggling, anchor removal and nested overlays. Tooltip remains a separate pointer-transparent presentation; it is not converted into an interactive Popover.

## 10. Files and compatibility

Target: `include/nativeui/popover.hpp` and `src/popover.cpp`. The header contains public declarations; the `.cpp` contains a real retained kernel, measurement, layout, applicable events and rendering.

Source to extract or reuse: existing OverlaySpec/OverlayHandle and overlay service. The only templates are child conversion and the State overload; behavior/decorator/chrome/closure belong in popover.cpp. Do not add a second popup manager.

Essential template adapters stay in the header and delegate to the non-template kernel. Preserve historical includes through their collective headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, without an empty file or central widget switch. This API exposes no Pugl, Skia, OS or plugin SDK types.

This delivery is documentation: no extraction or CMake change is performed in this documentation phase.

## 11. Tests and acceptance criteria

Tests to implement with the component:

- `popover_anchor_retained`: opening/closing does not remount the anchor.
- `popover_outside_escape`: closure writes false and emits one user notification.
- `popover_width_placement`: width matching/Auto/flip/clamp use Overlay.
- `popover_focus_policy`: optional focus and nonmodal traversal without trapping follow the specified policy.
- `popover_stale_generation`: reopening during close does not close the new panel.
- `popover_publish_fault`: allocation/invalidation that throws preserves a recovery owner or closes without a ghost overlay.

Create `examples/features/popover.cpp` and the `nativeui_example_popover` target, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a clock, works headlessly and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering and coexistence of two independent UIs. Cover recovery from the faults above under ASan/UBSan when lifetimes are involved.

Acceptance: all named tests pass, no capture/registration survives unmounting, and the published API matches these contracts. Verification performed here: reading declarations and sources; no C++ or interactive test was executed for this specification.
