# NativeUI 1.0 rendering, styling and animation

This chapter documents the stable NativeUI 1.0 presentation model that is already implemented: UI-owned theme values, lexical style scopes, typed widget style resolution, retained invalidation, custom painting and instance-owned animation. Backend/private drawing details remain implementation concerns; public documentation follows the current validated public/package surface on `main`.

For retained structure, layout, widgets and input, see [Composition, layout and widgets](v1-composition-layout-and-widgets.md). For resource loading and Dispatcher ownership, see [Services, testing and limits](v1-services-testing-and-limits.md).

## Presentation ownership

Presentation state belongs to a concrete NativeUI UI/retained tree. Theme, style resolution, paint invalidation and animation are not process-global services and do not require a hidden current-UI singleton.

The stable layering is:

1. the owning UI supplies the root `Theme`;
2. lexical `StyleScope` values can override inheritable theme fields for one retained subtree;
3. typed widget style recipes combine those inherited defaults with component-local overrides;
4. the widget's `VisualState` selects interaction/state patches;
5. layout- or paint-level invalidation is requested according to what actually changed;
6. painting happens through NativeUI's retained drawing boundary rather than application-owned platform paint loops.

Normal consumer code should stay on NativeUI public abstractions. Skia, Pugl, platform backends and `nativeui/detail/` remain implementation surfaces, not normal application APIs.

## Theme

[`include/nativeui/theme.hpp`](../include/nativeui/theme.hpp) defines the root typed theme value. `Theme` is ordinary owned C++ value data: it stores no backend handles, subscriptions, singleton state or borrowed UI pointers. A UI owns its effective theme; copying or comparing an independent `Theme` has no retained side effects.

The five families have explicit responsibilities and logical-unit geometry:

- `ThemePalette` supplies surface/text/border/accent/focus/selection and standard interaction colors. Theme storage does not clamp or color-convert them.
- `ThemeTypography` supplies preferred/fallback family names, logical-unit font sizes, weights and slant. Theme construction does not perform font discovery; families are resolved later by the text service.
- `ThemeSpacing` and `ThemeRadii` are logical-UI-unit token scales. Storage does not impose monotonicity or positivity.
- `ThemeControlMetrics` contains shared standard-control dimensions. `minimum_width`, `control_height` and `minimum_hit_target` affect retained measurement; thumb/track/border/focus-ring values are currently paint geometry.
- `default_theme()` returns a fresh default value rather than a borrowed process-global object.

Theme values are not a synchronization primitive. Replacing the theme on a live retained UI is UI/main-thread work even though detached Theme values are ordinary copyable data.

### Theme invalidation

`classify_theme_change(previous, next)` is a pure value classifier with no callbacks or UI mutation. Equality is exact, including floating-point tokens and ordered font fallback vectors. It returns the minimum retained work required by the current v1 style system:

- `ThemeInvalidation::None` when the complete values compare equal;
- `ThemeInvalidation::Layout` when typography, spacing, `minimum_width`, `control_height` or `minimum_hit_target` differs;
- `ThemeInvalidation::Paint` for any other unequal palette/radius/paint-metric change.

`Layout` implies repaint as part of the resulting retained update. Purely visual color/radius/stroke changes should not force unnecessary measurement, while typography or measurement-token changes must not be downgraded to paint-only work.

## Lexical style scopes

[`include/nativeui/style_scope.hpp`](../include/nativeui/style_scope.hpp) provides typed inheritable Theme overrides for exactly one retained child subtree. A scope does not own a second style engine: it resolves a descendant `Theme`, and the normal typed widget resolvers then apply component-local recipes and `VisualState` on top.

`StyleScopeOverrides` is sparse owned value data. Every empty optional inherits the nearest outer value; nested scopes are applied outer-to-inner, so the nearest supplied field wins independently. The five override groups mirror Theme:

- `StyleScopePaletteOverrides` covers background/surface/text/muted/border/accent/disabled/selection/focus/control/track colors. Colors are copied verbatim.
- `StyleScopeTypographyOverrides` owns preferred/fallback families plus base/control/label sizes, weights and slant. Font sizes are logical UI units; the scope does not perform font discovery.
- `StyleScopeSpacingOverrides` and `StyleScopeRadiiOverrides` contain logical-unit spacing/radius tokens.
- `StyleScopeControlOverrides` contains logical-unit standard-control metrics. `minimum_width`, `control_height` and `minimum_hit_target` are layout-affecting; thumb/track/border/focus-ring values follow the Theme classifier as paint metrics.
- `StyleScopeOverrides` groups those families and has exact value equality. Float/color comparisons are exact; ordered font fallback vectors are significant.

The scope layer stores numeric values verbatim rather than clamping or normalizing them. Per-instance width/height constraints, Flex/Grid placement, scroll position, callbacks, models and availability are intentionally absent: those remain component/retained state rather than inheritable style.

### Pure scope resolution and invalidation

`apply_style_scope_overrides(inherited, overrides)` receives `inherited` by value and returns a fully-owned Theme. Empty fields preserve inherited values. The result borrows nothing from either argument; applying nested scopes is therefore simply repeated outer-to-inner application. Typography strings/vectors can allocate and allocation failures propagate.

`classify_style_scope_change(inherited, before, after)` first resolves both patches against the same inherited Theme and then delegates to `classify_theme_change()`. This makes **effective** equality authoritative: two different sparse patches that yield the same descendant Theme produce `ThemeInvalidation::None`. Typography/layout-token differences produce `Layout`; other effective differences produce `Paint`.

Both helpers are pure with respect to retained UI state: no callbacks or native/backend operations occur. They are ordinary preparation/UI-domain helpers, not audio-real-time operations, because owned Theme copies can allocate.

