# NativeUI roadmap

**Updated:** 2026-09-10

This roadmap turns NativeUI into a reusable desktop retained-mode UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained behavior/layout/input/widgets/resources. GitHub Issues remain the source of truth for exact ticket status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are the hard gates;
- resume existing work before creating another stream;
- among Ready work, prefer priority then downstream unblock value;
- behavior/configuration changes use test-first RED -> GREEN -> REFACTOR;
- code-changing tickets require exact-head validation and a `CODE_REVIEW.md` record;
- unrelated lanes continue while another PR waits on external CI;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

`main` contains the standard widget set through T033, the supported T060 multi-window lifecycle stress from #139, and the completed T052 v0.1 developer-preview release/package gate from PR #120. The state/widget lane remains on T034 / PR #135, the platform/event lane on T065 / PR #133, and the platform geometry/input lane is completing T043 / PR #142 before taking T044.

Current dependency frontier:

```text
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(active PR #135) -> T035 / T036

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

platform/geometry: T041(done) -> T043(active PR #142) -> T044

platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes, pointer capture, wheel normalization, gestures, commands and drag/drop primitives. Later widget work may add generic retained-tree seams only when required by those contracts; platform-specific widget input paths remain forbidden.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide the text-edit model, Label, fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**Status: T030–T033 complete; T034 active.**

### T030 — Button

Complete in PR #94. Pointer/keyboard activation, capture, disabled-state handling and reentrant callbacks use generic retained input/state semantics.

### T031 — Checkbox and Radio

Complete in PR #95. Checkbox and typed RadioGroup/RadioButton share deterministic value/state ownership and T059 availability semantics.

### T032 — Slider and RangeSlider

Complete in PR #115, squash-merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1`. Shared numeric-domain and track-axis helpers handle finite ranges, wide binary32 endpoints through double intermediates, horizontal/vertical mapping, keyboard editing, nearest-thumb RangeSlider selection, no crossing, T059 read-only/disabled behavior and bounded reentrancy.

### T033 — ProgressBar and Meter

Complete in PR #123, squash-merged as `44626fef8d70f73428aa6ce906357a922cb010f5`.

Delivered behavior:

- display-only `ProgressBar` and `Meter` bound to `State<float>&`;
- finite `minimum < maximum` validation and presentation normalization in `double`, including full binary32 endpoint ranges;
- out-of-range finite values clamp for rendering only; NaN/Inf render as minimum; no State writeback;
- horizontal left-to-right and vertical bottom-to-top fill;
- optional presentation formatter;
- non-focusable, input-ignored widgets with no timer, smoothing, capture or hidden animation policy;
- paint-only invalidation for bound-state changes;
- deterministic domain/geometry/raster/idle tests and `examples/features/t033_progress_meter.cpp --self-test`.

Final exact head `149b36c063b27eea6bb0bb370de7d736110fb357` passed normal CI `34437578487`, T060 Application Contract `34437578456`, and T042 Lifecycle Stress `34437578451` after the Linux lifecycle diagnostic was successfully rerun on the same head. Final mandatory review record `5162886816` has no Blocking/Important finding. Issue #33 is Done.

### T034 — ScrollView

T034 / issue #34 / PR #135 is the active existing widget/container stream. It must remain built around the existing T012 `ScrollState` as the sole offset/metrics model.

Required contract includes:

- wheel deltas apply through `ScrollState`; return Handled only when effective offset changes so nested ScrollViews bubble correctly at boundaries;
- optional pointer pan with toolkit pointer capture, no inertia/timer;
- 8 px overlay scrollbars with 18 px minimum thumb, no layout-space consumption, deterministic thumb drag and consumed track clicks;
- overlay input precedence over interactive content and overlay paint after content;
- `ensure_visible` Nearest/Start/Center/End plus automatic focused-descendant reveal;
- T059 Disabled/Hidden/Collapsed suppression and cancellation; ReadOnly does not disable scrolling;
- one content clip, no native scrollbar/event pump/platform header path;
- deterministic tests, goldens, full suite/sanitizers and `examples/features/t034_scroll_view.cpp --self-test`.

