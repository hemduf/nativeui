# NativeUI roadmap

**Updated:** 2026-09-21

NativeUI is a reusable C++20 retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- finish the current merge-near ticket before widening source work;
- behavior/configuration changes use local RED -> GREEN -> REFACTOR and coherent published batches;
- code-changing tickets require exact-head normal/path qualification and the complete `CODE_REVIEW.md` record with no Blocking/Important finding;
- keep implementation PRs Draft while executable source/tests/build/workflows change;
- Draft -> Ready is reserved for a frozen candidate and triggers T042/T052 final qualification;
- any executable source/test/build/workflow change after final qualification invalidates the candidate; pure completion/project-state documentation does not;
- NativeUI-owned targets compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- every completion cycle synchronizes the issue, `CONTEXT.md`, this roadmap and relevant automation status;
- never place personal information in tickets, source, tests, examples, fixtures or generated metadata.

## Current execution snapshot

### T130 — Done

**T130 / issue #291 / PR #408 merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`.**

Frozen head `4c40b881fa13ea0f9839ae415059744f6107c62d` closed lifecycle/layout/paint/native construction and teardown exception safety. Normal/path CI, T042 and T052/T051 were green; final review reported zero Blocking/Important findings.

### T125 — Done

**T125 / issue #286 / PR #382 merged as `401d73983c2103fae1e48c1984a982277088e060`.**

Frozen head `da4386626837b8cedf9d7f17bc6e8b150aa99921` closed retained dispatch/reconciliation/cancellation exception safety and pending-work preservation on top of T130. Normal/path CI, T042 and T052/T051 were green; final self and independent reviews reported zero Blocking/Important findings.

### T174 — Done

**T174 / issue #409 / PR #410 merged as `1ef326494ce00d215c1211ad0cde2437e3ffadbb`.**

Frozen exact head `e6d747961a2fd39760f5703748c10440f8fb0efa` delivered the final pre-freeze raw-key fallback surface:

- `UI::set_key_down_handler(std::function<EventResult(const InputEvent&)>)`;
- per-`UI`/`Tree` ownership only, with no mutable global/singleton/`thread_local` state;
- framework KeyDown policy and Command normalization remain authoritative before the fallback;
- handled focused/ancestor routes suppress fallback and no sibling broadcast is introduced;
- only raw `KeyDown` is eligible; Command-mapped chords are never double-delivered;
- active callable lifetime survives re-entrant clear/replacement;
- non-empty installation allocates before publication, preserving the prior handler on allocation failure;
- fallback exceptions reuse T125's canonical dispatch unwind/reconciliation recovery without duplicate guards;
- feature example/self-test covers normal routing and throw -> catch -> later dynamic reconciliation.

Exact-head qualification is green for CI `35154523714`, T050 `35154524036` and T066 `35154523694`. Final T042 Lifecycle Stress `35156481983` passed Linux ASan+UBSan, Linux X11, macOS and Windows. T052 v0.1 Release Gate `35156481846` passed the release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 performance/allocation benchmark. Self review `5228886765` and independent frozen-head review `5228890731` report zero Blocking/Important findings; the historical Blocking thread is resolved/outdated and privacy review is clean.

### T175 — Done

**T175 / issue #428 / PR #429 completed from frozen executable head `d5c23ae534c26b2b7400784fc4e5d37f853397c1`.**

Delivered contract:

- `InputType::ContextMenu` is appended without renumbering existing public input values;
- Pugl's normalized secondary-button press (`button == 1`) maps to ContextMenu on every pinned backend, while secondary release, middle and extra buttons remain ignored;
- context-menu delivery uses pointer hit testing and normal ancestor bubbling without moving keyboard focus;
- any active pointer capture is cancelled first with `PointerCancel`, and context-menu handlers cannot establish replacement capture;
- capture/interaction guards recover exactly across re-entrant routing and exceptions;
- primary press/release, multi-click and drag behavior remains unchanged;
- dedicated routing/fault tests, explicit Pugl button-translation vectors and the `t175_context_menu --self-test` feature example cover the shipped behavior.