### Retained StyleScope lifetime

`StyleScope` wraps exactly one child Spec and is transparent to that child's constraints and final bounds. The value constructor owns an immutable override patch for the retained lifetime. The `Binding<StyleScopeOverrides>` constructor stores the binding handle; the `State<StyleScopeOverrides>&` convenience constructor delegates through `State::binding()` and does not retain a raw `State*`.

A live binding-backed scope subscribes only while mounted. Updates are UI/main-thread operations and follow the normal State/Binding notification/reentrancy contract. Each replacement resolves against the current inherited Theme, classifies the effective delta, and requests no invalidation, bounded paint invalidation, or layout+paint as appropriate. Unmount releases the subscription and callback bridge, so later State changes cannot call into the removed retained component.

Scopes remain instance/tree-local. Sibling subtrees do not inherit one another's overrides, and independent UI trees share no mutable current-scope registry. Dynamic descendants inserted later resolve the current lexical ancestry when they enter the retained tree.

```cpp
ui::StyleScopeOverrides panel;
panel.palette.surface = ui::Color{0.10f, 0.14f, 0.20f, 1.0f};
panel.typography.control_size = 15.0f;
panel.controls.control_height = 38.0f;

ui::State<ui::StyleScopeOverrides> live_scope{panel};

auto subtree = ui::StyleScope{
    live_scope,
    ui::Column{
        ui::Button{"Scoped", [] {}},
        ui::Label{"Inherits the same lexical Theme"},
    }};

// Later, on the owning UI thread:
auto next = live_scope.get();
next.palette.surface = ui::Color{0.18f, 0.12f, 0.10f, 1.0f}; // paint-only
live_scope.set(next);
```

The maintained [`t039_style_scope`](../examples/features/t039_style_scope.cpp) example covers nested precedence, exact no-op/paint/layout invalidation, sibling/two-tree isolation, dynamic insertion and restoration after scope removal.

## Typed widget styles

[`include/nativeui/style.hpp`](../include/nativeui/style.hpp) and the widget-family `*_style.hpp` headers use typed structures rather than string-keyed properties or runtime reflection.

The common model is:

- a typed patch contains optional fields;
- a complete style recipe contains base and state-specific patches;
- inherited theme/scope defaults resolve first;
- explicit component-local style resolves after inherited values;
- the widget's `VisualState` selects the applicable state layers;
- the result is a concrete resolved style consumed by measurement and painting.

### Interaction precedence

The common `InteractionVisualState` branch has fixed precedence:

```text
disabled > pressed > hovered > normal
```

Other states remain orthogonal where a widget supports them. For example, checked/selected, read-only and focused patches can be applied without replacing the interaction branch.

This matters for combinations such as focused+hovered or selected+disabled: style resolution is deterministic rather than dependent on callback order.

### Geometry versus paint fields

A style field that participates in control measurement must trigger layout when its effective value changes. A paint-only field should remain paint invalidation. The same rule applies whether the value came from the root theme, a lexical scope or a component-local recipe.

Do not assume every visual property is paint-only merely because it is named "style". Typography, control height, minimum size, padding or similar metrics can alter layout.

For field-by-field standard-widget recipes and examples, see [Widget style reference](v1-widget-style-reference.md).

## Custom painting boundary

Custom components paint through NativeUI's retained callback using [`Painter`](../include/nativeui/paint.hpp). [`paint_style.hpp`](../include/nativeui/paint_style.hpp) supplies backend-neutral brush/effect values and [`path.hpp`](../include/nativeui/path.hpp) supplies vector paths/stroke geometry. Coordinates, lengths, widths, radii and effect offsets/sigmas are **logical UI units**; framebuffer/device scaling stays below this retained boundary.

### Painter ownership, threading and backend escape hatch

A `Painter` **borrows** the `SkCanvas&` supplied to its constructor and must not outlive that canvas. Painting is UI/render-thread work and can allocate while materializing paths, gradients, effects, text, image/runtime-shader brushes or unusually deep save stacks; it is not an audio-real-time API.

`canvas()` is a low-level integration escape hatch. Normal components should use Painter methods: raw backend save/restore/transform operations bypass Painter's logical transform/history bookkeeping.

`current_transform()` reports the affine transform accumulated through Painter. Translation is in logical units, scale is dimensionless, and rotation uses **radians**. Non-finite transform inputs are ignored. Painter updates its logical transform only after the backend mutation succeeds.

### Scoped state, clipping and layers

`scoped_state()`, `scoped_clip(...)` and `scoped_layer(...)` return a non-copyable/non-movable `Painter::StateGuard`. The guard borrows the Painter and restores protected backend/logical state on destruction. Manual `save()`/`restore()` calls inside a protected scope must balance before the guard dies; crossing its restore floor is a programming error.

Rectangular clips turn non-finite, empty and non-positive geometry into an empty clip. Rounded clips canonicalize non-finite/non-positive radius to zero and clamp positive radius to half the smaller rectangle dimension. Path clipping validates every command coordinate before publishing its save frame; non-finite path geometry becomes an empty clip.

`scoped_layer(bounds, options)` hard-clips to finite logical bounds and applies `PaintOptions` once when the offscreen layer is composed back. Invalid bounds deliberately create an empty clip-only scope rather than an unbounded backend layer. Scope setup is transactional: failures roll back private backend frames before propagation.

The effect overload adds `Effect` filtering. Zero Gaussian blur reduces to an ordinary layer. Transparent `drop_shadow` keeps the source without a shadow, while transparent `drop_shadow_only` produces empty output. NativeUI computes a conservative finite device support rectangle; unrepresentable/non-finite transformed support fails closed instead of requesting an unbounded temporary surface. Materialized effects set `used_effects()`, which lets the scene renderer conservatively fall back from localized partial reconstruction when needed.

