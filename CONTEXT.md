# NativeUI compact recovery context

**Updated:** 2026-09-18

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, state, widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- NativeUI-owned source-tree targets compile with zero unapproved warnings and an empty default `NATIVEUI_ALLOWED_WARNINGS`;
- ordinary C++ callbacks may propagate only after NativeUI invariants are restored; foreign/native ABI callbacks contain C++ exceptions;
- destructor-driven retained/native teardown is no-throw and best-effort complete;
- retained invalidators intentionally allowed to outlive a component use lifetime-safe owner/identity semantics;
- accepted/deferred work is not silently lost, duplicated or converted to unsafe synchronous execution by failure;
- state machines crossing fallible work have explicit prepare/commit/recovery boundaries;
- partial native construction leaves no registered callback/native resource behind;
- top-level `UI`/window/view destruction from an active callback is deferred unless the complete caller chain proves self-destruction safety;
- `CODE_REVIEW.md`, `CI_POLICY.md`, exact-head tests, `CONTEXT.md` and `ROADMAP.md` are merge/Done gates for code-changing tickets;
- no personal information is placed in tickets, source, tests, examples, fixtures or generated metadata.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `hemduf/skia-builder` `chrome/m149`, forked from `olilarkin/skia-builder`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release for the normal dependency path; CI additionally validates NativeUI natively on ARM64 against the forked `skia-build-linux-arm64-gpu-release.zip` asset.

## Current baseline and critical path

### Safety frontier complete

The full T123–T132 pre-freeze safety series is Done. The two most recent blockers are merged:

- **T130 / #291 / PR #408** — lifecycle/layout/paint/native construction/teardown exception safety, merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a` from frozen head `4c40b881fa13ea0f9839ae415059744f6107c62d`.
- **T125 / #286 / PR #382** — retained dispatch/reconciliation/cancellation exception safety, pending-work preservation, dynamic enqueue failure recovery and transient native-borrow hardening, merged as `401d73983c2103fae1e48c1984a982277088e060` from frozen head `da4386626837b8cedf9d7f17bc6e8b150aa99921`.

Both completed normal/path CI, T042 Lifecycle Stress and T052/T051 final qualification with zero remaining Blocking/Important review findings.

### T174 is Done

**T174 / #409 / PR #410 merged as `1ef326494ce00d215c1211ad0cde2437e3ffadbb`.** Frozen exact head `e6d747961a2fd39760f5703748c10440f8fb0efa` resolves the last planned public input addition before the v1 freeze.

Delivered contract:

- `UI::set_key_down_handler(std::function<EventResult(const InputEvent&)>)` is per UI/Tree only;
- framework KeyDown policy, Command normalization and ordinary focused/ancestor routing retain precedence;
- only otherwise-unhandled raw KeyDown can reach the fallback;
- Command-mapped shortcuts never double-deliver to the raw fallback;
- KeyUp/TextInput/Composition/Tick/pointer/wheel/drop remain excluded;
- active callable lifetime survives re-entrant clear/replacement through stable shared ownership;
- installation allocates before publication; dispatch itself adds no heap allocation or extra tree walk;
- callback exceptions use T125's canonical unwind path and later successful dispatch reaches normal outermost reconciliation;
- no new scheduler, queue, native resource or mutable global/TLS state.

Exact-head normal/path qualification is green for CI `35154523714`, T050 `35154524036` and T066 `35154523694`. Final T042 `35156481983` is green on Linux ASan+UBSan, Linux X11, macOS and Windows. Final T052 `35156481846` is green for release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Reviews `5228886765` and `5228890731` are 0 Blocking / 0 Important; the historical Blocking thread is resolved/outdated and privacy review is clean.

### T073 is Done

**T073 / #156 / PR #420 merged as `5c769749f280f18f60e8176ec5eadbc38881acc7`.** Frozen exact head `63bc82c6e2fd3db0aa6aae06456e51c36da33bfc` delivers the generic backend-neutral Brush fill foundation targeted at the post-1.0/NativeUI 1.1 rendering line.

Delivered contract:

- `ui::Brush` represents Color, LinearGradient and RadialGradient without public backend types;
- Painter and Canvas fill rounded rectangles, circles and Paths through one Brush-compatible fill materialization seam;
- existing Color/gradient overloads remain source-compatible and share the same rendering path;
- Color construction, moves and destruction preserve the declared non-allocating/non-throwing representation contract;
- copy assignment uses prepare-then-no-throw-commit semantics and preserves the prior destination on allocation failure;
- moved-from and self-moved Brushes become deterministic transparent solid values;
- gradient logical storage is owned and remains safe across independent UI lifetimes;
- Brush construction creates no backend shader/resource and introduces no mutable global/singleton/`thread_local` state;
- transform, clip, invalid-gradient, opacity and blend behavior remains compatible with T019/T021.

