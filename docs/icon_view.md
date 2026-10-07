# IconView

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

Declarative SvgIcon display, monochrome and tinted by default, sized relative to text with preserved ratio. Distinct from SvgIcon, a prepared resource, and multicolor ImageView.

The [SvgIcon](../include/nativeui/svg.hpp) resource and draw_svg exist; [skia_svg.cpp](../src/skia_svg.cpp) renders the DOM without tinting. Neither public IconView nor a current monochrome overload exists.

MyGo: `ui/svg.go`, `Icon` and `Painter.Icon`; `ui/widgets.go`, intrinsicSize. Icon at font height, SVG ratio, TextColor tint regardless of fills, decorative unless labeled. This tint is a NativeUI rendering extension to implement.

## 2. Public API and composition

Proposed target API:

```cpp
class IconView {
public:
    explicit IconView(SvgIcon value);
    explicit IconView(Binding<SvgIcon> value);
    explicit IconView(State<SvgIcon>& value);
    IconView&& size(double height) &&;
    IconView&& color(Color value) &&;
    IconView&& monochrome(bool value = true) &&;
    IconView&& alt(std::string value) &&;
    IconView&& decorative(bool value = true) &&;
    Spec spec() &&;
};
```

Defaults: height Theme typography.control_size, color Theme.text, monochrome=true, decorative=true, empty alt. Color applies only in monochrome mode; false mode preserves SVG colors exactly.

Proposed target example:

```cpp
ui::SvgIcon saveIcon;
auto icon = ui::IconView{saveIcon}
    .size(16.0)
    .color(ui::Color{0.2f, 0.4f, 0.9f, 1.0f})
    .spec();
```

## 3. State, ownership, and notifications

Owned/copied SvgIcon handle with stable shared backing; Binding overload observed, State converted immediately. Color/size/alt are owned values.

Resource updates change intrinsic ratio and therefore layout and paint; theme color changes affect only implicit tint, never shared SVG data.

Source destroyed: Binding invalid, get retains the last handle and observe inactive without automatic notification; no set or on_change callback. A resource invalid after update is safe to measure/paint.

## 4. Interactions

IconView ignores pointer, drag, wheel, keyboard, text, and drops. No focus/capture; inside Button it diverts neither activation nor Button name.

For a clickable icon, the application composes a named Button. Informative alt does not make the image interactive.

Confirmation/cancellation do not apply. Rotation/animation are not implicitly included; Spinner is the dedicated activity component.

## 5. Measurement and layout

Specified or typographic height, width=height × intrinsic_size ratio. Invalid resource: a square of the chosen height as a layout placeholder, no pixels; no division by zero.

Final bounds use centered Contain, clipped without stretching shapes. The renderer handles DPR; public size remains logical.

size 0 means no pixels and zero measurement, with no invalid rectangle passed to the backend. Reduced parent width may shrink the contained image without changing the requested ratio.

## 6. Presentation and invalidation

Monochrome target: use the rendered SVG alpha mask and apply uniform Color, preserving original shape alpha × Color alpha. Do not modify shared DOM fills/strokes on every paint.

A private SVG adapter extension is required; current source does not provide it. Any mask/backend cache depends on backing identity, destination/resolution, and color, with explicit instance/context ownership rather than a global mutable namespace.

Theme changes reevaluate size only for implicit height, and color only for implicit tint. No IconViewStyle required for these fixed options or imaginary Theme slot.

## 7. Accessibility

Decorative: SemanticRole::None. Informative: Image with name=alt and no action; this role exists, unlike Role::Icon.

The parent Button keeps its application name; do not automatically concatenate alt and label and read “Save” twice. An informative icon without alt is explicitly unnamed.

T068 bridges are deferred. No IME or pixel/resource-driven live announcement.

## 8. Lifecycle and recovery

UI/main thread; widget/renderer subscription and cache use RAII and a weak invalidator. The original source buffer may disappear after SvgIcon.parse according to the resource contract.

Prepare mask/tint without mutating shared DOM: exceptions restore save/clip/transform and leave the next paint possible. Retain no raw SvgData* without a handle owner.

Destruction is no-throw and invokes no callbacks; clearing one UI's cache does not remove a resource still shared by another. No OS/module-global registration.

## 9. Dependencies and edge cases

Depends on SvgIcon/Painter/private adapter, Theme, and Binding. No ResourceProvider/XML loading in paint; animated or external SVG remains outside resource v1.

Non-finite/negative size causes invalid_argument; zero is valid. Non-finite color causes invalid_argument at Spec; components outside [0,1] clamp for visible tint without modifying source options.

monochrome(false) ignores explicit color as specified and preserves native image colors; ImageView covers Fill/Cover fit for illustrative SVGs. Missing file/failed parse produce an invalid handle rather than an invisible retry button.

## 10. Files and compatibility

Target: `include/nativeui/icon_view.hpp` and `src/icon_view.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: svg.hpp and the private adapter in src/skia_svg.cpp. SvgIcon/SvgCache preserve historical APIs. The monochrome adapter remains backend-private; icon_view.cpp contains measurement/composition/tint requests rather than an empty include-only file.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `icon_view_ratio_size`: typographic/explicit height and SVG ratio are correct.
- `icon_view_monochrome_alpha`: red/blue shapes become the requested tint with alpha preserved.
- `icon_view_native_colors`: monochrome false preserves original colors and ignores tint.
- `icon_view_shared_resource`: two differently colored views do not mutate each other's DOM.
- `icon_view_invalid_clip`: invalid SVG and zero bounds paint nothing and do not pollute scopes.
- `icon_view_accessible_icon`: decorative and alt expose exactly the expected roles.

Create `examples/features/icon_view.cpp` and target `nativeui_example_icon_view`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.