### Paths and primitive drawing

[`Path`](../include/nativeui/path.hpp) owns move/line/quadratic/cubic/close commands in logical coordinates and retains no Painter/backend resource or caller-owned point storage. Mutators copy coordinates verbatim and do not pre-normalize NaN/Inf. Fluent construction may allocate as its vector grows; copying a Path copies that owned command vector and may allocate. `clear()` is noexcept, retains capacity and is intended for same-owner reuse after synchronous Painter consumption returns. Path mutation is unsynchronized and is not an audio-real-time construction API.

`StrokeStyle::width` is a logical length. `StrokeCap::Butt` ends at the endpoint, `Round` adds a semicircular cap, and `Square` extends by half the stroke width. `StrokeJoin::Miter` extends outer edges to their intersection subject to `miter_limit`; `Round` arcs the corner and `Bevel` truncates it. `miter_limit` is a dimensionless miter-length/stroke-width ratio and negative values clamp to zero before backend use. `stroke_path()` treats an empty path or non-positive width as a no-op; finite positive widths remain the portable caller contract.

Painter also draws rounded rectangles, circles, lines and arcs. Arc start/end are in **radians**, sweep is `end - start`, and arc/line helpers use round caps.

Primitive drawing and protected clipping intentionally have different invalid-input contracts. `scoped_clip(...)` canonicalizes invalid geometry to an empty clip because an unbounded/invalid clip would threaten retained traversal correctness. Primitive draw helpers instead forward geometry to the backend. Callers should therefore provide finite points/rectangles/angles, non-negative radii and finite positive stroke widths when they need portable deterministic output. In particular, rounded-rectangle draw radii are **not** clamped the way rounded clip radii are.

Path draw calls borrow the `Path` only for the synchronous call and convert it to backend geometry immediately; conversion may allocate. `fill_path()` treats an empty path as a no-op. `stroke_path()` additionally treats `width <= 0` as a no-op and clamps the miter limit to at least zero. A NaN stroke width is not rejected by the `<= 0` check, and non-finite path commands are not rejected by the draw path, so finite inputs remain the caller contract. By contrast, `scoped_clip(path)` validates every command and fails closed to an empty clip.

Every Brush-taking primitive borrows the Brush only until return. Gradient/image/runtime-shader materialization happens synchronously, may allocate or throw, and no pointer/reference into the caller's Brush is retained. `PaintOptions` is applied after source materialization using the documented opacity/blend canonicalization. A draw/materialization exception propagates to the retained paint boundary; the transactional rollback guarantee described above applies specifically to protected scope setup, not to arbitrary primitive draws.

`push_clip()`/`pop_clip()` are legacy/manual pairing helpers. `push_clip()` saves state and forwards the rectangle directly; it does not perform `scoped_clip()`'s invalid-geometry canonicalization or protected transactional setup. Pair every successful manual push with a pop in the same traversal and prefer `scoped_clip()` in fallible component code.

### Text drawing and measurement

`Painter::text(position, text, style)` treats `position.y` as the run's **vertical center**, not a baseline. `position.x` follows `TextStyle::align` (Left/Center/Right). Font size and coordinates are logical units; negative draw size clamps to zero. The UTF-8 view and `TextStyle` are borrowed only for the synchronous call. Font/fallback resolution and layout may allocate or throw; malformed UTF-8 is repaired into temporary owned valid bytes before being passed to Skia, and Painter retains no view into caller storage after return.

`measure_text(text, size)` is the width-only convenience path through `TextService` with the same borrowed-input and allocation/error boundary. Use the full text service/style APIs when ascent/descent or explicit typography is required.

## Brushes, gradients, paint options and effects

[`paint_style.hpp`](../include/nativeui/paint_style.hpp) stores renderer-neutral values; backend resources are materialized later by Painter.

`PaintOptions::opacity` is canonicalized at draw/layer time: finite values clamp to [0, 1], non-finite opacity falls back to 1.0. `BlendMode::SourceOver` is standard source-over alpha compositing; `Multiply` and `Screen` use the corresponding artistic equations; `Plus` performs additive composition. Unknown enum payloads fail closed to SourceOver at materialization.

`LinearGradient` and `RadialGradient` own their stop vectors. Constructor points/centers/radii use logical Painter coordinates/lengths. Initializer-list storage is borrowed only during construction and copied before return; construction may allocate, but no pointer into caller storage is retained. Offsets are normalized [0, 1] values and must be finite and **strictly increasing**, with at least two stops. Validation is deliberately deferred to materialization: an invalid non-empty sequence falls back to its first stop color; an empty sequence falls back to `Color{}` (opaque black with the current `Color` defaults). A radial gradient stores radius verbatim; a non-positive/non-finite radius cannot materialize a radial shader and likewise falls back to the first stop color. `stops()` is a borrowed vector reference and must not be retained across assignment/destruction.

`VisualOutset` reports conservative logical expansion independently on left/top/right/bottom. It is damage-accounting metadata, not a clip or allocation request by itself. `VisualOutset::uniform(v)` turns non-finite/non-positive input into zero expansion.

`Brush` is the common owned/snapshotted fill source: solid `Color`, owned linear/radial gradient, `ImageTexture`, or immutable snapshot of a prepared `ShaderInstance`. Invalid `ImageTexture` construction yields a transparent Brush. Copying may allocate because gradient stop vectors are owned; moving is noexcept and leaves the source as a valid transparent Brush.