Normal/path exact-head qualification is green for CI #1828, T044 #267 and T066 #324. Final T042 Lifecycle Stress #834 passed Linux ASan+UBSan, Linux X11, Windows and macOS. Final T052 v0.1 Release Gate #558 passed the release contract, clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Final review `5233819886` reports zero remaining Blocking/Important findings.

T073 is not on the v1 critical path. Because it is now present on `main`, T069's v1 public-surface audit must explicitly account for the current Brush-facing surface when deciding the frozen v1 API.

### T074 is Done

**T074 / #157 / PR #422 merged as `36425b5e0c96de943a1158ed006e027f9a28ff09`.** Frozen executable head `0566b25d96cc98a61ef4ba7dca4a8d97549e8a77` extends the T073 backend-neutral Brush paint-source seam to stroked vector primitives for the post-1.0/NativeUI 1.1 rendering line.

Delivered contract:

- Painter and Canvas accept `Brush + PaintOptions` for rounded-rect/rect strokes, Path strokes, lines and arcs;
- existing Color stroke overloads remain source-compatible and delegate to the same Brush path;
- legacy round caps for line/arc and existing Path `StrokeStyle` cap/join/miter behavior are preserved;
- a multi-segment Path shares Painter-local Brush coordinates; gradients are not remapped independently per segment;
- opacity and blend are applied exactly once through the existing T073 paint materialization seam;
- Brush values are consumed per draw without retained caller storage, persistent backend resources, registries, caches or mutable global/TLS state;
- dedicated compile/API, Color parity, cap/join/miter, radial line/arc, transform/opacity/blend, independent stroke golden and `t074_brush_strokes --self-test` coverage is present.

Normal/path exact-head qualification is green for CI #1842, including Linux ASan+UBSan, Linux X11, macOS and Windows, plus T044 #285 and T066 #336. Final T042 Lifecycle Stress #836 and T052 v0.1 Release Gate #560 are green on the frozen executable candidate. Final independent review `5236179950` reports zero remaining Blocking/Important findings and no unresolved review threads remain.

T074 is not on the v1 critical path. Because its stroke API is now present on `main`, T069's public-surface audit must explicitly account for the current Brush fill/stroke surface when deciding the frozen v1 API.

### T075 is Done

**T075 / #158 / PR #421 merged as `7cd37ef7a2e23b2e3c69880174f60f9578edbbc1`.** Frozen exact head `adf486a8aad6f7a94c1a512ed230ab40e7eec63f` delivers strict lexical scoped clipping through the existing `Painter::StateGuard` abstraction for the post-1.0/NativeUI 1.1 rendering line.

Delivered contract:

- Rect, rounded-Rect and arbitrary `Path` scoped clipping all return `Painter::StateGuard`;
- `StateGuard` is non-copyable and non-movable, with no-throw destruction and guaranteed-copy-elision factories;
- one private Painter scope-frame mechanism shares the existing save-depth/restore-floor state with `scoped_state()`;
- Path/rounded geometry preparation is completed before publication and clip-application failure rolls back the entered frame;
- invalid, empty, inverted and non-finite clip geometry becomes a balanced empty effective clip, never an unclipped fallback;
- rounded radius canonicalization is deterministic and bounded to half the smallest rectangle dimension;
- nested scoped/manual clipping, transform capture, early-return/exception restoration and two-Painter isolation are covered;
- `examples/features/t075_scoped_clipping.cpp` provides the required interactive example and deterministic `--self-test`.

Exact-head normal/path qualification is green for CI #1836, T044 #275, T050 #156, T066 #332, T072 #351 and Package Contracts #265. Final T042 Lifecycle Stress #835 and T052 v0.1 Release Gate #559 are green on the frozen head, including Linux ASan+UBSan, Linux X11, Windows/macOS qualification, clean package bootstraps and exact-head T051 benchmark. Final review `5233924873` records zero Blocking/Important findings and no unresolved review threads remain.

T075 is not on the v1 critical path. Because its Painter API is now present on `main`, T069's public-surface audit must explicitly account for the current scoped-clipping surface together with T073/T074.

### T076 is Done