Exact-head normal/path qualification is green for CI `35343703682`, T050 `35343703732`, T060 `35343703732`, T064 `35343703852`, T065 `35343703617`, T072 `35343703816` and Package Contracts `35343703787`. Final T042 Lifecycle Stress `35345455301` and T052 v0.1 Release Gate `35345455659` are green. Final review reports zero remaining Blocking/Important findings.

### T073 — Done (post-1.0 rendering foundation)

**T073 / issue #156 / PR #420 merged as `5c769749f280f18f60e8176ec5eadbc38881acc7`.**

Frozen exact head `63bc82c6e2fd3db0aa6aae06456e51c36da33bfc` introduces backend-neutral `ui::Brush` values for solid colors, linear gradients and radial gradients, with one shared Painter fill-materialization seam and Canvas/Painter Brush fill overloads.

Delivered contract includes:

- exact transparent-solid moved-from semantics, including self-move assignment;
- non-allocating/non-throwing Color construction, moves and destruction by representation contract;
- strong copy-assignment guarantee under deterministic allocation failure;
- owned gradient logical state with independent multi-UI lifetime behavior;
- preserved transform, clip, invalid-gradient, opacity and blend semantics;
- no mutable production global/singleton/`thread_local` state and no backend resource creation during Brush construction;
- isolated public-header coverage plus radial-circle and linear-Path golden coverage.

Exact-head normal/path qualification passed CI #1828, T044 #267 and T066 #324. Final T042 Lifecycle Stress #834 passed Linux ASan+UBSan, Linux X11, Windows and macOS. Final T052 v0.1 Release Gate #558 passed the release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Final review `5233819886` reports zero remaining Blocking/Important findings.

T073 remains a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path. T069 continues to own the v1 public-surface audit/freeze against the current `main` state.

### T074 — Done (post-1.0 rendering foundation)

**T074 / issue #157 / PR #422 merged as `36425b5e0c96de943a1158ed006e027f9a28ff09`.**

Frozen executable head `0566b25d96cc98a61ef4ba7dca4a8d97549e8a77` extends the T073 backend-neutral Brush seam to stroked vector primitives without introducing a second paint-source path.

Delivered contract includes:

- Painter and Canvas Brush + PaintOptions overloads for rounded-rect/rect strokes, Paths, lines and arcs;
- existing Color stroke APIs remain source-compatible and delegate through the same Brush materialization seam;
- legacy round caps for line/arc and existing Path StrokeStyle cap/join/miter behavior are preserved;
- gradient sampling remains in shared Painter-local coordinates across multi-segment Paths rather than remapping per segment;
- opacity and blend are applied once through the existing T073 paint materialization path;
- no mutable global/singleton/`thread_local` state, registry, cache or persistent backend resource is introduced;
- dedicated independent Path-stroke golden coverage plus `t074_brush_strokes` interactive example/self-test.

Exact-head normal/path qualification passed CI #1842, including Linux ASan+UBSan, Linux X11, macOS and Windows, plus T044 #285 and T066 #336. Final T042 Lifecycle Stress #836 and T052 v0.1 Release Gate #560 both completed successfully on the frozen executable candidate. Final independent review `5236179950` reports zero remaining Blocking/Important findings and no unresolved review threads remain.

T074 is a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path. T069 must account for the current Brush stroke surface when freezing the public API exposed by `main`.

### T075 — Done (post-1.0 rendering foundation)

**T075 / issue #158 / PR #421 merged as `7cd37ef7a2e23b2e3c69880174f60f9578edbbc1`.**

Frozen exact head `adf486a8aad6f7a94c1a512ed230ab40e7eec63f` extends the existing strict lexical `Painter::StateGuard` model with scoped Rect, rounded-Rect and `Path` clipping without adding a second public scope type or logical stack.

Delivered contract includes:

- `Painter::scoped_clip(Rect)`, `scoped_clip(Rect,float)` and `scoped_clip(const Path&)` all return the existing `StateGuard`;
- `StateGuard` is non-copyable and non-movable, with `noexcept` destruction and C++20 guaranteed-copy-elision factories;
- one Painter-private scope-frame begin/adopt/rollback path shares the existing save-depth/restore-floor invariant with `scoped_state()`;
- fallible Path/rounded preparation occurs before state publication and clip-application failure rolls back the just-entered frame;
- invalid, empty, inverted and non-finite clip geometry produces a balanced empty effective clip rather than unbounded drawing;
- rounded radius canonicalization is deterministic and capped at half the smallest rectangle dimension;
- transform capture/intersection semantics, legacy manual clipping interoperability, exception/early-return restoration and independent Painter isolation are covered deterministically;
- dedicated `t075_scoped_clipping` feature example/self-test is present.