`Effect` is a small noexcept-copyable value. Gaussian sigma canonicalizes to [0, 64] logical units. Shadow offsets canonicalize per axis to [-256, 256], and color channels to [0, 1]; non-finite values are replaced with bounded zero-equivalents before clamping. `visual_outset()` reports conservative non-negative logical expansion using three sigma plus directional shadow offset. Fully transparent shadows report zero outset.

```cpp
ui::Path outline;
outline.move_to({4.0f, 4.0f})
       .line_to({92.0f, 4.0f})
       .line_to({92.0f, 28.0f})
       .close();

auto state = painter.scoped_state();
auto clip = painter.scoped_clip({0.0f, 0.0f, 96.0f, 32.0f}, 6.0f);

ui::LinearGradient fill{
    {0.0f, 0.0f},
    {96.0f, 0.0f},
    {{0.0f, ui::Color{0.1f, 0.7f, 0.4f, 1.0f}},
     {1.0f, ui::Color{0.8f, 0.9f, 0.2f, 1.0f}}}};

painter.fill_path(
    outline,
    ui::Brush{fill},
    {.opacity = 0.9f, .blend = ui::BlendMode::SourceOver});
```

Painter state belongs to the current retained paint traversal. It is mutable, unsynchronized and serial to the owning UI/render domain. Do not retain `Painter&`, `StateGuard`, `SkCanvas&`, Path/Brush/TextStyle borrows or borrowed gradient-stop references beyond the call/traversal lifetime that supplies them. Do not hand Painter to asynchronous work or use it as a cross-thread or audio-real-time handoff mechanism.

Runtime shaders use a two-stage contract:

1. `ShaderProgram::compile(sksl)` explicitly compiles source and freezes a supported reflected interface into an immutable, shared program;
2. `ShaderInstance` owns one mutable binding set for that program and can be snapshotted into a `Brush` for painting.

### Shader compilation and reflection

The source `string_view` passed to `compile()` is borrowed only for the call. A successful program owns its backend representation plus copied uniform/child names, so the caller may release or mutate the source buffer afterwards. Expected SkSL syntax/backend compile failures return `CompileError`; a program that compiles but uses a reflected interface outside NativeUI's profile returns `UnsupportedInterface`. The published profile supports scalar/vector float and signed-int uniforms, `layout(color)` float4/half4, and `uniform shader` children. Uniform arrays, matrices, and non-shader child interfaces are rejected rather than silently exposed through backend types.

`ShaderCompileResult::ok()` is exactly `program != nullptr`. Diagnostics own their message strings; source line/column use 0 when the backend does not supply a position. Compilation/reflection ownership may allocate, so allocation failures propagate rather than being converted to a normal shader diagnostic. Rendering never performs implicit source compilation.

`uniforms()` and `children()` return borrowed spans over program-owned reflection. Their names and span storage are stable only for the lifetime of the `ShaderProgram`; copy data that must survive beyond that lifetime.

### Typed bindings and snapshots

Constructing `ShaderInstance` retains the immutable program, allocates a zero-initialized numeric binding block, and creates empty child slots. Passing a null program throws `std::invalid_argument`. Copying an instance creates independent mutable bindings/child slots while sharing the immutable program. Moving is `noexcept` and leaves the source inert; setters on an inert instance return `NotFound`, and normal assignment can make it valid again.

Numeric setters are `noexcept`, allocation-free, and transactional:

- `NotFound`: no slot with that name, or the instance is inert;
- `TypeMismatch`: a numeric/color slot exists but requires another typed setter;
- `InvalidValue`: floating input contains NaN/Inf or the internal slot cannot accept the value;
- `Ok`: the complete binding was written.

`Color` is distinct from ordinary `Float4`: a `layout(color)` slot must use `set_color()`. Color channels must be finite but are not clamped by this binding API. Signed integer setters accept the complete `std::int32_t` domain.

`set_child(name, brush)` binds only reflected `uniform shader` slots. It snapshots/owns a copy of the Brush and may allocate; later lifetime or mutation of the caller's Brush does not alter the child slot. Unbound child slots render as transparent black. Nested shader Brush depth is bounded by `ShaderInstance::kMaxChildDepth == 16`; an over-depth replacement returns `InvalidValue` without replacing the previous valid child. Child Brush coordinates are evaluated in the runtime shader's coordinate expression, so `child.eval(p * 0.5)` intentionally changes child sampling coordinates.

Constructing `Brush{shader_instance}` snapshots the instance's current numeric bytes and child Brush references. Later changes to the original `ShaderInstance` do not mutate that Brush snapshot.

```cpp
constexpr std::string_view source = R"(
    uniform float gain;
    layout(color) uniform half4 tint;
    uniform shader texture;

    half4 main(float2 p) {
        return texture.eval(p) * gain * tint;
    }
)";

const auto compiled = ui::ShaderProgram::compile(source);
if (compiled.ok()) {
    ui::ShaderInstance instance{compiled.program};
    instance.set_float("gain", 0.8f);
    instance.set_color("tint", {1.0f, 0.7f, 0.5f, 1.0f});
    instance.set_child("texture", ui::Brush{ui::Color{1, 1, 1, 1}});

    ui::Brush stable_snapshot{instance};
}
```

Compilation, ShaderInstance construction/copying, child binding and Brush snapshot creation may allocate and are explicitly not audio-real-time operations. A mutable `ShaderInstance` has no internal synchronization; keep mutation in an owned UI/resource-preparation domain. Once prepared, pass NativeUI `Brush` values through retained painting rather than exposing backend shader/compiler objects through application contracts.

### Built-in procedural noise