Current RED evidence on PR #135: normal CI fails `nativeui_scroll_layout_tests` at `tests/scroll_layout_tests.cpp:109` because the retained tree currently selects focusable nodes as pointer targets, so a ScrollView around non-focusable content cannot receive its wheel event. This should be solved with a generic retained-tree pointer-targeting seam, not by introducing platform code or silently turning unrelated controls into native widgets. The mandatory architecture review also requires a real overlay-node solution for scrollbar precedence/paint order rather than parent-after-child bubbling that interactive content can consume first.

T035/T036 remain downstream of T034 and must not start until their explicit dependency is satisfied.

## Milestone 6 — Styling, theme and animation

**Pending explicit dependencies.** Typed theme tokens, component styles, scoped inheritance, animation/tween helpers and reduced-motion support proceed through their issue DAG after required widget foundations.

## Milestone 7 — Platform and embedded robustness

Core lifecycle/consumer-safety baseline is delivered:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- T042 deterministic lifecycle stress;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 / PR #140 post-T060 T042 qualification, merged as `ef10f9f8ece733b1f0f19d3326be9a3ced903e41`.

#139's final head `ee482da974222299bc94904ed8256511da1a256d` passed T042 Lifecycle Stress `34434737094`, T060 Application Contract `34434737109` and normal CI `34434737107`; final review found no Blocking/Important issue.

### T043 — Resize/scale negotiation

T043 / issue #43 / PR #142 is the active completion candidate for the geometry/input lane. It establishes one per-view logical/native geometry contract: finite positive retained scale, configure-authoritative logical viewport, transient zero-size suppression, one-time physical/logical conversion for size/input/dirty/drop/text-input geometry, and advisory preferred-size callbacks for standalone and embedded views.

Review/TDD corrections in the candidate include dedicated build registration, Release-active tests, teardown-safe preferred callback dispatch, and an exact Pugl span conversion that keeps covering `ceil` rounding while preserving the pinned dependency's documented 1..10000 view-size range. Embedded native smoke covers parent authority and reentrant callback-driven size grants. T043 may merge only after the final documentation head is refreshed from current `main`, its exact CI/T042/T060/platform/sanitizer gates are green, and the final `CODE_REVIEW.md` pass reports no Blocking/Important finding.

### T044 — OS pointer capture evidence/fix

T044 follows T043 in this lane once T043 is merged. Its T015/T041 dependencies are already complete. The ticket is evidence-gated: run the same outside-view drag/release/focus-loss/two-view fixture on macOS, Windows and Linux/X11, classify each platform as toolkit capture sufficient or native capture required, and add only the smallest per-platform native extension where loss is reproduced.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T052 — v0.1 developer-preview release gate

T052 / PR #120 establishes the infrastructure/package release baseline without claiming NativeUI 1.0 product completeness. Its pre-completion candidate was synchronized from `main` `63e2603a6d85be3636347d476d2df0247f180622` and passed T052 Release Gate `34442124731`, normal CI `34442124735`, T042 Lifecycle Stress `34442124762`, T051 Release Benchmarks `34442124746`, and T060 Application Contract `34442124771`. PR #120 is now merged on `main`.

The gate validates clean-cache pinned Pugl/Skia bootstrap and fail-closed checksums on Linux X11, Windows and macOS; relocated low-level package consumption through `NativeUI::Core + nativeui_attach_platform()`; macOS consumer-specific T053 Objective-C namespaces; the supported T060/#139 lifecycle path; T051 comparative performance policy and exact-zero idle invalidation; and developer-preview release/legal documentation.

Remaining release/package frontier after T052:

```text
T060(done) -> T065(active) -> T072 -> T064
many v1 feature/platform dependencies -> T069 -> T070 -> T071
```

There is no additional dependency-unblocked P0 package implementation immediately after T052; later package/API/release tickets remain gated by their explicit dependencies.

## Immediate cross-lane plan

1. Finish T043 / PR #142 exact-head qualification/review and merge it if the complete candidate is green; then take T044 in the geometry/input lane.
2. Resume T034 / PR #135 only in the independent state/widget lane.
3. Continue T065 / PR #133 only in the independent platform/event lane; T072/T064 remain downstream.
4. Keep T069/T070/T071 blocked until their explicit dependency sets are complete; do not broaden package work to bypass those gates.

## Prioritization rule

Preserve the architectural direction:

```text
core correctness
  -> layout
  -> input/focus
  -> rendering/text primitives
  -> widgets
  -> styling
  -> platform hardening
  -> packaging/release
```

This is architectural progression, not serialization. Independent tickets may proceed concurrently once explicit dependencies are satisfied.
