# ImageView

**Status: new — implementation required.**

[Component catalog](widgets.md)

NativeUI reference: `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo reference: `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Purpose and current state

A declarative raster or SVG image component preserving document colors, with fit and intrinsic measurement. Distinct from the Image resource and monochrome IconView.

NativeUI has [Image](../include/nativeui/image.hpp), [SvgIcon](../include/nativeui/svg.hpp), caches, and CanvasContext2D.draw_image/draw_svg in [component_base.hpp](../include/nativeui/component_base.hpp). No current public ImageView; existing SVG uses centered contain.

MyGo: `ui/widgets.go`, `ImageSource`, `Image`, `Element.intrinsicSize`, `Element.Fit`. Bitmap or SVG, natural size in DIP, and configured fit; Icon is separate for tinting. Assemble prepared resources without copying Go storage policy.

## 2. Public API and composition

Proposed target API:

```cpp
class ImageView {
public:
    using Source = std::variant<Image, SvgIcon>;
    explicit ImageView(Image value);
    explicit ImageView(SvgIcon value);
    explicit ImageView(Binding<Source> value);
    explicit ImageView(State<Source>& value);
    ImageView&& fit(ImageFit value) &&;
    ImageView&& size(Size value) &&;
    ImageView&& pixel_scale(double value) &&;
    ImageView&& alt(std::string value) &&;
    ImageView&& decorative(bool value = true) &&;
    Spec spec() &&;
};
```

Defaults: centered Contain, no explicit size, pixel_scale=1, empty alt, decorative=true. `pixel_scale` converts only raster intrinsic size from pixels to logical units; SVG intrinsic_size is already logical and pixel_scale has no effect.

Proposed target example:

```cpp
ui::Image photo;
auto view = ui::ImageView{photo}
    .fit(ui::ImageFit::Cover)
    .size({96.0f, 64.0f})
    .alt("Document preview")
    .decorative(false)
    .spec();
```

## 3. State, ownership, and notifications

The component owns a copyable Image/SvgIcon handle or Source Binding; State converts in the constructor. Handles share backing and retain no borrowed encoded buffer.

Source update prepares dimensions/snapshot, then publishes, invalidating layout if intrinsic size changes and paint/semantics. A resource replaced during paint remains in an independent snapshot until current paint ends.

Binding invalid after source destruction retains the last readable resource, with inactive observation and no automatic notification; no setter or on_change. Loading bytes and handling async completion remain with the application, outside paint.

## 4. Interactions

No implicit input, focus, capture, activation, or drag/drop. Pointer, wheel, keyboard, and text are Ignored; a Button/Link parent provides any interaction.

No confirmation/cancellation. Displaying a photo creates neither a context menu nor on-demand file/network access.

Do not use the pixel rectangle as a link proxy. Image semantics and parent actions stay separate.

## 5. Measurement and layout

Natural measurement=Image.size()/pixel_scale or SvgIcon.intrinsic_size; explicit size gives the specified preferred size in logical coordinates. Do not equate raster pixels with framebuffer DPR.

Fill stretches to bounds, Contain preserves ratio and centers with free bands, Cover preserves ratio and clips sides within bounds. Measurement is identical across fit modes; only paint mapping changes.

SVG requires a private draw-adapter extension for Fill/Cover; do not claim current draw_svg provides them. Raster reuses draw_image. Zero/non-finite destination is a no-op, with balanced clipping even on exceptions.

## 6. Presentation and invalidation

ImageView preserves original colors and alpha; no tint, Theme image slot, or imposed checkerboard. Transparency reveals the background composed by the application.

Changing fit/Source affects paint; dimensions/pixel_scale affect layout and paint. No implied SVG or GIF animation: resources are static as prepared.

No continuous repaint or decoding during paint. Resource/backend rendering uses RAII clip/transform scopes to isolate each image from siblings.

## 7. Accessibility

decorative=true: None by default. decorative=false: SemanticRole::Image, alt name, no action; alt serves only accessibility and is not visible fallback text.

An informative Image should supply non-empty alt; empty alt is allowed but documented as an unnamed image, to be detected for example by an application self-test. Binding changes update resource bounds without assigning a new id every frame.

Hooks/role exist; T068 bridges are deferred. No IME or claimed OCR/native capability.

## 8. Lifecycle and recovery

UI/main thread for Binding/composition, and resource preparation according to Image/SvgIcon contracts. Handles preserve valid backing even if the preparation cache is cleared.

Acquire Source snapshot before measure/paint, with no raw backend pointer retained in the component. Adapter/render errors restore scopes before C++ propagation after invariants; no automatic callback retry.

Destruction is no-throw, subscription RAII, with no loading function called during unmounting. No new global mutable data/resource registry; two UIs may display distinct handles with the same names.

## 9. Dependencies and edge cases

Depends on Image/SvgIcon resources and existing Painter adapters. ResourceProvider/ImageCache/SvgCache are prepared/owned by the application; provider lifetime follows its cache contract rather than the widget's.

Invalid handle: no pixels, natural {0,0}, explicit size still preserved; no decode/network fallback. External SVG files/network remain forbidden by resource v1.

pixel_scale must be finite and strictly positive; explicit size components finite >=0, otherwise invalid_argument. Invalid SVG intrinsic size already means an invalid resource. Valid→invalid Source recalculates natural size without a phantom interactive hit area.

## 10. Files and compatibility

Target: `include/nativeui/image_view.hpp` and `src/image_view.cpp`. The header contains public declarations; the `.cpp` contains a real retained core, measurement, layout, applicable events, and rendering.

Source to extract or reuse: image.hpp, svg.hpp, and draw_image/draw_svg adapters. Preserve Image/ImageTexture/ImageCache and SvgIcon/SvgCache in their historical headers. Source variant and ImageView stay in image_view.hpp; private SVG fit extension remains behind the backend adapter without Skia types in the public widget.

Essential template adapters remain in the header and delegate to the non-template core. Preserve historical includes through their aggregate headers; do not leave a second implementation in the `.inc` files.

Register the future `.cpp` in `NativeUI::Core`, with no empty file or central widget switch. This API must not expose Pugl, Skia, OS, or plugin SDK types.

This delivery consists of documentation: no extraction or CMake changes are performed.

## 11. Tests and acceptance criteria

Tests to implement during implementation:

- `image_view_intrinsic_scale`: raster size/pixel_scale and logical SVG size match measurement.
- `image_view_fit_modes`: raster and SVG Fill/Contain/Cover are bounded and centered.
- `image_view_invalid_swap`: an invalid resource then prepared async replacement recalculates without paint I/O.
- `image_view_alpha`: original transparency and absence of tint preserved.
- `image_view_cache_lifetime`: clearing a cache does not remove backing retained by a view.
- `image_view_clip_fault`: after a rendering exception, a neighboring image preserves transforms.
- `image_view_semantics`: decorative/alt give the expected role and name.

Create `examples/features/image_view.cpp` and target `nativeui_example_image_view`, linked to `NativeUI::Core`. The `--self-test` mode uses deterministic events and a deterministic clock, runs without a display, and returns a nonzero code on the first failure.

Verify standalone header compilation, public composition, headless rendering, and coexistence of two independent UIs. Cover recovery from the failures described above under ASan/UBSan when lifetime is involved.

Acceptance: the named tests pass, no capture or registration remains after unmounting, and the published API matches these contracts. Verification performed here: declarations and sources were read; no C++ or interactive tests were run.