[`noise.hpp`](../include/nativeui/noise.hpp) exposes immutable procedural sources that are prepared explicitly and then converted to a normal `Brush`. NativeUI currently provides `Value`, `Perlin`, `Simplex`, `WorleyF1`, and `WorleyF2` single-octave sources.

`NoiseOptions::feature_size` is the number of **logical Painter pixels per base noise cell** and must be finite and positive. `seed` is an exact 32-bit deterministic seed: repeating the same algorithm and options produces the same procedural field.

```cpp
auto result = ui::NoiseSource::create(
    ui::NoiseType::WorleyF1,
    {.feature_size = 32.0f, .seed = 0x12345678u});

if (result.ok()) {
    ui::Brush brush = result.noise.as_brush(
        ui::Color{0.05f, 0.08f, 0.12f, 1.0f},
        ui::Color{0.85f, 0.95f, 1.0f, 1.0f});
}
```

`NoiseSource::create()` validates the algorithm and feature size before compiling the built-in backend effect. Invalid inputs return `NoiseCreateError::InvalidArgument`. Internal shader compilation failure returns `BackendCompileFailed` plus a diagnostic when available. A default or failed source is safe to call: `as_brush()` returns a transparent solid brush.

#### Fractal noise

`NoiseSource::create_fractal()` combines multiple octaves of **Value, Perlin, or Simplex** noise. Worley F1/F2 are valid single-octave algorithms but are intentionally not accepted as fractal bases.

`FractalNoiseOptions` defaults to 4 octaves, lacunarity 2, gain 0.5, and `FBm`. Its setters are deliberately value-only and do **not** validate; validation occurs atomically when `create_fractal()` is called.

The accepted creation ranges are:

- octaves: `1..6`;
- lacunarity: finite `[1,4]` frequency multiplier per octave;
- gain: finite `[0,1]` amplitude multiplier per octave;
- mode: `FBm`, `Turbulence`, or `Ridged`;
- base feature size: finite and strictly positive.

The modes differ in how each octave's base value is converted and accumulated:

- `FBm` uses signed noise and remaps the normalized weighted result to `[0,1]`;
- `Turbulence` uses the absolute signed value;
- `Ridged` uses a squared ridge term `(1 - abs(signed_noise))^2`.

```cpp
ui::FractalNoiseOptions fractal;
fractal.set_octaves(5)
       .set_lacunarity(2.0f)
       .set_gain(0.5f)
       .set_mode(ui::FractalNoiseMode::Ridged);

auto result = ui::NoiseSource::create_fractal(
    ui::NoiseType::Perlin,
    {.feature_size = 48.0f, .seed = 0x12345678u},
    fractal);

if (result.ok()) {
    auto brush = result.noise.as_brush();
}
```

Fractal creation compiles one combined built-in shader; increasing octave count does not trigger one compilation per octave. Source creation and backend compilation may allocate and therefore belong to UI/resource-preparation code, never an audio/DSP real-time callback.

#### Color mapping

`as_brush(low, high)` maps procedural value 0 to `low` and 1 to `high`. Color channels are clamped to `[0,1]`; non-finite channels become 0. Interpolation occurs before premultiplication in renderer space. The resulting object is an ordinary NativeUI `Brush`, so application code does not receive Skia shader objects or backend handles.

### Backend boundary

Do not make direct Skia types, canvases or backend objects part of application/component contracts. T069 has been retired as a standalone freeze gate; the current validated public/package surface on `main` is authoritative. The stable rule remains the abstraction boundary: normal consumers paint through NativeUI, while renderer/platform objects remain implementation details.

T125/T130 have closed the retained dispatch, lifecycle, layout, paint and teardown exception-safety work. Presentation documentation may rely on those landed contracts, while implementation-only recovery mechanisms remain private.

## Text, images and vector content

Text, raster images and SVGs remain backend-neutral public values. Preparation may allocate, parse/decode data, consult platform services or call an application `ResourceProvider`; keep these operations in the UI/resource-preparation domain, never in an audio/DSP real-time callback. Painter/Canvas operations consume prepared handles without exposing Skia/platform objects to application contracts.

### Text and font contracts

[`include/nativeui/text.hpp`](../include/nativeui/text.hpp) uses logical UI units for font sizes and returned metrics. `TextStyle` owns its family and ordered fallback-family strings; no renderer-native font handle is retained. Malformed UTF-8 supplied to measurement/drawing is repaired with U+FFFD through the shared text path.

`FontManager::register_embedded_font(alias, bytes)` borrows both inputs only for the call and retains its own successful registration. Embedded aliases are process-shared and immutable: registering the same alias with byte-identical data is idempotent, while trying to replace an existing alias with different bytes fails. Plug-ins should namespace aliases when process-wide sharing is not intentional.

Face matching tries the requested family, then non-empty fallback families in order, then platform Unicode fallback. `FontMatch` owns only diagnostic metadata; `glyph_available` must be checked independently from the presence of a resolved family. Font registration/matching may allocate or synchronize and is not audio-RT safe.

`TextService::measure()` borrows the UTF-8 input only for the call and returns owned logical-unit metrics. Color and horizontal alignment do not affect measurement. Allocation failures are not represented by sentinel metrics and may propagate as normal C++ exceptions.

### Raster images

[`include/nativeui/image.hpp`](../include/nativeui/image.hpp) exposes `Image` as a copyable handle to immutable decoded backing. `Image::decode(encoded)` borrows the encoded span only during the call, copies it, eagerly realizes raster pixels, and returns an invalid handle for empty/malformed/unsupported data or ordinary backend decode failure. Successful callers can release the encoded source immediately. Decoding may allocate and is not audio-RT safe.

`Image::size()` reports decoded pixel dimensions; an invalid image reports `{0, 0}`. Equality is handle identity: copies of one decoded image compare equal, but two independent decodes of identical bytes need not.

