# NativeUI 1.0 rendering, styling and animation

This chapter documents the stable NativeUI 1.0 presentation model that is already implemented: UI-owned theme values, lexical style scopes, typed widget style resolution, retained invalidation, custom painting and instance-owned animation. It deliberately avoids freezing backend/private drawing details or exception guarantees that are still owned by active pre-1.0 blockers and the T069 API freeze.

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

The final `State<T>`/`Binding<T>` notification, recursive-write, callback-exception and subscription-lifetime contract remains owned by T123/T124 and T069. This chapter documents only the style result and invalidation boundary, not unfinished observer internals.

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

### Backend boundary

Do not make direct Skia types, canvases or backend objects part of application/component contracts. T069 owns the final mechanical public-header and backend-neutral API inventory. Until that freeze closes, the stable rule is the abstraction boundary: normal consumers paint through NativeUI, while renderer/platform objects remain implementation details.

The active T130/T125 safety work also owns the final strong wording for paint/lifecycle/callback exception recovery. This documentation does not promise behavior beyond the currently qualified retained contract while those blockers are open.

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

The final callback-throw/reentrancy recovery wording remains subject to the active safety closeout and should not be inferred from this overview.

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
| widget-family style recipes | `include/nativeui/*_style.hpp` families |
| drawing/path/paint style | [`include/nativeui/paint.hpp`](../include/nativeui/paint.hpp), [`include/nativeui/paint_style.hpp`](../include/nativeui/paint_style.hpp), [`include/nativeui/path.hpp`](../include/nativeui/path.hpp) |
| text presentation | [`include/nativeui/text.hpp`](../include/nativeui/text.hpp) |
| image/vector presentation | [`include/nativeui/image.hpp`](../include/nativeui/image.hpp), [`include/nativeui/svg.hpp`](../include/nativeui/svg.hpp) |
| retained animation | [`include/nativeui/animation.hpp`](../include/nativeui/animation.hpp) |

This table is navigation to currently implemented sources, not the final authoritative 1.0 API inventory. T069 owns the final freeze and may narrow which implementation-adjacent headers are advertised directly to normal consumers.

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
- exception/reentrancy claims are checked against the final safety closeout rather than guessed while T125/T130 remain active.

## Freeze boundaries before T122 completion

This chapter is a bounded T122 documentation slice, not final 1.0 freeze evidence. T122 completion still requires reconciliation with:

- T125/T130 for final retained callback/lifecycle/paint exception guarantees;
- T123/T124 for final State/Binding notification and lifetime semantics used by state-backed style scopes;
- T069 for the exact supported public-header/API inventory and backend-neutral drawing exposure;
- T070 for the canonical copy-pasteable Getting Started/reference application;
- T071 for release-candidate policy and release-readiness wording.

Until those tickets close, this document describes stable architecture and already-delivered presentation behavior without promoting unresolved implementation details into promised public API.
