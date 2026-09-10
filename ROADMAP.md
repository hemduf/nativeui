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

`main` contains the standard widget set through T033, the supported T060 multi-window lifecycle stress from #139, and the merged T052 v0.1 developer-preview release/package qualification (`bc5e38e7e87168d6faf1edcb15f9beb3b725c070`). The widget/layout lane is finalizing T034 / PR #135; the platform/event lane remains on T065 / PR #133.

Current dependency frontier:

```text
widgets/layout:    T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(completion PR #135) -> T036
                                                                    -> T035 only after T061

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

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

**Status: T030–T033 complete; T034 completion candidate.**

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

T034 / issue #34 / PR #135 is the active completion candidate. It remains built around the existing T012 `ScrollState` as the sole offset/metrics model.

Delivered contract on the current code candidate:

- wheel deltas apply through `ScrollState`; events are Handled only when the effective enabled-axis offset changes, so nested ScrollViews bubble at boundaries;
- optional `.pointer_pan(true)` uses toolkit pointer capture with exact start-offset minus pointer-delta mapping, deterministic Up/Cancel/unavailability teardown and no inertia/timer;
- retained overlay scrollbars use fixed 8 logical px thickness and 18 px minimum thumb, consume track clicks without jumping, support linear captured thumb drag and shorten both-axis tracks at the corner;
- overlays are retained children after content, so paint order plus reverse retained-child pointer targeting gives scrollbar input precedence over interactive content;
- public `ensure_visible(ScrollState&, Rect, ScrollAlignment)` implements Nearest/Start/Center/End, including oversized-child start behavior, and focused descendants are automatically revealed with Nearest before subsequent paint;
- T059 Disabled suppresses scroll interaction/capture, Hidden/Collapsed/Disabled transitions use the existing exactly-once cancellation path, and ReadOnly remains scrollable;
- T012 remains the single content clipping layer; no native scrollbar, platform header, timer or hidden event pump is introduced;
- a generic `Component::pointer_targetable()` seam separates pointer targetability from keyboard focus without changing historic focusable behavior: it defaults to `focusable()`, while ScrollView/scrollbar overlays explicitly opt in and inert visual siblings remain non-targetable.

TDD/review history closed the main architecture gaps: non-focusable ScrollView wheel targeting; real overlay precedence over interactive content; focused-descendant reveal; and the inert-overlap regression caused by overly broad pointer hit testing. Review `5164075036` then found one remaining Important test-quality issue: the legacy overlay-precedence helper was not explicitly pointer-targetable. Exact code head `4cac2b82b0cf775c1e691ec412460072f6abaf06` fixes that fixture so the test cannot pass trivially. CODE_REVIEW.md follow-up `5164615464` reports no remaining Blocking/Important production finding.

Required registered coverage includes nested boundary wheel bubbling, pointer-pan default/capture/up/cancel, exact scrollbar geometry/corner behavior, thumb/track interaction, overlay-vs-interactive-content precedence, inert-overlap pointer routing, T059 Disabled/ReadOnly and mid-interaction cancellation, Nearest/Start/Center/End and focus reveal, idle/no-timer behavior, vertical/horizontal/both headless scrollbar states, and `examples/features/t034_scroll_view.cpp --self-test`.

On code head `4cac2b82b0cf775c1e691ec412460072f6abaf06`, T042 Lifecycle Stress, T060 Application Contract and T052 Release Gate are green; normal CI is green on Linux ASan+UBSan, Linux X11 and Windows while macOS is completing. This documentation synchronization creates a new final exact head, so every required workflow must be green again before merge.

After T034 merges, T036 / issue #36 is the preferred next dependency-unblocked widget/layout ticket. T035 additionally depends on T061 and must not start before T061 is merged.

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

Independent platform hardening such as T043/T044 remains outside the widget/layout and package/release lanes.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T052 — v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete and merged as `bc5e38e7e87168d6faf1edcb15f9beb3b725c070`. It establishes the infrastructure/package release baseline without claiming NativeUI 1.0 product completeness.

The gate validates clean-cache pinned Pugl/Skia bootstrap and fail-closed checksums on Linux X11, Windows and macOS; relocated low-level package consumption through `NativeUI::Core + nativeui_attach_platform()`; macOS consumer-specific T053 Objective-C namespaces; the supported T060/#139 lifecycle path; T051 comparative performance policy and exact-zero idle invalidation; and developer-preview release/legal documentation.

Remaining release/package frontier:

```text
T060(done) -> T065(active) -> T072 -> T064
many v1 feature/platform dependencies -> T069 -> T070 -> T071
```

Later package/API/release tickets remain gated by their explicit dependencies.

## Immediate cross-lane plan

1. Finish exact-head qualification, final review and merge of T034 / PR #135.
2. After T034 merges, begin/resume T036 / issue #36 in the widget/layout lane using strict TDD.
3. Keep T035 blocked until T061 is merged.
4. Continue T065 / PR #133 only in its independent platform/event lane; T072/T064 remain downstream.
5. Keep T069/T070/T071 blocked until their explicit dependency sets are complete.

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