Image drawing has two coordinate domains:

- source rectangles are decoded-image **pixel coordinates**;
- destination rectangles are Painter-local **logical UI coordinates**.

`ImageFit::Fill` stretches independently on both axes. `Contain` preserves aspect ratio and centers the complete selected source inside the destination. `Cover` preserves aspect ratio while center-cropping the selected source to fill the destination. Invalid handles and non-drawable geometry are safe no-ops at the drawing boundary.

### ImageTexture

`ImageTexture` is an owned value description layered over shared immutable `Image` backing. The source rectangle must be finite, positive and fully inside the decoded image; the destination rectangle must be finite and positive. Invalid construction canonicalizes to an inert texture. The destination rectangle describes one complete texture period in logical coordinates rather than a clip/bounds rectangle.

Tiling is independent on X/Y (`Clamp`, `Repeat`, `Mirror`, `Decal`). Sampling defaults to linear filtering with no mipmap selection. `TextureInterpretation::Color` uses color-managed RGB with ordinary alpha coverage; `Data` samples normalized numeric channels without color conversion/implicit premultiplication. Texture mutation only changes the public value description; backend shader/mipmap materialization remains renderer-owned.

The optional texture-local `Transform2D` maps texture/pattern coordinates into Painter-local coordinates before the Painter/device transform. NativeUI retains even a non-invertible transform verbatim so it can be inspected/replaced, but `valid()` becomes false until the transform satisfies the shared inversion contract. Copies share the decoded image; moves reset the source object to the inert state.

### ImageCache

`ImageCache` borrows one `ResourceProvider`, which must outlive the cache. `load(resource_id)` copies the identifier as its cache key, calls the provider synchronously only on a cache miss, then caches **both success and failure**:

- provider `std::nullopt` -> `ImageLoadError::NotFound`;
- returned bytes that fail `Image::decode()`, including an empty byte vector -> `DecodeFailed`;
- valid decoded image -> `None`.

A repeated identifier does not re-enter the provider or decoder until `clear()`. `size()` therefore counts cached failures as well as successes. `clear()` drops cache entries but does not invalidate `Image` handles already copied out. Provider/allocation exceptions propagate rather than being converted to an `ImageLoadError`.

### SVG icons

[`include/nativeui/svg.hpp`](../include/nativeui/svg.hpp) exposes `SvgIcon` as a copyable handle to an immutable parsed SVG DOM. `SvgIcon::parse(encoded)` borrows bytes only for the call. Empty/malformed SVG, parser failure, or missing/non-positive/non-finite intrinsic geometry returns an invalid handle. NativeUI uses the parsed DOM container size when valid and falls back to a finite positive root `viewBox` width/height when needed.

V1 SVG resources are static and self-contained: NativeUI does not fetch external file/network resources or drive SVG animation. Drawing uses a centered contain-style fit, preserves intrinsic aspect ratio, clips to the requested destination and treats non-positive/non-finite destinations as no-ops.

`SvgCache` has the same lifetime and caching rules as `ImageCache`: it borrows its provider, caches successes and failures per exact identifier, maps provider absence to `NotFound`, maps returned-but-unusable bytes (including empty bytes) to `ParseFailed`, counts all entries in `size()`, and retries only after `clear()`. Existing copied `SvgIcon` handles keep the parsed DOM alive after cache clear/destruction.

### Resource-loading example

```cpp
class Assets final : public ui::ResourceProvider {
public:
    std::optional<std::vector<std::byte>> load(std::string_view id) override;
};

Assets assets;                    // must outlive both caches
ui::ImageCache images{assets};
ui::SvgCache svgs{assets};

const auto logo = images.load("ui/logo.png");
const auto mark = svgs.load("ui/mark.svg");

if (logo) {
    canvas.draw_image(
        logo.image,
        {0.0f, 0.0f, 96.0f, 48.0f},
        ui::ImageFit::Contain);
}

if (mark) {
    canvas.draw_svg(mark.icon, {104.0f, 0.0f, 48.0f, 48.0f});
}
```

See [Services, testing and limits](v1-services-testing-and-limits.md) for the broader resource/provider lifetime and threading boundary and [Packaging and CMake](v1-packaging-and-cmake.md) for binary-resource packaging.

## Retained invalidation

Presentation changes should invalidate the smallest correct retained scope.

At a conceptual level:

- **paint invalidation** means existing layout remains valid and the affected retained paint region must be redrawn;
- **layout invalidation** means measurement/placement can change and layout must run before repaint;
- no-op effective theme/scope replacement should remain a no-op.

Animation uses the same distinction explicitly, so a paint-only tween does not need to become a layout animation.

### DirtyRegion

[`invalidation.hpp`](../include/nativeui/invalidation.hpp) exposes `DirtyRegion`, NativeUI's bounded logical-coordinate damage accumulator.

`add(rect, clip)` first intersects `rect` with `clip`. Both rectangles must use the same **logical coordinate space**; this value type performs no framebuffer/device-scale conversion. An empty clipped rectangle, or one already completely covered by existing damage, returns `std::nullopt`.

Overlapping **or touching** rectangles are coalesced. At most eight disjoint rectangles are retained (`kMaxRects == 8`). If adding another independent fragment would exceed that budget, the accumulator deliberately collapses all damage into one conservative bounding rectangle. This bounds retained bookkeeping while preserving correctness: the renderer may repaint more pixels, but never fewer than the recorded damage requires.

Construction reserves storage for the full bounded working set and may throw on allocation failure. Once a `DirtyRegion` has been successfully constructed, `add()` is allocation-free and `noexcept`; `clear()` preserves reserved capacity. Move assignment is also `noexcept`, and moved-from instances remain valid/empty and reusable.