Exact-head normal/path qualification passed CI #1836 plus T044 #275, T050 #156, T066 #332, T072 #351 and Package Contracts #265. Final T042 Lifecycle Stress #835 and T052 v0.1 Release Gate #559 both completed successfully on the frozen head, including Linux ASan+UBSan, Linux X11, Windows/macOS final qualification, clean package bootstraps and exact-head T051 benchmark. Final review `5233924873` reports zero remaining Blocking/Important findings and there are no unresolved review threads.

T075 is a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path. T069 must nevertheless account for the current scoped-clipping public Painter surface when freezing the v1 API exposed by current `main`.

### T076 — Done (post-1.0 rendering foundation)

**T076 / issue #159 / PR #425 merged as `aea479af629b30ba21bb1ea7a7553b77b9717dd5`.**

Frozen executable head `f7ad7c2dea7d2077fc9060c069218f8480cb95ea` adds hard-bounded group compositing through the existing `Painter::StateGuard` and `PaintOptions` surface.

Delivered contract includes:

- `Painter::scoped_layer(Rect, PaintOptions)` returns the existing non-copyable/non-movable `StateGuard`; no `LayerScope`, duplicate `LayerOptions` or public backend type is added;
- finite layer bounds are enforced by a real hard clip captured under the creation transform before the private `saveLayer`, so Skia's layer bounds remain only a sizing hint;
- one logical guard may own multiple private backend frames through entry-depth bookkeeping while preserving the shared Painter restore-floor invariant;
- opacity and blend apply exactly once to the composed group rather than per child;
- empty, inverted and non-finite bounds produce balanced empty clip-only scopes and never open a backend layer;
- deterministic partial-entry failure after the hard clip or after `saveLayer` rolls back to the exact prior Painter/SkCanvas stack, and later drawing remains usable;
- no mutable global/singleton/`thread_local` layer state or persistent offscreen cache is introduced;
- nested layer/clip/state/manual-save, transform capture, exception/early-return unwind and two-Painter isolation are covered by the feature self-test and Core sanitizer runtime coverage.

Exact-head normal qualification passed CI #1862 on Linux X11, Linux ARM64, Linux ASan+UBSan, Windows and macOS. Final T042 Lifecycle Stress #839 and T052 v0.1 Release Gate #563 passed on the frozen head, including clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Final review `5237620829` reports zero remaining Blocking/Important findings.

T076 is a NativeUI 1.1/post-1.0 rendering foundation and is not on the v1 critical path.

### T077 — Done (post-1.0 effects foundation)

**T077 / issue #160 / PR #426 merged as `58fbe58d3d0535a321d2b81ada685cdc40d5d218`.**

Frozen exact head `b310fa5f3fe6f51494e2a0b8f4e37e1269577b0f` adds the first small backend-neutral `ui::Effect` value and bounded Gaussian blur through the existing `Painter::StateGuard` / `PaintOptions` layer surface.

Delivered contract includes:

- allocation-free/noexcept Gaussian Effect construction, copy/move/assignment/destruction with exact independent per-axis sigma canonicalization to `[0,64]`;
- `Painter::scoped_layer(Rect, const Effect&, PaintOptions)` with no second public scope/options abstraction;
- separate hard source and output clips so the complete composed layer is blurred once without clipping the halo at restore;
- transparent-edge Gaussian sampling, asymmetric/one-axis blur and zero/zero parity with the T076 unfiltered layer path;
- conservative device support through outward source mapping plus pinned Skia forward image-filter bounds, with fail-closed large/non-finite geometry;
- filtered `saveLayer(nullptr, ...)` correctness independent from optional backend bounds hints;
- deterministic rollback after materialization/output-clip/saveLayer/source-clip seams, strict LIFO nesting and destroy-A/continue-B isolation;
- direct Skia raster oracles, golden coverage, feature self-test and standalone/embedded native GPU smoke.

