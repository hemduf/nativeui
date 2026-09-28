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

[`include/nativeui/theme.hpp`](../include/nativeui/theme.hpp) defines the current typed theme model. A `Theme` groups:

- `ThemePalette` — background/surface/text/border/accent/focus and shared control interaction colors;
- `ThemeTypography` — family/fallbacks, sizes, weights and slant;
- `ThemeSpacing` — shared spacing tokens;
- `ThemeRadii` — shared corner-radius tokens;
- `ThemeControlMetrics` — standard-control geometry/paint metrics.

`default_theme()` provides the default value. Theme data is ordinary instance-owned value data; changing one UI's theme does not mutate unrelated UIs.

### Theme invalidation

Theme replacement distinguishes changes that only require repaint from changes that can alter measurement/layout. `classify_theme_change(...)` returns:

- `ThemeInvalidation::None` when the effective value is unchanged;
- `ThemeInvalidation::Paint` for palette/radius and paint-only metric changes;
- `ThemeInvalidation::Layout` when typography, spacing or measurement-affecting control metrics change.

Application/component code should preserve this distinction. A purely visual change should not be escalated into unnecessary layout work, while typography or measurement-token changes must not be treated as paint-only.

## Lexical style scopes

[`include/nativeui/style_scope.hpp`](../include/nativeui/style_scope.hpp) provides typed inheritable overrides for one retained subtree.

`StyleScopeOverrides` mirrors the inheritable theme families with optional fields. Empty fields inherit the nearest outer value. When scopes are nested, values resolve outer-to-inner and the nearest supplied field wins independently.

A style scope intentionally cannot encode arbitrary component state. Per-instance width/height constraints, Flex/Grid placement, scroll position, callbacks, models and availability are not inheritable style properties.

Two retained forms are supported by the current implementation:

- an immutable `StyleScopeOverrides` value for the retained lifetime;
- a `State<StyleScopeOverrides>`-backed form that can replace the effective scope on the UI thread and classify the resulting invalidation as none/paint/layout.

The `State<T>`/`Binding<T>` notification, recursive-write, callback-exception and subscription-lifetime safety work has landed through T123/T124 and the associated binding follow-ups. This chapter still focuses on the presentation result and invalidation boundary; detailed observable semantics live in [State and binding](v1-state-and-binding.md).

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

Custom components paint through NativeUI's retained paint callback and drawing abstractions. [`include/nativeui/paint.hpp`](../include/nativeui/paint.hpp), [`include/nativeui/paint_style.hpp`](../include/nativeui/paint_style.hpp) and [`include/nativeui/path.hpp`](../include/nativeui/path.hpp) contain the current drawing surface.

The current retained drawing model supports operations such as:

- filled/stroked rounded geometry;
- circles, arcs and lines;
- paths with fill/stroke styles;
- text using NativeUI text styles and measurement;
- transforms and clipping;
- linear/radial gradients and paint options;
- scoped drawing state so one component does not leak its transform/clip/save state into later paint work.

Application code should express drawing in logical coordinates and let the owning window/view handle framebuffer scale below the retained component layer.

## Brushes, effects and runtime shaders

The installed v1 headers expose higher-level fill/effect primitives in [`include/nativeui/paint_style.hpp`](../include/nativeui/paint_style.hpp) and explicit runtime-shader preparation in [`include/nativeui/shader.hpp`](../include/nativeui/shader.hpp).

`Brush` is the common fill source used by retained painting. It can snapshot:

- a solid `Color`;
- a `LinearGradient` or `RadialGradient`;
- an `ImageTexture`;
- a prepared `ShaderInstance`.

`PaintOptions` supplies bounded opacity plus the documented blend modes `SourceOver`, `Multiply`, `Screen` and `Plus`. Layer effects are explicit values: `Effect::gaussian_blur(...)`, `Effect::drop_shadow(...)` and `Effect::drop_shadow_only(...)`. The effect's `visual_outset()` reports the conservative visual expansion that retained invalidation/layout code can account for; effects do not create an application-owned renderer loop.

Runtime shaders use a two-stage contract:

1. `ShaderProgram::compile(sksl)` performs explicit compilation/resource preparation and returns owned diagnostics plus an immutable program;
2. `ShaderInstance` owns one mutable binding set for that program and is then snapshotted into a `Brush` for painting.

Compilation may allocate and is explicitly not an audio-real-time operation. Rendering does not compile shader source implicitly. Program reflection exposes supported uniform and child names/types through NativeUI-owned metadata. Numeric uniform setters are `noexcept` and do not allocate; binding a child `Brush` may allocate. Mutation of one `ShaderInstance` is ordinary UI/resource-preparation work and is not synchronized for concurrent writers.

Keep shader source compilation, image preparation and effect construction outside audio/DSP callbacks. Once prepared, use the resulting NativeUI values through `Brush`/retained painting rather than passing backend shader/compiler objects through application state.

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

Text styling and measurement are part of the NativeUI presentation layer and use logical sizes/families/fallbacks rather than platform-native widget typography. The current public source lives in [`include/nativeui/text.hpp`](../include/nativeui/text.hpp).

Image/SVG/resource loading is deliberately split from presentation ownership:

- the ResourceManager/resource layer owns lookup/loading/caching policy;
- image/vector abstractions represent content consumed by NativeUI rendering;
- components/widgets decide retained layout and paint placement;
- normal consumers should not acquire renderer-native image/canvas objects as application state.

See [Services, testing and limits](v1-services-testing-and-limits.md) for the resource-lifetime/threading boundary and [Packaging and CMake](v1-packaging-and-cmake.md) for binary-resource packaging.

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

[`include/nativeui/animation.hpp`](../include/nativeui/animation.hpp) defines the current instance-owned animation layer.

`AnimationContext` is UI-thread confined and uses the owning `Dispatcher` for time/wake scheduling; it does not introduce a second application event loop or a process-global animation registry.

The current model supports:

- tween animation with `Linear`, `EaseIn`, `EaseOut` and `EaseInOut` easing;
- spring animation using `SpringOptions`;
- `AnimationHandle` cancellation scoped to the owning animation context;
- explicit `AnimationInvalidation::Paint` versus `AnimationInvalidation::Layout`;
- an `AnimationInvalidationTarget` containing the retained paint/layout invalidation routes;
- reduced-motion mode.

Each active animation writes its value through the supplied callback and routes the declared invalidation through the retained target. The invalidation kind is therefore part of animation behavior rather than an optional convention left to every caller.

### Reduced motion

When reduced motion is enabled, the current animation layer converges animation values to their target without continuing normal time-based interpolation. New zero-duration/reduced-motion transitions likewise apply their target immediately through the retained invalidation path.

Use reduced motion as behavior, not as a cosmetic afterthought: application components should avoid starting a second independent timer/animation mechanism that bypasses the owning `AnimationContext` policy.

### Animation lifetime

Keep animation ownership tied to the retained UI/component lifetime. Cancellation and shutdown should not depend on a process-global handle table, and callbacks must not assume their target outlives the owner that registered them.

Callback/reentrancy recovery follows the landed retained safety contracts; this overview intentionally leaves their implementation mechanics to the dedicated state/lifecycle documentation and tests.

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
