# Avatar

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A circular avatar displaying a valid raster image, otherwise name initials on a deterministic background. Covers profiles, authors, and collaboration items without a network user service.

Absent from NativeUI; [Image](../include/nativeui/image.hpp), TextService, and Painter provide foundations. [ImageView](image_view.md) supports Cover cropping, but Avatar owns its fallback logic.

MyGo: `ui/indicators.go`, `Avatar`, `initials`, `hslColor`, name FNV-1a hash for hue. The target preserves Cover images, stable fallback, and image role; Unicode must be explicitly safe rather than byte-sliced.

## 2. Public API and composition

Proposed target API:

```cpp
class Avatar {
public:
    explicit Avatar(std::string name);
    explicit Avatar(Binding<std::string> name);
    explicit Avatar(State<std::string>& name);
    Avatar&& image(Image value) &&;
    Avatar&& image(Binding<Image> value) &&;
    Avatar&& image(State<Image>& value) &&;
    Avatar&& size(double diameter) &&;
    Avatar&& initials(std::string value) &&;
    Avatar&& style(AvatarStyle value) &&;
    Spec spec() &&;
};
```

Defaults: diameter 32 DIP, no image, automatic initials, informative semantics with name. New AvatarStyle: TextStyle initials_text, optional background/foreground, border_width/color; automatic foreground ensures contrast with the chosen background.

Proposed target example: `ui::Avatar{"Camille Martin"}.size(32.0).spec()`; dynamic name and image can observe two independent Bindings.

## 3. State, ownership, and notifications

Name/image/static initials/style are owned; State overloads convert immediately to Binding. External account/contact data remains with the application.

Name observation recalculates initials/background/semantics; image updates choose image/fallback. Both observers publish independent snapshots at the checkpoint; no on_change callback or built-in loader.

Binding source destroyed: get retains the last name/image, valid=false and observe inactive without destruction notification. No user-data dereference or resetting initials to zero; shared Image backing may remain visible.

## 4. Interactions

Display only: no focus, capture, activation, wheel, dragging, or drop. All inputs Ignored; Button/ContextMenu may explicitly wrap it.

No automatic “change photo” action or file opened on double-click. Identity/login validation is outside the component.

Cancellation does not apply. Parent ReadOnly/Disabled does not erase the name; Disabled may attenuate border/fallback text through style.

## 5. Measurement and layout

Square diameter measurement; layout centers a disk of diameter min(bounds.w,bounds.h,diameter). Cover image crops centrally within a circular clip, never a square with visible corners.

Initials centered through TextService, automatic size 0.4×diameter when initials_text supplies no override; a long initials override clips without widening Avatar.

Logical dimensions; zero diameter gives no pixels/measurement or invalid clipping operation. Rectangular space does not stretch the circle into an ellipse.

## 6. Presentation and invalidation

Automatic initials fallback v1: split words on ASCII whitespace/punctuation and valid Unicode boundaries; take the first grapheme of the first/last word, only one for a single word. Uppercase only ASCII a-z, leaving other graphemes without approximate transformations.

Empty name or no usable letter/digit grapheme: “?”. Explicit initials override, including an empty string, replaces automatic initials; semantic value remains name.

Automatic background=hue from FNV-1a32 of repaired name UTF-8 bytes, saturation 0.45/lightness 0.55; internal sRGB calculation. Automatic foreground is black or white according to the better WCAG contrast of these candidates; explicit style may replace these choices without a global cache.

## 7. Accessibility

Target contract: SemanticRole::Image named by name; no separate Text for initials already representing that name. Image/fallback do not change semantic identity on every replacement.

Empty name gives “unnamed avatar” description and empty name value; the application should supply an informative name. Do not announce “Camille Martin” twice with a neighboring Label if composition explicitly excludes it.

T068 bridges are deferred. No IME, native profile data, or implicitly delivered photo-change announcement.

## 8. Lifecycle and recovery

UI/main thread; separate RAII subscriptions, copy Image backing and name snapshot before paint. Paint/measure contexts are borrowed only during the callback.

Prepare fallback before publishing name; failed allocation/measurement leaves the last consistent snapshot. Crop clip/scopes must balance if image or text backend throws.

Destruction is no-throw without application notification/load cancellation; external async loader uses an application token before State update. Node removal makes stale invalidators no-ops and does not retain the application indefinitely.

## 9. Dependencies and edge cases

Depends on Image/TextService/Binding and Painter clipping rather than account/Sidebar models. Decode/ResourceProvider are prepared upstream, not at first paint.

Invalid Image after load failure uses initials; a valid replacement returns to the image without a global error cache. Transparent image alpha reveals fallback background; a valid even fully transparent image does not restore initials.

Non-finite/negative diameter/border_width cause invalid_argument; zero is valid. Invalid UTF-8 is repaired identically for hash/initials/text. A combining grapheme/emoji may be used as an initials override; automatic detection respects boundaries without splitting bytes.

## 10. Files and compatibility

Target: `include/nativeui/avatar.hpp` and `src/avatar.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: existing Image/State/TextService/Painter. AvatarStyle and options stay in avatar.hpp; actual fallback/crop/layout/observation in avatar.cpp. Reuse Cover draw_image without duplicating decode/cache or introducing public Skia types.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `avatar_image_fallback`: valid/invalid Image update chooses crop or initials without paint-time decoding.
- `avatar_initials_unicode`: empty name, one/two words, combining accents, invalid UTF-8, and empty override are stable.
- `avatar_hash_contrast`: the same name gives the same background and the highest-contrast black/white choice.
- `avatar_circle_clip`: Cover and differing parent ratio remain circular without pixels outside the clip.
- `avatar_sources_lifetime`: vanished name/image Binding sources retain safe last snapshots.
- `avatar_multi_instance`: one avatar's updates/caches/styles do not affect others.

Create `examples/features/avatar.cpp` and target `nativeui_example_avatar`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.