`rects()` returns a **borrowed** reference to the current coalesced rectangle vector. Do not retain that reference across mutation, assignment or destruction of the `DirtyRegion`.

The value type has no internal synchronization. Retained Tree/UI use follows NativeUI's UI/main-thread confinement. A separate caller sharing a `DirtyRegion` across threads must supply its own synchronization.

### Relationship to Tree damage and partial repaint

The low-level retained [Tree runtime](v1-low-level-tree-runtime.md) exposes logical dirty regions through `Tree::dirty_regions()`, `invalidate()`, `invalidate(Rect)` and `invalidate_layout()`. Normal applications should usually invalidate through `UI`/component callback contexts rather than owning a second repaint loop.

The post-v1 T096 renderer uses bounded logical damage as one input to conservative partial-scene reconstruction: it maps logical damage to a device clip, rebuilds only when that mapping/effect state is proven safe, and otherwise falls back to a full scene. This optimization does not change the public coordinate contract: application/component invalidation remains logical, and device-pixel expansion belongs below the retained public surface.

Avoid application-level whole-window repaint loops as a substitute for NativeUI invalidation. Components should use the retained invalidation path supplied by their owning UI/tree.

## Animation

[`include/nativeui/animation.hpp`](../include/nativeui/animation.hpp) defines one UI-thread-confined scalar animation registry per UI/view domain. It uses the owning `Dispatcher` for wake scheduling and monotonic time; it does not create a worker thread, a second event loop or a process-global animation table.

### Easing, timing and callbacks

A tween interpolates finite scalar values from `from` to `to` over a finite non-negative `DispatcherDuration` measured in seconds. `Linear` is constant-rate; `EaseIn`, `EaseOut` and `EaseInOut` are cubic curves. Positive-duration tweens do **not** synchronously write the initial `from` value when started. Their first value callback runs at a later Dispatcher checkpoint. Progress is derived from monotonic elapsed time, not wake count, and the successful final step writes `to` exactly.

For every normal step the observable callback order is:

1. `ValueCallback(next)`;
2. the selected `AnimationInvalidationTarget` route;
3. if the animation completed, removal from the active registry;
4. optional `CompletionCallback()`.

Callbacks execute synchronously in the Dispatcher owner/UI domain and may re-enter the animation context, cancel this or another animation, start new work, or destroy the context. A self-cancel during the value/invalidation phase suppresses later completion for that entry. Destroying the `AnimationContext` cancels its pending wake and discards entries without invoking application completion callbacks.

If a value, invalidation or completion callback throws during a scheduled step, that animation is terminal and is not retried. NativeUI removes it, attempts to preserve scheduling for unrelated surviving animations, and rethrows the original exception. Failure while taking the per-tick active-id snapshot likewise preserves/rearms surviving work when possible. These recovery paths avoid leaving a logically active entry permanently unscheduled.

### Handles, scheduling and rejection

`AnimationHandle` is an opaque context-scoped identity. It keeps only a small owner token alive; it does not retain the animation context, Dispatcher queue, callbacks or target object. `handle.valid()` means the identity is non-empty, **not** that the entry is still active. A handle can remain syntactically valid after completion, cancellation or owner destruction; `cancel()` returns `false` for such stale handles and for handles from another context.

Multiple active animations in one context share at most one one-shot Dispatcher wake, normally rearmed at a 16 ms cadence. The cadence is a wake policy rather than a fixed simulation step. Delayed event-loop checkpoints therefore produce elapsed-time tween progress, while spring integration applies its documented `max_dt` clamp.

Starting an animation returns an empty handle without invoking callbacks when required state is unavailable or validation fails: invalid Dispatcher, empty value callback, invalid invalidation target, non-finite scalar/duration, negative tween duration, unknown easing value, invalid spring options, id exhaustion, or inability to obtain the initial wake. Allocation or scheduler exceptions are not converted to a normal empty-handle result; they propagate after the provisional animation is removed.

### Retained invalidation target

`AnimationInvalidationTarget` owns two `std::function<void()>` routes: one bounded paint invalidator and one layout invalidator. Both must be present. `AnimationInvalidation::Paint` invokes only the paint route; `Layout` invokes the layout route so normal retained layout + repaint behavior follows.

The function objects are owned, but arbitrary captured pointers/references are not automatically made lifetime-safe. Component code should use the retained node/owner invalidators supplied by its lifecycle rather than capturing an unguarded raw Tree or component pointer that may disappear before a later tick.

### Spring solver and units

`start_spring()` animates a generic scalar in application-defined units U. `SpringOptions::initial_velocity` is U/s, `distance_epsilon` is U, `velocity_epsilon` is U/s, `stiffness` acts as s^-2 and `damping` as s^-1 in the implemented equation:

```text
acceleration = stiffness * (target - value) - damping * velocity
velocity    += acceleration * dt
value       += velocity * dt
```

This is one semi-implicit Euler update per wake. The elapsed `dt` is clamped to the strictly-positive `max_dt`; a long stall does not replay hidden catch-up substeps. The spring completes only when both distance and velocity satisfy their non-negative epsilons, at which point NativeUI snaps/writes the exact target and invokes completion after invalidation. Zero stiffness/damping are valid, so a configuration that cannot physically converge may remain active until explicitly cancelled.

All spring scalars/options must be finite. stiffness, damping and both epsilons must be >= 0; `max_dt` must be > 0. Invalid configurations are rejected before scheduler/callback state is mutated.

### Reduced motion and immediate paths

A zero-duration tween and every tween/spring started while reduced motion is enabled use the synchronous immediate path: exact target write, selected retained invalidation, then completion, all before the start call returns. No timer is armed and the returned handle is empty.