Exact-head normal/path qualification passed CI #1893, Package Contracts #310, T050 #187 and T072 #381. Final T042 Lifecycle Stress #840 passed Linux ASan+UBSan, Linux X11, Windows and macOS. Final T052 v0.1 Release Gate #564 passed release contract, clean Linux/macOS/Windows bootstraps and the exact-head T051 benchmark. Final review comment `5726825909` records zero remaining Blocking/Important findings.

T077 and T078 remain outside the v1 critical path. **T078 / #161 / PR #431 is Done**, merged as `da4ba30bcc9453029aee689b56543d4a74be6db3` from frozen executable head `23f0e0e08b94be5c82bf2896104643768a6d0e96`. It adds immutable DropShadow/DropShadowOnly effects plus the retained published visual-outset invalidation contract consumed by later partial-rendering work. Exact-head CI #1916, Package Contracts #332, T050 #209 and T072 #401 passed; final T042 Lifecycle Stress #845 and T052 v0.1 Release Gate #569 also passed, including the exact-head T051 benchmark.

### Active pre-freeze work

- **T069 / #81 — Deprecated:** retired as a standalone v1 freeze gate; current `main` public/package contracts and exact-head qualification are authoritative. It is not a dependency or merge gate for T079 or subsequent 1.1 shader work.
- **T068 / #80 / PR #241 — deferred to 1.2:** native accessibility bridges remain outside the v1 critical path.

## Current dependency frontier

```text
state/safety:
  T123(done) -> T138(done) -> T139(done) -> T140(done) -> T141(done) -> T124(done)
  T123(done) -> T127(done)
  T125(done)
  T126(done)
  T128(done)
  T129(done)
  T130(done)
  T131(done)
  T132(done)

pre-freeze public additions:
  T173(done) -> T174(done)
  T175(done) -----------------------------------------> current public/package surface

v1 critical path:
  T070 -> T122/docs closeout -> T071 -> v1.0.0
  T049(done) -----------------------------------------> T071
  T044(done) -----------------------------------------> T071

post-1.0 / later-release work already landed:
  T073(done) -----------------------------------------> NativeUI 1.1 foundation
  T074(done) -----------------------------------------> NativeUI 1.1 foundation
  T075(done) -----------------------------------------> NativeUI 1.1 foundation
  T076(done) -> T077(done) -> T078(done) -> T094(blocked on T071) -> NativeUI 1.1 effects
  T079(done) -> T080(done) -> T081(done) -> T082(done) -> T083(done) -> T084(done) -> T085(done) -> T086(done) -> NativeUI 1.1 shaders
  T098(done) ---------------------------------------------------------------> T086(done)
  T068 ----------------------------------------------> 1.2
```

## Milestone status

### Milestone 0 — Baseline hardening

**Complete.** Core/state tests, retained lifecycle foundations, public-header split and invalidation foundations are established; unapproved NativeUI-owned compiler warnings are Blocking.

### Milestone 1 — Layout system

**Complete.** Constraints, alignment/distribution, flex, grid, scroll layout/state and clipping/overflow foundations are delivered. T130 hardens layout publication/recovery under exceptions.

### Milestone 2 — Input, focus and gestures

**Complete for the v1 input surface.** Event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop are delivered. T173 completed public ASCII A-Z key exposure; T125 finalized exception-safe dispatch/reconciliation semantics; T174 provides the per-UI fallback for otherwise-unhandled raw KeyDown shortcuts; T175 adds first-class right-button ContextMenu delivery with capture-safe routing.

### Milestone 3 — Rendering and graphics

**Complete for v1.** Transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests are delivered. T130 guarantees Painter/SkCanvas unwind balance and failed-frame recovery. T073's generic Brush fill foundation, T074's Brush stroke extension, T075's strict lexical scoped-clipping API, T076's hard-bounded scoped layer compositing, T077's bounded Gaussian Effect layer, T078's drop-shadow/published-visual-outset invalidation contract, T083–T086's ImageTexture mapping/tiling/filtering/mipmap/local-transform value surface, and T098's shared affine Transform2D/Painter tracking are merged on `main` for the post-1.0/1.1 rendering line without changing the v1 critical path.

