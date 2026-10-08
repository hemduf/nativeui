# Badge

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Short text or a visual counter in a pill, usable beside a navigation item. Read-only presentation; no implicit action or filter.

Absent from NativeUI. [Label](label.md), TextService, and rounded primitives exist; current simple composition can draw one without a standalone builder.

MyGo: `ui/sidebar.go`, `Badge`, a small pill of SingleLine text with a text-colored background at alpha 0.1. The target makes size/colors explicit and can observe text already formatted by the application.

## 2. Public API and composition

Proposed target API:

```cpp
class Badge {
public:
    explicit Badge(std::string text);
    explicit Badge(Binding<std::string> text);
    explicit Badge(State<std::string>& text);
    Badge&& style(BadgeStyle value) &&;
    Spec spec() &&;
};
```

New BadgeStyle: TextStyle text, Color background, double horizontal_padding=6, vertical_padding=1, minimum_width=0. Radius=half-height, with no click option. No implicit `max_count`/99+ capping.

Proposed target example:

```cpp
ui::State<std::string> count{"3"};
auto badge = ui::Badge{count}.spec();
```

## 3. State, ownership, and notifications

The string constructor owns a snapshot; Binding overload observes the effective string, and State converts immediately. Display content as supplied rather than parsing it as a number.

Source destroyed: Binding invalid, last string readable, observe inactive/set never used, and no automatic notification. Destroyed data is never reinterpreted as “zero”.

Text update affects layout/paint/semantics; no onchange callback. Subscription handles stay local to each badge; no global current badge.

## 4. Interactions

Badge is neither focusable nor an interactive target; pointer, wheel, dragging, keyboard, text, and drops are Ignored.

In Sidebar/Button, activation comes from the parent item. No automatic tooltip, dismiss, or clear on click.

Confirmation and cancellation do not apply; Disabled does not remove the counter value, and ReadOnly has no mutation to prevent.

## 5. Measurement and layout

Single-line TextService measurement plus padding, optional minimum width; parent supplies constraints and clipping. Long text clips without added ellipsis or wrapping in v1.

Empty text retains the padding pill in the builder as explicit decoration. To remove space when empty, the application composes Visibility/If; no hidden collapse.

Tiny space bounds the radius to half the actual height and prevents painting outside bounds. Logical units; external margin belongs to parent layout.

## 6. Presentation and invalidation

New BadgeStyle: default text size 12/inherited family, foreground Theme.text, background=Theme.text at alpha 0.1. No Theme badge slot is assumed to exist.

Color/availability changes affect paint; typography/padding/text affect layout. Disabled applies palette.disabled to text, with unchanged background; no press/hover style.

No animation, blinking, or “urgent” announcement based on color. Transparent color preserves measurement.

## 7. Accessibility

Target contract: Text with a string value, or None if composition already incorporates the value into the parent's name/description. Do not silently add an absent Role::Badge.

Do not read the value twice when the parent item already supplies “Inbox, 3 unread”; exclusion belongs to the container.

Native T068 bridges are deferred. No IME/action; backend-neutral updates do not prove live native announcements.

## 8. Lifecycle and recovery

UI/main thread; owned string snapshot and RAII observation for the dynamic overload. Stale invalidators are no-ops after removal.

Failed text allocation/measurement does not publish partial style/text. Recovery after an exception uses the last valid snapshot without forcing application notification.

Destruction is no-throw and invokes no callbacks; no timer. An observer changing the same source and throwing does not leave a badge guard blocked.

## 9. Dependencies and edge cases

Depends on TextService/Painter/Binding/Theme rather than Sidebar or a message model. A sidebar can consume Badge without importing a counter service.

Invalid UTF-8 is repaired like Label; negative numbers, 99+, and non-numeric text are valid values. Capping/classification happen upstream.

Non-finite/negative padding/minimum_width cause invalid_argument before publication. Long/empty text and emoji graphemes remain layout cases, without resources loaded in paint.

## 10. Files and compatibility

Target: `include/nativeui/badge.hpp` and `src/badge.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: existing TextService/Label primitives. BadgeStyle in the parent header, subscription/layout/pill in badge.cpp. No variadic template or numeric specialization by counter type.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `badge_static_dynamic`: string/Binding/State constructors display the same value.
- `badge_text_layout`: empty, 99+, long, and Unicode text have intended padding/clipping.
- `badge_source_lifetime`: a destroyed source leaves the last safe string without callback.
- `badge_parent_input`: Badge in Button leaves activation to the parent and has no focus.
- `badge_style_isolation`: two badges and separate Themes share neither data nor styles.

Create `examples/features/badge.cpp` and target `nativeui_example_badge`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.