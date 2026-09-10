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

`main` includes T036 / PR #155 as `cbf68026fc0780f1e5d04e2120cae76b188ed780`, plus T065 / PR #133, T034 / PR #135, the supported T060 multi-window lifecycle model, post-T060 T042 stress qualification, the T052 v0.1 developer-preview release/package gate, and merged Tree paint-ownership regression #152 / PR #153. T045 is now the next UI/accessibility dependency-unblocked item. T072 is active in the independent platform lane after T065 completion.

Cross-cutting rendering regression #152 / PR #153 removed implicit Tree-level visual decoration: `Tree::paint()` no longer paints a default viewport background or the hard-coded `TAB / SHIFT+TAB...` instruction line. Visual backgrounds and overlays belong to consumer/component composition, with a headless regression protecting that ownership boundary.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045 -> T067 -> T068
                                           T058(done required) ----^      ^

widgets/overlay:   T034(done) + T061 -> T035 ---------------------------> T068
                   T061 + T034 -> T063 -------------------------------> T068

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(active) -> T064
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

**Status: T030–T034 and T036 complete.**

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

### T036 — ListView and Tabs

Complete in PR #155 and squash-merged to `main` as `cbf68026fc0780f1e5d04e2120cae76b188ed780`.

Delivered contract:

- fully retained, intentionally non-virtualized `ListView<T>` with stable keys and application-owned optional selection state;
- deterministic duplicate-key rejection and missing-selection behavior without mount-time state rewrite;
- ListView acts as one composite Tab stop, with row content excluded from global traversal while preserving normal retained lifecycle;
- Up/Down/Home/End and pointer selection skip unavailable items, call T034 `ensure_visible`, and optional Enter/Space/pointer activation fires exactly once after selection;
- application-originated selected-key changes also reveal the selected row through the same T034 `ScrollState` path;
- `Tabs<T>` uses stable keys and application-owned selection with automatic Left/Right/Home/End activation, disabled-tab skipping/wrap and pointer activation;
- inactive panels consume T059 `Collapsed` semantics, so they leave layout/paint/hit testing/focus consistently;
- ReadOnly remains navigation-capable while Disabled suppresses normal targeting/focus;
- dedicated tests cover public/data model, interaction, composite focus, O(N) fully-retained construction baseline, two-instance isolation and deterministic headless states; `t036_list_tabs --self-test` is the feature acceptance executable.

The completion review found one Important application-selection reveal gap and corrected it test-first in `fe51752e9093df0f23a68c4db0e1ef9532b16da7`. Exact completion head `458b48ed60e187a1a89587b39735da3d387b0532` then passed CI #861 on Linux ASan+UBSan, Linux X11, Windows and macOS, plus T042 Lifecycle Stress #433, T052 Release Gate #178, T060 Application Contract #256 and T065 Dispatcher Contract #82. No Blocking/Important review finding remained at merge.

T045 is now the next UI/accessibility item. T067 waits on T045 + T058 before T068. T035 remains separate and additionally waits for T061.

## Milestone 6 — Styling, theme and animation

**In progress through independent dependencies.** T037 -> T038 -> T039 owns typed theme/style/scoped inheritance. T040 consumes the now-complete T065 deterministic timer service and must not create a second scheduler.

## Milestone 7 — Platform and embedded robustness

Core lifecycle/consumer-safety baseline is delivered:

- #62 plug-in-host instance/runtime safety;
- #64 standalone ownership Decision B;
- T053 consumer-specific Objective-C bridge naming;
- T042 deterministic lifecycle stress;
- T060 explicit one-Application/one-PROGRAM-world multi-window ownership;
- #139 / PR #140 post-T060 T042 qualification;
- T065 bounded UI-thread dispatcher/timer service.

### T065 — Dispatcher/timer service

T065 / issue #77 / PR #133 is complete and merged.

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

### Critical downstream platform order

With T065 complete:

1. **T072 / issue #84** — shared bounded Linux `libdbus-1` transport is active. It directly unlocks both T064 and the Linux side of T068.
2. **T043 / issue #43 / PR #142** — resize/scale contract may progress independently and is required by T066 and T068.
3. **T064 / issue #76** — DesktopServices, after T072.
4. **T066 / issue #78** — final standalone window controls, after T043.

T044 remains a T071 release dependency but is not on the T068/T069 critical path; defer substantive T044 work until the above prerequisites unless its existing PR is already exact-head green and needs only final review/metadata/merge.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T052 — v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete and merged as `bc5e38e7e87168d6faf1edcb15f9beb3b725c070`. It establishes the infrastructure/package release baseline without claiming NativeUI 1.0 product completeness.

The gate validates clean-cache pinned Pugl/Skia bootstrap and fail-closed checksums on Linux X11, Windows and macOS; relocated low-level package consumption through `NativeUI::Core + nativeui_attach_platform()`; macOS consumer-specific T053 Objective-C namespaces; the supported T060/#139 lifecycle path; T051 comparative performance policy and exact-zero idle invalidation; and developer-preview release/legal documentation.

Remaining release/package frontier:

```text
T036(done) -> T045 -> T067 ----\
T058 --------------------------+--> T068 -> T069 -> T070 -> T071
T035/T063 ---------------------+
T065(done) -> T072 -> T064 ----+
T043 -----------> T066 --------/
other explicit T069 deps -------/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 then validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Move T045 to Ready and start it as the next UI/accessibility critical-path item.
2. Keep T067 blocked until T045 + T058 are complete; then prioritize it because T068 depends on its immutable virtual semantic metadata model.
3. Continue active T072; once complete, advance T064.
4. Continue T043 / PR #142 independently, then T066.
5. Let the dynamic/overlay lane finish T058 -> T061 and then T035/T063; those converge at T068.
6. Keep T069/T070/T071 dependency-gated; do not freeze the v1 API early.

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