### Milestone 4 — Text system

**Complete.** Text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges are delivered.

### Milestone 5 — Standard widget set

**Complete for the v1 standard-widget scope.** Button, Checkbox/Radio, Slider/RangeSlider, ProgressBar/Meter, ScrollView, ComboBox/PopupMenu and ListView/Tabs are delivered.

### Milestone 6 — Styling, theme and animation

**Complete.** T037–T040 provide typed Theme/style state, retained StyleScope inheritance and deterministic animation/reduced-motion behavior. T128 hardens exceptional animation progress/recovery.

### Milestone 7 — Platform and embedded robustness

**Complete for the current v1 platform surface.** T043 resize/scale, T044 pointer capture, T053 macOS Objective-C runtime identity, T060 Application ownership, T064 DesktopServices, T065 Dispatcher, T066 window controls, T072 Linux D-Bus and T175 normalized secondary-button ContextMenu delivery are delivered. T125/T126/T128/T130/T132 close the relevant failure boundaries.

### Milestone 8 — Packaging, virtualization, overlays and release convergence

**Ready for the final API freeze.** Delivered foundations include T047/T048 packaging, T049 gallery, T050 inspector, T051/T052 qualification, T054/T056/T057 helpers/resources, T058 dynamic composition, T061/T062/T063 overlay/Tooltip/Dialog, T067 virtualized ListView and the completed safety series T123–T132 plus T138–T141. T174 is also resolved before freeze.

The remaining v1 sequence is:

1. validate T070 reference application/Getting Started against the current public/package surface;
2. complete explicitly scheduled v1 documentation closeout including T122 where applicable;
3. run T071 validation/release-only on one exact RC SHA.

## Completed safety and pre-freeze closeouts

- **T123 / #281:** deterministic lifetime-safe `State<T>` observer/reentrancy/throw semantics.
- **T124 / #282 + T138–T141:** stable `Binding<T>` lifetime/value contract and migration of retained/stateful consumers.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety and pending-work preservation.
- **T126 / #287:** DesktopServices exceptional completion/native boundaries.
- **T127 / #288:** `ScrollState` lifetime/reentrancy and retained-consumer safety.
- **T128 / #289:** Dispatcher accepted-work preservation and Animation exceptional recovery.
- **T129 / #290:** lifetime-safe retained invalidation callbacks.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction failure safety.
- **T132 / #294:** failure-safe standalone close lifecycle-control deferral.
- **T173 / #401:** complete public ASCII A-Z key exposure and routing coverage.
- **T174 / #409:** per-UI fallback for unhandled raw KeyDown shortcuts on final T125 semantics.
- **T175 / #428:** first-class ContextMenu input delivery from Pugl's normalized secondary button, with focus invariance and capture-safe exception recovery.

## Completed post-1.0 foundations

