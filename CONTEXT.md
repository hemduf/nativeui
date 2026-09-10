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

`main` contains the explicit T060 Application/multi-window ownership model, the post-T060 T042 lifecycle qualification from #139, the completed standard widgets through T033, and the T052 v0.1 developer-preview release gate after PR #120 merges.

Cross-cutting rendering regression #152 / PR #153 removes Tree-owned visual decoration: `Tree::paint()` no longer forces `colors::background` across the viewport and no longer draws the hard-coded keyboard/mouse help line. Generic retained-tree painting is therefore consumer/component-owned; the regression test seeds a headless surface and verifies an otherwise empty Tree does not modify it.

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
- #139 / PR #140: T042 now stress-tests that supported T060 A+B multi-window path while preserving #64 Decision B for the legacy independent-PROGRAM path.

## T052 v0.1 developer-preview release gate

T052 qualifies the current infrastructure/package baseline as a developer preview, not as the NativeUI 1.0 product-completeness gate. The final pre-completion candidate was synchronized from current `main` `63e2603a6d85be3636347d476d2df0247f180622` and passed all exact-head workflows before the final documentation-only completion update: T052 Release Gate `34442124731`, normal CI `34442124735`, T042 Lifecycle Stress `34442124762`, T051 Release Benchmarks `34442124746`, and T060 Application Contract `34442124771`.

The completed gate preserves the low-level installed package contract `NativeUI::Core + nativeui_attach_platform()`, validates relocated external consumers and consumer-scoped macOS Objective-C namespaces, verifies clean-cache pinned dependency acquisition/fail-closed checksum behavior, retains the T051 exact-zero idle invalidation hard gate, and publishes `docs/releases/v0.1.0.md` with developer-preview semantics and current known v1 gaps. The final documentation head must rerun the same exact-head workflows before PR #120 is merged.

## State/widget lane — T033 complete, T034 active

T032 provides the shared finite numeric-domain and track-axis behavior used by Slider/RangeSlider. T033 adds display-only ProgressBar/Meter widgets with finite bounded normalization performed in `double`, presentation clamp/no-writeback semantics, horizontal left-to-right and vertical bottom-to-top fill, optional presentation formatter, paint-only state invalidation and no focus/input/timer/capture behavior.

Final T033 head `149b36c063b27eea6bb0bb370de7d736110fb357` passed normal CI `34437578487`, T060 Application Contract `34437578456` and T042 Lifecycle Stress `34437578451` after the failed Linux lifecycle diagnostic job was successfully rerun on the same exact head. Mandatory final `CODE_REVIEW.md` review `5162886816` found no Blocking/Important issue. PR #123 squash-merged as `44626fef8d70f73428aa6ce906357a922cb010f5`; issue #33 is Done.

T034 / issue #34 / PR #135 is now the active existing widget/container stream. The current implementation work covers `ScrollView` over the existing `ScrollState`, wheel/pan behavior, scrollbar geometry/dragging and `ensure_visible`. Exact-head CI exposed the current RED at `tests/scroll_layout_tests.cpp:109`: a ScrollView containing non-focusable content cannot currently receive pointer-wheel targeting because the retained tree only selects focusable pointer targets. This is a real generic input-targeting seam required before T034 can be completed; do not paper over it with platform code or a hidden event pump. Mandatory review also requires scrollbar overlay input precedence over interactive content and paint ordering after content.

## Current dependency frontier

```text
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(done)
                                       +-> T034(active PR #135) -> T035 / T036

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done after PR #120)
                   T042(done) -> T051(done) ---------------------> T052

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

T034 and subsequent standard widgets belong to the state/widget lane. T065/T072/T064 and platform hardening are separate streams and must not be duplicated by widget work. After T052 merges, the platform/package lane has no further dependency-unblocked P0 package implementation until later v1 freeze/release work becomes ready.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Normal CI validates Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts and native lifecycle/platform smokes. Code-changing widget candidates also require exact-head T042 Lifecycle Stress and T060 Application Contract when those workflows are registered for the PR.

## Next actions

1. Complete the final documentation-head exact-SHA qualification and merge T052 / PR #120 if every required workflow remains green and the final `CODE_REVIEW.md` audit is clean.
2. Resume T034 / PR #135 in the independent state/widget lane; do not create a duplicate stream.
3. Continue T065 / PR #133 only in its independent platform/event lane; T072 and T064 remain downstream of it.
4. Keep later package/release work dependency-gated; do not start T069/T071 until their explicit dependency sets are complete.