**T076 / #159 / PR #425 merged as `aea479af629b30ba21bb1ea7a7553b77b9717dd5`.** Frozen executable head `f7ad7c2dea7d2077fc9060c069218f8480cb95ea` adds hard-bounded group compositing through the existing `Painter::StateGuard` and `PaintOptions` surface for the post-1.0/NativeUI 1.1 rendering line.

Delivered contract:

- `Painter::scoped_layer(Rect, PaintOptions)` returns the existing non-copyable/non-movable `StateGuard`; no `LayerScope`, duplicate `LayerOptions` or public backend type is introduced;
- finite layer bounds are enforced by a real clip captured under the creation transform before a private `saveLayer`, so backend bounds hints are never the correctness boundary;
- one logical `StateGuard` can own multiple private backend frames through entry-depth bookkeeping while preserving the shared Painter save-depth/restore-floor invariant;
- layer opacity and blend apply once to the completed group rather than once per child;
- empty, inverted and non-finite bounds are balanced empty clip-only scopes and do not open a backend layer;
- deterministic failure after the hard-clip frame or after `saveLayer` restores the exact prior Painter/SkCanvas stack and a later valid draw remains usable;
- nested layer/clip/state/manual saves, transform capture, early-return/exception unwind and two-Painter lifetime isolation are covered;
- no mutable global/singleton/`thread_local` layer state or persistent offscreen cache is introduced;
- `examples/features/t076_layers.cpp` provides the interactive example and deterministic `--self-test`, while the Core paint suite executes the bounded-layer rendering path under sanitizers.

Exact-head normal CI #1862 is green on Linux X11, Linux ARM64, Linux ASan+UBSan, Windows and macOS. Final T042 Lifecycle Stress #839 and T052 v0.1 Release Gate #563 are green on the frozen head, including clean Linux/macOS/Windows bootstraps and exact-head T051 benchmark. Final review `5237620829` records zero Blocking/Important findings and no unresolved review threads remain.

T076 is not on the v1 critical path.

### T077 is Done

**T077 / #160 / PR #426 merged as `58fbe58d3d0535a321d2b81ada685cdc40d5d218` from frozen head `b310fa5f3fe6f51494e2a0b8f4e37e1269577b0f`.**

Delivered contract:

- small allocation-free/noexcept backend-neutral Gaussian `ui::Effect` value with exact independent per-axis sigma canonicalization;
- effect-aware `Painter::scoped_layer(...)` reuses the existing `StateGuard` and `PaintOptions` abstractions;
- hard source clip remains inside the filtered layer while a separate finite device-space output clip preserves the halo;
- output support uses conservative outward source mapping plus Skia's forward image-filter bounds contract and fails closed on unrepresentable/non-finite geometry;
- zero/zero blur is the T076 no-op path; asymmetric one-axis blur and transparent-edge sampling are supported;
- group blur, opacity and blend each apply once to the complete composed source;
- deterministic partial-entry failure seams recover the exact Painter/SkCanvas stack and later operations remain usable;
- no mutable global/TLS cache/registry is introduced and independent Painter instances remain isolated;
- direct Skia raster oracles, golden/self-test, ASan/UBSan and Linux/Windows/macOS native GPU smoke are green.

Exact-head CI #1893, Package Contracts #310, T050 #187 and T072 #381 are green. Final T042 #840 and T052 #564 are green, including lifecycle stress, clean platform bootstraps and exact-head T051 benchmark. Review record `5726825909` reports zero Blocking/Important findings.

**T078 / #161 is now Ready** for the NativeUI 1.1 effects line; it adds drop shadows plus retained visual-outset invalidation/culling semantics.

### Remaining v1 work

- **T069 / #81 — Deprecated.** It is no longer a scheduler/merge gate. Current `main` public/package contracts and exact-head qualification are authoritative.
- **T068 / #80 / PR #241 — deferred to NativeUI 1.2.** Native accessibility bridges do not block 1.0.

Current path:

```text
T123–T132(done) + T173(done) + T174(done)
                         |
                         v
T070 -> T122/docs -> T071 -> v1.0.0
T068 ---------------------------------------> 1.2

post-1.0 / later-release line already landed on main:
T073(done) ---------------------------------> NativeUI 1.1 foundation
T074(done) ---------------------------------> NativeUI 1.1 foundation
T075(done) ---------------------------------> NativeUI 1.1 foundation
T076(done) -> T077(done) -> T078(ready) -----> NativeUI 1.1 effects
```

## Completed foundations relevant to v1

