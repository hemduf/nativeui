# NativeUI roadmap

**Updated:** 2026-09-10

This roadmap turns NativeUI into a reusable desktop retained-mode UI toolkit while preserving the architecture: Pugl for native views/events, Skia for rendering, NativeUI for retained behavior/layout/input/widgets/resources. GitHub Issues remain the source of truth for exact ticket status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing work before creating another stream;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use test-first RED -> GREEN -> REFACTOR;
- code-changing tickets require exact-head validation and a `CODE_REVIEW.md` record;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue while another PR waits only on external CI;
- every completion cycle synchronizes `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

`main` is `c83595ea53a9212d9544da36400c2a01d2843f7e` and includes T058 / PR #154, #163 / PR #181 warning-free source-tree builds, T036 / PR #155, T065 / PR #133, T034 / PR #135, the supported T060 multi-window lifecycle model, post-T060 T042 stress qualification, the T052 v0.1 developer-preview release/package gate, and merged Tree paint-ownership regression #152 / PR #153. T037 / PR #151 is refreshed onto that baseline and is the active style completion candidate. T045 is the next UI/accessibility dependency-unblocked item; T072 remains active in the independent platform lane.

T058 is complete and provides bounded retained dynamic composition (`If`, `Switch`, keyed `ForEach`) with deterministic structural diagnostics and lifecycle/focus-safe reconciliation. This removes T058 as a blocker for T067 and advances the T061/T035/T063 overlay chain.

Cross-cutting rendering regression #152 / PR #153 removed implicit Tree-level visual decoration: `Tree::paint()` no longer paints a default viewport background or the hard-coded `TAB / SHIFT+TAB...` instruction line. Visual backgrounds and overlays belong to consumer/component composition, with a headless regression protecting that ownership boundary.

#163 / PR #181 makes unapproved NativeUI compiler warnings Blocking by default, migrates source-tree examples/smokes to the v1 Application ownership path and fixes the warnings exposed by the stricter build. `NATIVEUI_ALLOWED_WARNINGS` remains empty by default and is the only normal explicit opt-in for a temporarily accepted diagnostic.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045 -> T067 -> T068
                                           T058(done) -----------^      ^

style:             T037(PR #151 completion) -> T038 -> T039
                                               +-> T040 with T065(done)

dynamic/overlay:   T058(done) -> T061 -> T035 ------------------------> T068
                                      +-> T063 ------------------------> T068

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

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 additionally establishes warning-free NativeUI-owned source-tree builds as a project-wide quality gate.

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

Complete in PR #115. Shared numeric-domain and track-axis helpers handle finite ranges, horizontal/vertical mapping, keyboard editing, nearest-thumb RangeSlider selection, no crossing, T059 read-only/disabled behavior and bounded reentrancy.

### T033 — ProgressBar and Meter

Complete in PR #123. The widgets are display-only, use finite presentation normalization without State writeback, support horizontal/vertical fill and optional formatting, and add no hidden timer or interaction state.

### T034 — ScrollView

Complete in PR #135.

Delivered contract:

- `ScrollState` remains the sole offset/metrics authority;
- wheel events are handled only when effective offset changes, allowing natural nested boundary bubbling;
- optional pointer panning uses toolkit capture with no inertia/timer;
- retained overlay scrollbars use fixed 8 logical px thickness and 18 px minimum thumb, deterministic thumb drag, handled/no-jump track clicks and both-axis corner shortening;
- public Nearest/Start/Center/End `ensure_visible` plus focused-descendant reveal;
- T059 Disabled/Hidden/Collapsed interaction suppression/cancellation while ReadOnly remains scrollable;
- generic `pointer_targetable()` separates pointer targeting from keyboard focus so scroll overlays remain interactive without polluting focus traversal.

### T036 — ListView and Tabs

Complete in PR #155 and present on `main`.

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

T045 is now the next UI/accessibility item. Because T058 is complete, T067 waits only on T045 before T068. T035 remains separate and additionally waits for T061.

## Milestone 6 — Styling, theme and animation

**Status: T037 completion candidate; T038/T039 follow.**

### T037 — Typed theme tokens

T037 / issue #37 / PR #151 delivers:

- strongly typed Theme palette, typography, spacing, radii and shared control metrics;
- deterministic `default_theme()` with explicit paint-only versus layout-affecting change classification;
- one Theme per UI/retained tree with no mutable process-global current theme;
- Button/Slider measurement and paint driven by the bound tree theme;
- public `UI::theme()` / `UI::set_theme()` plus isolated `theme.hpp` and umbrella coverage;
- deterministic per-UI isolation, invalidation and representative widget regression tests;
- `t037_theme --self-test` registered as a feature example;
- explicit Windows public-header regressions: theme spacing/radius tokens use `sm` rather than macro-prone `small`, and `geometry.hpp` uses macro-safe `(std::min)` / `(std::max)` calls so `theme.hpp` remains usable after `windows.h`;
- explicit coexistence with T058: Tree retains dynamic-source/reconciliation support while adding per-tree Theme ownership; UI retains `structural_diagnostic()` while adding Theme accessors; the umbrella and header gate expose both features.

The TDD stream preserves the established 72x160 formatted vertical Slider default geometry after review caught the potential regression. The branch is refreshed directly onto the T058 + warning-free baseline. Fresh exact-head CI/T042/T060/T052/T065 plus final `CODE_REVIEW.md` review are the remaining merge gates. After T037, T038 is the direct style continuation, followed by T039; T040 must reuse T065 rather than create a second scheduler.

### T058 — Dynamic composition

T058 / issue #58 / PR #154 is complete on `main`.

Implemented contract includes bounded deferred structural reconciliation, `If`, `Switch`, keyed `ForEach`, deterministic duplicate-key diagnostics, focus/lifecycle-safe subtree replacement, monotonic NodeId ownership, and deterministic tests/example coverage. T058 is no longer a blocker for T067 and now feeds T061 in the overlay path.

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

T044 / issue #44 / PR #145 remains a T071 release dependency and is being completed independently through native outside-view capture evidence on macOS, Windows and Linux/X11. Native extensions are added only where the evidence proves the toolkit/Pugl path insufficient.

## Milestone 8 — Packaging, tooling and release

Delivered foundations include T047 low-level package export, T048 relocated external consumers, T051 performance-regression harness, T052 v0.1 developer-preview release gate, T054 native application helper, T056 deterministic binary-data generation and T057 ResourceManager.

### T052 — v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete and merged. It establishes the infrastructure/package release baseline without claiming NativeUI 1.0 product completeness.

The gate validates clean-cache pinned Pugl/Skia bootstrap and fail-closed checksums on Linux X11, Windows and macOS; relocated low-level package consumption through `NativeUI::Core + nativeui_attach_platform()`; macOS consumer-specific T053 Objective-C namespaces; the supported T060/#139 lifecycle path; T051 comparative performance policy and exact-zero idle invalidation; and developer-preview release/legal documentation.

Remaining release/package frontier:

```text
T036(done) -> T045 -> T067 ----\
T058(done) --------------------+--> T068 -> T069 -> T070 -> T071
T035/T063 ---------------------+
T065(done) -> T072 -> T064 ----+
T043 -----------> T066 --------/
other explicit T069 deps -------/
```

T069 is the final v1 public API freeze and must not start until every explicit dependency in issue #81 is complete. T070 then validates the production reference application/Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA.

## Immediate cross-lane plan

1. Finish exact-head qualification, final review and merge of T037 / PR #151; continue T038 afterward.
2. Continue T043 / PR #142 independently, then T066.
3. Finish T044 / PR #145 through its native capture evidence matrix and final review.
4. Continue T045 then T067 now that T058 is complete; keep active T072/T064 progressing according to explicit dependencies.
5. Continue T061 and then T035/T063; those converge at T068.
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
