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
- NativeUI-owned source-tree targets compile with zero unapproved warnings; `NATIVEUI_ALLOWED_WARNINGS` is empty by default and is the only normal explicit CMake opt-in for a temporarily accepted diagnostic;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` is `96dc57a2cc3395fcb00f5cd2c0398f3da60b4675`. It includes T065 / PR #133 in addition to T034 / PR #135, the supported T060 multi-window lifecycle model, post-T060 T042 lifecycle qualification, standard widgets through T034 and the T052 v0.1 developer-preview release gate.

Cross-cutting rendering regression #152 / PR #153 removes Tree-owned visual decoration: `Tree::paint()` no longer forces `colors::background` across the viewport and no longer draws the hard-coded keyboard/mouse help line. Generic retained-tree painting is therefore consumer/component-owned. The headless renderer now mirrors the GPU renderer's black framebuffer clear, and its regression verifies that an otherwise empty Tree adds no styled background or instructional overlay beyond that renderer-level clear.

Relevant completed foundations:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B for the pre-v1 compatibility path.
- T059 / PR #89: inherited visibility/enabled/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T032 / PR #115: Slider + RangeSlider.
- T033 / PR #123: ProgressBar + Meter.
- T034 / PR #135: ScrollView.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T052 / PR #120: v0.1 developer-preview release gate.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus allocating provider adapter.
- T060 / PR #118: one explicit `ui::Application` owns one standalone `PUGL_PROGRAM` world and multiple independent `StandaloneWindow(Application&, ...)` views.
- #139 / PR #140: T042 stress qualification of the supported T060 multi-window path.
- T065 / PR #133: bounded per-owner UI dispatcher/timers plus platform-native standalone wake integration.

## #163 / PR #181 — warning-free source-tree builds

#163 / PR #181 is the active build-quality incident on top of current `main`.

Delivered behavior on the branch:

- shared feature examples use the v1 `Application + StandaloneWindow(Application&, UI&, WindowDesc)` ownership path, eliminating the repeated `StandaloneWindow` deprecation warning;
- `examples/standalone.cpp`, T041 and supported standalone/embedded smoke paths use the same v1 Application ownership model;
- NativeUI-owned source-tree builds enable a strict warning level and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`;
- `NATIVEUI_ALLOWED_WARNINGS` is a semicolon-separated CMake cache setting with an empty default; Clang/GCC diagnostics are named without `-W`, MSVC diagnostics use four-digit codes;
- malformed warning identifiers fail configuration;
- the historical #64 legacy independent-PROGRAM diagnostics are excluded from the strict default build and become available only when `deprecated-declarations` is explicitly approved through `NATIVEUI_ALLOWED_WARNINGS`;
- repository workflow and review rules classify an unapproved NativeUI compiler warning as Blocking and forbid target/source-local suppression as a substitute for the explicit CMake opt-in.

The strict build immediately exposed and corrected existing warnings instead of whitelisting them: the example deprecation uses, MSVC C4458 parameter shadowing, GCC `-Wsubobject-linkage` caused by platform implementation types with anonymous linkage, and MSVC C4244 narrowing in the slider value-contract test. The default warning allowlist remains empty.

After synchronizing PR #181 with the current `main`, all required CI, T060, T065, T042 and T052 gates must pass on the resulting exact head before merge.

## Current dependency frontier

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

critical platform: T060(done) -> T065(done) -> T072 -> T064
                   T041(done) -> T043(active PR #142) -> T066
                                              |-------> T068
                   T065 + T072 + T043 + remaining feature deps -> T068 -> T069
```

T072 is now dependency-unblocked by T065. T064 depends on T065 and T072. T066 depends on T060 and T043. T043 may progress independently. T044 is not on the T068/T069 critical path and remains lower priority until those prerequisites are complete.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The default build must use an empty `NATIVEUI_ALLOWED_WARNINGS`. A temporary approved Clang/GCC exception is explicitly configured, for example:

```bash
cmake -S . -B build -DNATIVEUI_ALLOWED_WARNINGS=deprecated-declarations
```

Do not add an exception when the warning can be fixed in NativeUI-owned code.

## Next actions

1. Complete exact-head qualification and final `CODE_REVIEW.md` review for #163 / PR #181; merge only if every required executed gate is green and no Blocking/Important finding remains.
2. After #163 merges, keep T072 / issue #84 as the next critical platform prerequisite unlocked by T065.
3. Advance existing T043 / PR #142 independently; it remains required by T066 and T068.
4. Keep unrelated widget/style/dynamic/accessibility/release work in separate lanes and preserve explicit dependency gates.
