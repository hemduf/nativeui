# NativeUI compact recovery context

**Updated:** 2026-09-10

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` contains the explicit T060 Application/multi-window ownership model, post-T060 T042 lifecycle qualification from #139, the completed standard widgets through T033, and the merged T052 v0.1 developer-preview release gate (`bc5e38e7e87168d6faf1edcb15f9beb3b725c070`).

Relevant completed foundations:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T032 / PR #115: Slider + RangeSlider.
- T033 / PR #123: ProgressBar + Meter, squash-merged as `44626fef8d70f73428aa6ce906357a922cb010f5`.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T052 / PR #120: v0.1 developer-preview release gate with clean-cache bootstrap, exact package consumer, lifecycle, benchmark, idle-invalidation and platform qualification.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- #139 / PR #140: T042 stress-tests that supported T060 A+B multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM path.

## T052 v0.1 developer-preview release gate

T052 / issue #52 / PR #120 is complete and merged on `main` as `bc5e38e7e87168d6faf1edcb15f9beb3b725c070`. It qualifies the current infrastructure/package baseline as a developer preview, not as the NativeUI 1.0 product-completeness gate.

The gate preserves the low-level installed package contract `NativeUI::Core + nativeui_attach_platform()`, validates relocated external consumers and consumer-scoped macOS Objective-C namespaces, verifies clean-cache pinned dependency acquisition/fail-closed checksum behavior, retains the T051 exact-zero idle invalidation hard gate, and publishes `docs/releases/v0.1.0.md` with developer-preview semantics and current known v1 gaps.

## Widget/layout lane — T034 completion candidate

T032 provides the shared finite numeric-domain and track-axis behavior used by Slider/RangeSlider. T033 adds display-only ProgressBar/Meter widgets. T034 / issue #34 / PR #135 is the active widget/container completion stream and remains built around the existing T012 `ScrollState` as the sole scroll offset/metrics model.

Current T034 implementation delivers:

- change-based wheel scrolling through `ScrollState`, so nested ScrollViews bubble naturally at clamped boundaries;
- opt-in pointer pan with retained pointer capture and no inertia/timer;
- retained 8 px overlay scrollbars with 18 px minimum thumb, deterministic thumb drag, handled/no-jump track clicks and both-axis corner shortening;
- scrollbar input/paint precedence through retained overlay children after content and reverse paint-order pointer hit testing;
- public `ensure_visible` with Nearest/Start/Center/End plus automatic focused-descendant Nearest reveal;
- T059 Disabled/Hidden/Collapsed interaction suppression/cancellation while ReadOnly remains scrollable;
- one existing T012 content clip and no native scrollbar/platform-specific event path;
- a generic retained `pointer_targetable()` seam, defaulting to historical `focusable()` behavior, so ScrollView/scrollbar overlays can receive pointer input without becoming keyboard Tab stops while inert visual siblings do not mask interactive content.

Review `5164075036` found one Important test-quality gap: a legacy overlay-precedence helper intended to represent interactive content did not explicitly opt into pointer targeting. Exact code head `4cac2b82b0cf775c1e691ec412460072f6abaf06` corrects that regression fixture. Follow-up CODE_REVIEW.md review `5164615464` finds no remaining Blocking/Important production finding. On that code head, T042 Lifecycle Stress, T060 Application Contract and T052 Release Gate are green; normal CI is green on Linux ASan+UBSan, Linux X11 and Windows while macOS qualification is completing. This documentation synchronization creates the final completion candidate and therefore requires fresh exact-head validation before merge.

## Current dependency frontier

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

T034 and then T036 belong to the widget/layout lane. T035 additionally depends on T061 and must remain blocked until that dependency is merged. T065/T072/T064 and other platform hardening are separate streams and must not be duplicated by widget work.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Normal CI validates Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts and native lifecycle/platform smokes. Code-changing widget candidates also require exact-head T042 Lifecycle Stress and T060 Application Contract when those workflows are registered for the PR; current completion candidates also run the T052 Release Gate.

## Next actions

1. Qualify the final documentation-synchronized T034 / PR #135 head with normal CI, T042 Lifecycle Stress, T060 Application Contract and T052 Release Gate; complete final CODE_REVIEW.md evidence and merge only if current `main` is still its base and every required executed gate is green.
2. After T034 merges, start/resume T036 / issue #36 in this widget/layout lane using strict TDD; do not duplicate an existing branch/PR if one appears.
3. Keep T035 blocked until T061 is merged.
4. Leave T065 / PR #133 and downstream T072/T064 to their independent platform/event lane, and keep later v1 freeze/release work dependency-gated.