- **T073 / #156:** backend-neutral generic Brush fill painting with deterministic move/copy/failure semantics and shared Painter materialization.
- **T074 / #157:** Brush + PaintOptions stroke support for rounded rectangles, Paths, lines and arcs with preserved Color/style semantics and shared Painter-local sampling.
- **T075 / #158:** strict lexical scoped clipping for Rect, rounded Rect and Path through the existing `Painter::StateGuard` stack model.
- **T076 / #159:** hard-bounded group compositing through `Painter::scoped_layer(Rect, PaintOptions)` with one logical `StateGuard`, exact multi-frame restore/rollback, group opacity/blend and empty invalid-bound scopes.
- **T077 / #160:** allocation-free backend-neutral Gaussian `Effect` values and hard-bounded filtered layers with conservative Skia-derived output support, exact rollback and native GPU qualification.
- **T078 / #161 / PR #431:** allocation-free backend-neutral DropShadow/DropShadowOnly effects, exact continuous `VisualOutset` semantics, per-node successfully published visual bounds, old/new paint invalidation, layout rollback/reentrancy safety and bounded no-throw dirty publication; merged as `da4ba30bcc9453029aee689b56543d4a74be6db3` after CI #1916, Package #332, T050 #209, T072 #401, T042 #845 and T052 #569.
- **T079 / #162:** explicit backend-neutral SkSL `ShaderProgram` compilation with immutable sharing, deterministic diagnostics, strong failure publication guarantees, pinned-Skia source-size safety, isolated fault seams and installed-package shader linkage validation. Merged through PR #427 as `99762beb9bbb42f9f15bebcf318b60e84d746fd1`.
- **T080 / #164:** typed SkSL uniform reflection/binding with NativeUI-owned stable descriptors, bounded profile rejection, deterministic zero-initialized per-instance storage, exact backend `int` packing, allocation-free/noexcept setters, independent copy/move/inert semantics, partial-reflection fault recovery and zero-recompile instrumentation. Original PR #430 was followed by post-merge correction PR #432, squash-merged as `3fdaacf30fdf1a67c936cb97ee7f889c8eadd49e` from frozen executable head `018b76c50e3ec2a2eac893a618aedab36976b27d`; the correction hardens exact vector/Color byte extents, contiguous/full pinned-Skia uniform-block validation and complete allocation-proof coverage. CI #1929, Package Contracts #345, T042 #847 and T052 #571 are green.
- **T081 / #165 / PR #434:** immutable ShaderInstance -> Brush snapshots with owned uniform bytes, canonical inert conversion, preserved Brush value semantics, transient Skia runtime-shader materialization, zero source recompilation, pinned `layout(color)` semantics, deterministic materialization-failure recovery, shader Brush golden coverage and Linux/macOS/Windows native GPU smoke. Squash-merged as `ae4280b6e52c6b68ed49b230883965e632e0e9f8` from frozen executable head `2c7749583fa698d910ee7e06e4f40ef2f969a2fd`. CI #1941, Package Contracts #352, T050 #219, T072 #408, T042 #848 and T052 #572 are green.
- **T082 / #166 / PR #435:** bounded shader-child reflection/binding/composition with source-order NativeUI-owned descriptors, immutable shared child Brush snapshots, cached depth and a hard maximum depth of 16, recursive Color/LinearGradient/RadialGradient/shader Brush materialization, null-child transparent-black semantics, direct-vs-child parity, nested failure recovery, dedicated two-level/deep goldens and native nested-shader GPU smoke. Squash-merged as `2c58657c4592419b1c607fda9f41a258b80fb174` from frozen executable head `fafb3a586ff5bacbd0f69146cfb73884ac057b5c`. CI #1952, Package Contracts #363, T050 #230, T072 #418, T042 #849 and T052 #573 are green. T083 / #167 is Ready; retained per-view shader caching remains deferred to T097.
- **T083 / #167 / PR #436:** backend-neutral `ImageTexture` is now a normal Brush source with full-image and strict fully-contained fractional source-subrect mapping in Painter-local coordinates, Clamp+Linear sampling isolated to the selected crop, eager decoded Image backing, preserved Brush copy/move/self-move/noexcept/strong-assignment invariants, zero-allocation logical conversion, deterministic materialization/mapping failure recovery, shader-child integration, two-view lifetime isolation, sub-texel oracle coverage and Linux/macOS/Windows native GPU smoke. Squash-merged as `596ed2b32e6f996eb5902ed0eb4ae2eb3f0ffd26` from frozen executable head `96a1175cc61732b81523171a4ab84137430dfafd`. CI #1970, Package Contracts #379, T050 #244, T072 #431, T042 #850 and T052 #574 are green. Retained per-view backend texture/shader caching remains deferred to T097.
- **T084 / #168 / PR #444:** per-axis `TextureTileMode::{Clamp,Repeat,Mirror,Decal}` for `ImageTexture`, with exact floor/parity periodic semantics for negative coordinates, independent X/Y state, unchanged T083 Clamp/Clamp behavior, strict integer/fractional selected-source isolation including non-zero-origin crops, allocation/decode/materialization-free `noexcept` tile-mode mutation, shared-Image instance isolation, all 16 mixed-axis render combinations, Painter-transform coverage and Linux/macOS/Windows native GPU smoke. The final closeout also corrected the inherited v0.1 release-note Skia tag so T052 matches the current `chrome/m153` dependency pin. Squash-merged as `bff79cff141a0592b90fa7abd87e950ce0c3ca0d` from frozen executable head `120a84705a1591b13503df0c91d9f342007540e1`. CI #1994, Package Contracts #398, T050 #262, T072 #445, T042 #853 and T052 #577 are green; seven distinct review passes report zero remaining Blocking/Important findings.
- **T085 / #169 / PR #446:** backend-neutral `TextureSampling` adds `TextureFilter::{Nearest,Linear}` and `TextureMipmap::{None,Nearest,Linear}` to `ImageTexture`, defaulting to Linear/None. Sampling mutation is `noexcept` and proven allocation/decode/materialization-free; magnification stays on level 0, minification supports nearest-level and trilinear mip policies, and selected integer/fractional source subrects stay isolated across Clamp/Repeat/Mirror/Decal and both filters. Repeated draws do not re-decode Image data; backend/mipmap resources remain transient and context-local pending T097 caching. Shared-Image instances remain independent, deterministic materialization failure recovers, and the dedicated example/self-test plus standalone/embedded native smoke pass on Linux X11, Windows and macOS. Squash-merged as `64744044eefc4225607339890ff221756379e366` from frozen executable head `2172372e73a8edecb1dc6832dc00b4e2f815188f`. CI #2007, T085 Texture Sampling #4, T042 #854 and T052 #578 are green; seven distinct review passes and the mandatory record report zero remaining Blocking/Important findings.
- **T098 / #183 / PR #447:** shared affine transform tracking is complete, squash-merged as `abdce33618e6c3e61dde41e36e668213c7bbc635` from frozen executable head `8db9edd05cc9014698cb48cd0c715527a6a2cd98`. `Transform2D` now provides deterministic rotation, mapping, composition and normalized double-precision inversion; `Painter::current_transform()` tracks logical local-to-scene state across manual saves, StateGuard scopes, clips and layers while excluding device scale. Invalid/non-finite/composed-overflow mutations are atomic no-ops, near-singular finite transforms remain drawable, and deep save-history allocation is prepared before backend publication with deterministic fault/recovery coverage. CI #2051, T050 #309, Package Contracts #427 and T072 #473 are green; final T042 Lifecycle Stress #856 and T052 v0.1 Release Gate #580 are green, including the exact-head T051 benchmark. Ten review passes plus the final mandatory record report zero remaining Blocking/Important findings.
- **T086 / #170 / PR #449:** ImageTexture local affine transforms are complete, squash-merged as `79e48ae7812290d8ebb1be8df80e6a93c701207f` from frozen executable head `b07d516e65dbbf81ee07c5e3776753df4da6e40a`. `ImageTexture::set_transform()`/`transform()` retain exact raw affine state with allocation/decode/backend-free `noexcept` mutation, semantic validity delegates exclusively to T098, invalid transforms preserve logical texture state while rendering transparently, and valid Brush snapshots remain independent from later texture mutation. Backend materialization composes `texture_to_local * (source -> destination)`, preserves T084 transformed tile domains and selected-source isolation, and keeps T085 magnification/minification sampling behavior coherent, including the final full-source Repeat/Mirror fast path with Decal excluded to preserve transparent destination domains. Exact-head CI #2061, Package Contracts #434, T050 #319, T072 #479, T085 Texture Sampling #10 and T086 Texture Transform #6 are green. Final T042 Lifecycle Stress #857 and T052 v0.1 Release Gate #581 are green, including clean Linux/macOS/Windows bootstraps and exact-head T051. Twelve review passes plus the final mandatory record report zero remaining Blocking/Important findings.

## Release policy

T069/#81 is deprecated and no longer acts as a standalone v1 freeze gate. The current backend-neutral public headers/package contracts plus exact-head T070/T122/T071 validation are authoritative for v1 closeout. T073, T074, T075, T076, T077, T079, T080, T081, T082, T083, T084, T085, T086 and T098 are merged as later-release rendering foundations outside the v1 critical path. T078 remains Ready on the effects line. Retained per-view shader/resource caching remains deferred to T097. T070 validates the reference application against the frozen surface. T071 is validation/release-only: defects found there return to a focused canonical ticket, are merged first, and then a new exact release-candidate SHA is selected. T068 remains outside the v1 critical path and is targeted for NativeUI 1.2.