Calling `set_reduced_motion(true)` while animations are active cancels the pending wake and synchronously converges the captured active entries to their exact targets in the same write -> invalidation -> completion order. Reentrant callbacks are allowed. If allocation or any callback/invalidation throws during this policy transition, reduced motion remains enabled, all remaining animation entries are discarded, the wake is cancelled, and the exception propagates. Disabling reduced motion later does not resurrect completed/discarded work.

### Example

```cpp
ui::AnimationContext animations{window.dispatcher()};
ui::State<float> opacity{0.0f};

ui::AnimationInvalidationTarget invalidation{
    [&] { screen.invalidate({0.0f, 0.0f, 120.0f, 40.0f}); },
    [&] { screen.invalidate_layout(); }};

ui::AnimationHandle fade = animations.start_tween(
    opacity.get(),
    1.0f,
    std::chrono::milliseconds{250},
    ui::Easing::EaseInOut,
    ui::AnimationInvalidation::Paint,
    invalidation,
    [&](float value) { opacity.set(value); });

// Later, still in the owning UI domain:
(void)animations.cancel(fade);
```

The maintained [`t040_animation`](../examples/features/t040_animation.cpp) executable verifies easing vectors, validation/rejection, spring stepping/rest behavior, timer coalescing and context isolation, retained Paint-vs-Layout invalidation, reduced-motion convergence and reentrant teardown.

Animation setup, callback storage, scheduling and per-tick snapshots may allocate and synchronize through Dispatcher services. None of this API is suitable for direct use from an audio/DSP real-time callback; hand real-time state into the UI domain through an explicitly reviewed bounded mechanism.


## Choosing the invalidation class

A useful rule for custom components is:

| Change | Typical invalidation |
| --- | --- |
| fill/stroke color, opacity, visual highlight | Paint |
| transform that does not alter retained measurement | Paint |
| font family/size/weight when it affects measurement | Layout |
| padding/minimum/control size | Layout |
| child structure or availability that changes layout participation | Layout / retained structural reconciliation |
| animation of a purely visual scalar | Paint |
| animation of a measurement/placement scalar | Layout |

The table describes the stable design intent; the component/style implementation remains responsible for selecting the exact retained invalidator appropriate to its owner.

## Public reference map for this chapter

| Area | Current source |
| --- | --- |
| root theme tokens and invalidation classification | [`include/nativeui/theme.hpp`](../include/nativeui/theme.hpp) |
| lexical inheritable overrides | [`include/nativeui/style_scope.hpp`](../include/nativeui/style_scope.hpp) |
| common typed widget state/style resolution | [`include/nativeui/style.hpp`](../include/nativeui/style.hpp) |
| widget-family style recipes | [Widget style reference](v1-widget-style-reference.md) |
| drawing/path/paint style | [`include/nativeui/paint.hpp`](../include/nativeui/paint.hpp), [`include/nativeui/paint_style.hpp`](../include/nativeui/paint_style.hpp), [`include/nativeui/path.hpp`](../include/nativeui/path.hpp) |
| runtime shaders and per-instance bindings | [`include/nativeui/shader.hpp`](../include/nativeui/shader.hpp) |
| text presentation | [`include/nativeui/text.hpp`](../include/nativeui/text.hpp) |
| image/vector presentation | [`include/nativeui/image.hpp`](../include/nativeui/image.hpp), [`include/nativeui/svg.hpp`](../include/nativeui/svg.hpp) |
| retained invalidation / damage accumulation | [`include/nativeui/invalidation.hpp`](../include/nativeui/invalidation.hpp), [low-level Tree runtime](v1-low-level-tree-runtime.md) |
| retained animation | [`include/nativeui/animation.hpp`](../include/nativeui/animation.hpp) |

This table is navigation to currently implemented public sources. Current `main` public/package contracts and exact-head qualification are authoritative; implementation-adjacent headers should not be advertised to normal consumers unless they are part of that validated surface.

## Review checklist for presentation code

For presentation-related changes, verify at least:

- theme/style state remains instance-owned and does not introduce mutable globals/current-UI singletons;
- lexical style inheritance is deterministic and does not absorb per-component layout/model/callback state;
- combined interaction states resolve predictably;
- geometry-affecting style changes request layout rather than paint only;
- paint-only changes do not unnecessarily force whole-tree layout;
- custom paint code does not leak drawing state outside its retained paint scope;
- application contracts do not expose renderer/platform implementation objects;
- animation invalidation matches the property being animated;
- animation handles/callbacks cannot intentionally outlive their owning retained target;
- reduced-motion behavior follows the owning animation context rather than a separate timer path;
- exception/reentrancy claims are checked against the landed T125/T130 retained-safety contracts and current exact-head tests.

## T122 closeout boundaries

The safety and freeze assumptions that originally bounded this chapter have changed:

- T123/T124 and related binding follow-ups are complete, so state/binding wording can now be reconciled against landed behavior;
- T125/T130 are complete, so retained callback/lifecycle/layout/paint/teardown exception guarantees no longer need to be described as pending;
- T069 is deprecated as a standalone public-API freeze gate; current validated public/package contracts on `main` are authoritative;
- T070 is closed as not planned without delivering the reference application/guide; T133–T137 remain the separate open follow-up track for that work;
- T071 is closed as not planned; release/readiness wording follows the active repository validation policies and exact-head evidence instead of waiting on that retired gate;
- T068 native accessibility bridges remain deferred to NativeUI 1.2.

T122 completion should therefore focus on cross-document consistency, navigation, examples and release-facing accuracy rather than preserving historical blocker wording.
