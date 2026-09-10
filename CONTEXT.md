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

`main` contains the explicit T060 Application/multi-window ownership model, the post-T060 T042 lifecycle qualification from #139, the completed standard widgets through T033, and the completed T052 v0.1 developer-preview release/package gate from PR #120.

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

T052 qualifies the current infrastructure/package baseline as a developer preview, not as the NativeUI 1.0 product-completeness gate. Its final pre-completion candidate was synchronized from `main` `63e2603a6d85be3636347d476d2df0247f180622` and passed T052 Release Gate `34442124731`, normal CI `34442124735`, T042 Lifecycle Stress `34442124762`, T051 Release Benchmarks `34442124746`, and T060 Application Contract `34442124771`; PR #120 is now merged on `main`.

The completed gate preserves the low-level installed package contract `NativeUI::Core + nativeui_attach_platform()`, validates relocated external consumers and consumer-scoped macOS Objective-C namespaces, verifies clean-cache pinned dependency acquisition/fail-closed checksum behavior, retains the T051 exact-zero idle invalidation hard gate, and publishes `docs/releases/v0.1.0.md` with developer-preview semantics and current known v1 gaps.

## State/widget lane — T033 complete, T034 active

T032 provides the shared finite numeric-domain and track-axis behavior used by Slider/RangeSlider. T033 adds display-only ProgressBar/Meter widgets with finite bounded normalization performed in `double`, presentation clamp/no-writeback semantics, horizontal left-to-right and vertical bottom-to-top fill, optional presentation formatter, paint-only state invalidation and no focus/input/timer/capture behavior.

Final T033 head `149b36c063b27eea6bb0bb370de7d736110fb357` passed normal CI `34437578487`, T060 Application Contract `34437578456` and T042 Lifecycle Stress `34437578451` after the failed Linux lifecycle diagnostic job was successfully rerun on the same exact head. Mandatory final `CODE_REVIEW.md` review `5162886816` found no Blocking/Important issue. PR #123 squash-merged as `44626fef8d70f73428aa6ce906357a922cb010f5`; issue #33 is Done.

T034 / issue #34 / PR #135 is the active existing widget/container stream. The current implementation work covers `ScrollView` over the existing `ScrollState`, wheel/pan behavior, scrollbar geometry/dragging and `ensure_visible`. Exact-head CI exposed the current RED at `tests/scroll_layout_tests.cpp:109`: a ScrollView containing non-focusable content cannot currently receive pointer-wheel targeting because the retained tree only selects focusable pointer targets. This is a real generic input-targeting seam required before T034 can be completed; do not paper over it with platform code or a hidden event pump. Mandatory review also requires scrollbar overlay input precedence over interactive content and paint ordering after content.

## Platform geometry/input lane — T043 completion candidate

T043 / issue #43 / PR #142 is the active geometry/input stream. The completion candidate centralizes a per-view last-valid scale, physical/native versus logical conversions, configure-authoritative sizing, transient zero-extent handling, fractional dirty/input conversion and advisory preferred-size notification for standalone and embedded views.

Review corrections already incorporated include: registered dedicated T043 test/example coverage, Release-active `NUI_CHECK` assertions, a teardown-safe preferred-size dispatch token after a callback-destroys-owner regression, and restoration of the exact pinned Pugl 1..10000 view-span limit while retaining covering `ceil` rounding. The embedded native smoke verifies invalid child-size rejection, parent authority and callback-driven child resize without recursive preferred notification. T043 remains unmerged until the completion-doc head passes the exact platform/sanitizer/lifecycle/Application gates and the final `CODE_REVIEW.md` pass has no Blocking/Important finding.

After T043 merges, T044 is the preferred next ticket in this lane because its T015/T041 dependencies are already complete. T044 must first gather per-platform outside-window pointer-drag evidence before adding any native capture implementation.

## Current dependency frontier

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

T034 and subsequent standard widgets belong to the state/widget lane. T043/T044 are isolated platform geometry/input work. T065/T072/T064 are a separate platform/event stream. Later package/release work remains dependency-gated until the v1 feature/platform surface is complete.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Normal CI validates Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts and native lifecycle/platform smokes. Code-changing platform candidates also require exact-head T042 Lifecycle Stress and T060 Application Contract when those workflows are registered for the PR.

## Next actions

1. Finish T043 / PR #142 exact-head qualification and final `CODE_REVIEW.md` pass; merge only if every required executed gate is green.
2. After T043 merges, start/resume T044 / issue #44 in the geometry/input lane and establish the required per-platform evidence before implementing native capture.
3. Resume T034 / PR #135 only in the independent state/widget lane; do not duplicate it here.
4. Continue T065 / PR #133 only in its independent platform/event lane; T072 and T064 remain downstream.
5. Keep T069/T070/T071 dependency-gated until their explicit dependency sets are complete.
