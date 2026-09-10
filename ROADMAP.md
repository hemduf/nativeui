# NativeUI roadmap

**Updated:** 2026-09-10

This roadmap turns NativeUI into a reusable desktop retained-mode UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained behavior/layout/input/widgets/resources. GitHub Issues remain the source of truth for exact ticket status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing work before creating another stream;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use test-first RED -> GREEN -> REFACTOR;
- code-changing tickets require exact-head validation and a `CODE_REVIEW.md` record;
- unrelated lanes may continue while another PR waits only on external CI;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

`main` is `645679b3c375d54c68227753583c9cb0a74fd801` and includes T034 / PR #135, the supported T060 multi-window lifecycle model, post-T060 T042 stress qualification, and the T052 v0.1 developer-preview release/package gate. T065 / PR #133 is the active critical platform completion candidate and has been refreshed onto this main baseline.

Current dependency frontier:

```text
widgets/layout:    T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(done) -> T036
                                                      -> T035 only after T061

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(completion PR #133) -> T072 -> T064
                   T041(done) -> T043(active PR #142) -> T066
                                              |-------> T068
                   T065 + T072 + T043 + remaining feature deps -> T068 -> T069
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

**Status: T030–T034 complete.**

### T030 — Button

Complete in PR #94. Pointer/keyboard activation, capture, disabled-state handling and reentrant callbacks use generic retained input/state semantics.

### T031 — Checkbox and Radio

Complete in PR #95. Checkbox and typed RadioGroup/RadioButton share deterministic value/state ownership and T059 availability semantics.

### T032 — Slider and RangeSlider

Complete in PR #115, squash-merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1`. Shared numeric-domain and track-axis helpers handle finite ranges, horizontal/vertical mapping, keyboard editing, nearest-thumb RangeSlider selection, no crossing, T059 read-only/disabled behavior and bounded reentrancy.

### T033 — ProgressBar and Meter

Complete in PR #123, squash-merged as `44626fef8d70f73428aa6ce906357a922cb010f5`. The widgets are display-only, use finite presentation normalization without State writeback, support horizontal/vertical fill and optional formatting, and add no hidden timer or interaction state.

### T034 — ScrollView

Complete in PR #135 and merged to `main` as `645679b3c375d54c68227753583c9cb0a74fd801`.

Delivered contract:

- `ScrollState` remains the sole offset/metrics authority;
- wheel events are handled only when effective offset changes, allowing natural nested boundary bubbling;
- optional pointer panning uses toolkit capture with no inertia/timer;
- retained overlay scrollbars use fixed 8 logical px thickness and 18 px minimum thumb, deterministic thumb drag, handled/no-jump track clicks and both-axis corner shortening;
- public Nearest/Start/Center/End `ensure_visible` plus focused-descendant reveal;
- T059 Disabled/Hidden/Collapsed interaction suppression/cancellation while ReadOnly remains scrollable;
- generic `pointer_targetable()` separates pointer targeting from keyboard focus so scroll overlays remain interactive without polluting focus traversal.

T036 is now the preferred dependency-unblocked widget/layout continuation. T035 additionally waits for T061.

## Milestone 6 — Styling, theme and animation

**In progress through independent dependencies.** T037 -> T038 -> T039 owns typed theme/style/scoped inheritance. T040 additionally depends on T065 for deterministic timers and must not create a second scheduler.

## Milestone 7 — Platform and embedded robustness

Core lifecycle/consumer-safety baseline is delivered:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- T042 deterministic lifecycle stress;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 / PR #140 post-T060 T042 qualification.

### T065 — Dispatcher/timer service

T065 / issue #77 / PR #133 is the current critical completion candidate.

Implemented contract includes:

- exact per-owner limits: 65,536 pending tasks, 8,192 active timers and 1,024 callbacks per checkpoint;
- weak thread-safe Dispatcher handles and deterministic per-owner FIFO execution;
- one-shot/fixed-delay repeating timers, fake monotonic time, cancellation and queue-saturation retry;
- user callback/capture destruction outside internal dispatcher locks;
- independent task/timer namespaces even when standalone windows share one Application wake backend;
- worker wake via captured native primitives rather than concurrent Pugl calls;
- Application waits interrupted by worker posts and bounded by timer deadlines without busy polling;
- host-driven non-blocking EmbeddedView dispatch;
- deterministic core/platform tests and `t065_ui_dispatcher --self-test`.

Exact code head `d12064ab485aa9f21128eeb2059d40b1f8c3f969` passed normal CI, T065 core and platform workflows, T060 Application Contract, T042 Lifecycle Stress and T052 Release Gate. The PR was then cleanly refreshed onto current T034 main. The documentation-synchronized completion head must receive a fresh exact-head matrix and final `CODE_REVIEW.md` review before autonomous merge.

### Critical downstream platform order

After T065:

1. **T072 / issue #84** — shared bounded Linux `libdbus-1` transport. It directly unlocks both T064 and the Linux side of T068.
2. **T043 / issue #43 / PR #142** — resize/scale contract. It may advance while T065 waits only on external CI and is required by T066 and T068.
3. **T064 / issue #76** — DesktopServices, after T065 + T072.
4. **T066 / issue #78** — final standalone window controls, after T060 + T043.

T044 remains a T071 release dependency but is not on the T068/T069 critical path; defer substantive T044 work until the above prerequisites unless its existing PR is already exact-head green and needs only final review/metadata/merge.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T052 — v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete and merged as `bc5e38e7e87168d6faf1edcb15f9beb3b725c070`. It establishes the infrastructure/package release baseline without claiming NativeUI 1.0 product completeness.

The gate validates clean-cache pinned Pugl/Skia bootstrap and fail-closed checksums on Linux X11, Windows and macOS; relocated low-level package consumption through `NativeUI::Core + nativeui_attach_platform()`; macOS consumer-specific T053 Objective-C namespaces; the supported T060/#139 lifecycle path; T051 comparative performance policy and exact-zero idle invalidation; and developer-preview release/legal documentation.

Remaining release/package frontier:

```text
T065 -> T072 -> T064 ----\
T043 ---------> T066 -----+-> remaining v1 feature/platform convergence -> T068 -> T069 -> T070 -> T071
other explicit T069 deps -/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 then validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Finish fresh exact-head qualification, final review and merge of T065 / PR #133.
2. Start T072 immediately after T065 merges.
3. Advance existing T043 / PR #142 whenever T065 is waiting only on CI, then merge it before T066/T068.
4. Finish T064 and T066 as their dependencies become satisfied, prioritizing whichever existing stream is farther along or longer while the other waits only on CI.
5. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.

## Prioritization rule

Preserve the architectural direction:

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets and styles
  -> platform/lifecycle services
  -> accessibility/public API freeze
  -> reference package/release
```

This is architectural progression, not serialization. Independent tickets may proceed concurrently once explicit dependencies are satisfied.