- **T123 / #281:** deterministic lifetime-safe `State<T>` notifications and observer exception/reentrancy semantics.
- **T124 / #282 + T138–T141:** stable `Binding<T>` contract and state-consumer migration.
- **T125 / #286:** retained dispatch/reconciliation/cancellation exception safety.
- **T126 / #287:** DesktopServices completion/native exception boundaries.
- **T127 / #288:** `ScrollState` lifetime and observer recovery semantics.
- **T128 / #289:** Dispatcher accepted-work durability and Animation exceptional recovery.
- **T129 / #290:** lifetime-safe retained invalidation callbacks.
- **T130 / #291:** lifecycle/layout/paint/native construction/teardown exception safety.
- **T131 / #293:** Overlay/Dialog/popup/Tooltip transaction-safe failure recovery.
- **T132 / #294:** standalone close lifecycle-control deferral under queue rejection/throw.
- **T173 / #401:** complete public ASCII A-Z key exposure.
- **T174 / #409:** per-UI unhandled raw KeyDown fallback on final T125 semantics.

Other delivered v1 foundations include T030–T036 standard widgets, T037–T040 Theme/style/animation, T043 resize/scale, T044 pointer capture, T045 semantic accessibility architecture, T047/T048 packaging, T049 gallery, T050 inspector, T051/T052 qualification, T053 consumer-scoped macOS Objective-C runtime identity, T054/T056/T057 helpers/resources, T058 dynamic composition, T060 Application ownership, T061/T062/T063 overlay/Tooltip/Dialog, T064 DesktopServices, T065 Dispatcher, T066 window controls, T067 virtualized ListView and T072 Linux D-Bus.

## Post-1.0 foundations already merged

- **T073 / #156:** generic Brush fill painting foundation for Color/LinearGradient/RadialGradient with deterministic value/failure semantics and shared Painter materialization.
- **T074 / #157:** Brush stroke painting for rounded rectangles, Paths, lines and arcs with PaintOptions, preserved Color/style semantics and shared Painter-local sampling.
- **T075 / #158:** strict lexical scoped clipping for Rect, rounded Rect and Path through the existing `Painter::StateGuard` stack model.
- **T076 / #159:** hard-bounded group compositing through `Painter::scoped_layer(Rect, PaintOptions)` with exact multi-frame `StateGuard` restore/rollback and deterministic group opacity/blend semantics.
- **T077 / #160:** bounded Gaussian Effect layers through the same StateGuard/PaintOptions model with conservative backend-derived output support and exact failure recovery.
- **T079 / #162:** explicit backend-neutral SkSL `ShaderProgram` compilation merged through PR #427 as `99762beb9bbb42f9f15bebcf318b60e84d746fd1`; immutable/const backend ownership, deterministic diagnostics, failure-atomic publication, pinned-m149 source-size guard, no implicit paint-time compilation and installed-package shader smoke on Linux/Windows/macOS.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development:

1. keep source-changing PRs Draft;
2. use local TDD and coherent correction batches;
3. publish only qualification-worthy heads;
4. run normal CI plus path-relevant dedicated checks;
5. complete the full `CODE_REVIEW.md` record on the frozen head;
6. transition Draft -> Ready only for final T042/T052 qualification;
7. executable/build/workflow changes invalidate that candidate and require Draft + requalification;
8. pure project-state completion documentation does not invalidate an otherwise green executable candidate.

Fault injection is mandatory where normal execution cannot deterministically reproduce allocation, callback, queue, partial-construction, layout, paint, teardown or native-boundary failure classes.

## Automation / integration recovery

GitHub live state is the only durable source of truth. The legacy Scheduler/Reporter control plane and W1–W4 persistent assignments are retired. Current automation is serialized:

```text
implementation/source-changing lanes: 1
independent review: on demand for a frozen candidate
reporting/watchdog: read-only
fallback work: disabled
```

Recovery sequence:

1. read `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `CONTEXT.md`, `ROADMAP.md` and `AUTOMATION.md`;
2. re-fetch live issues, canonical PRs, exact heads, checks, reviews and unresolved threads;
3. finish the current merge-near ticket before starting another source-changing ticket;
4. do not trust retired scheduler snapshots over current GitHub state.

## Next actions

1. Execute T070 reference application/Getting Started against the current validated public/package surface.
2. Complete explicitly scheduled v1 documentation closeout including T122 where applicable.
3. Run T071 on one exact release-candidate SHA.
4. T078 / #161 remains Ready for the NativeUI 1.1 effects line.
5. T080 / #164 is now Ready after T079 completion and adds typed SkSL uniform reflection/binding.
6. Keep T068/PR #241 parked for NativeUI 1.2.
