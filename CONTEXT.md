# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current merged baseline

Current `main` before T032 integration is `ce86c86e663ad5f8464224874039312143146300`.

Completed foundations relevant to the current widget/release frontier:

- #64 / PR #90: standalone ownership frozen as Decision B; one application-level `PUGL_PROGRAM` world and no hidden singleton/shared-world workaround in the legacy constructor path.
- T042 / PR #93: deterministic headless/embedded/standalone lifecycle stress for every currently supported ownership path.
- T053 / T047 / T048: consumer-scoped macOS bridge plus relocatable low-level package/external-consumer qualification.
- T056 / PR #111: deterministic binary-resource packaging.
- T059 / PR #89: generic inherited visibility/enabled/read-only component state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox and typed RadioGroup/RadioButton.
- T051 / PR #116: reproducible Release benchmark harness and regression-budget policy, merged in current `main`.

T052 / issue #52 is an independent release-lane work item and is already in progress. It must not absorb or serialize the widget lane.

## T032 Slider / RangeSlider — PR #115

T032 is the current retained-state/widget completion candidate. It is refreshed from current `main` while preserving the merged T051 release infrastructure.

Delivered contract:

- one shared `detail::SliderDomain` validates finite `minimum < maximum`, finite non-negative step, quantize-then-clamp user writes and safe render-only external-state fallback;
- `ui::Slider` supports horizontal/vertical pointer capture, Arrow/Home/End keyboard editing, exact stepped/continuous increments, optional display-only formatting and T059 Disabled/Hidden/Collapsed/ReadOnly semantics;
- `ui::RangeSlider` / `RangeValue` uses the same numeric domain, selects the nearest thumb from raw pointer position before quantization, keeps the selected thumb stable for the interaction and enforces no crossing;
- external NaN/Inf/out-of-range state is made finite/clamped only for rendering/hit testing and is never silently rewritten by mount/paint;
- interaction bookkeeping is completed before synchronous `State::set()` boundaries so observer reentrancy cannot cause duplicate writes or stale `InputContext` access;
- visual state is per instance and covers Normal/Hover/Pressed/Focused/Disabled/ReadOnly;
- no platform/native control, process-global widget registry, singleton or `thread_local` state is introduced;
- dedicated value-domain, completion/reentrancy, headless visual and existing widget integration tests are registered;
- `examples/features/t032_slider.cpp` provides the required interactive example and deterministic `--self-test`.

Mandatory review found and corrected one Important defect: RangeSlider initially selected a thumb after step quantization, which could manufacture a false tie. The corrected implementation chooses from the raw pointer-domain value and only quantizes the eventual State write. The current production diff has no known Blocking/Important finding.

The branch has been structurally synchronized with current `main`; exact-head normal CI and T042 Lifecycle Stress must be green again after this synchronization/documentation update before merge.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(Doing)
platform/package:  T053(done) -> T047(done) -> T048(done) ----^
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(current) -> T037
                                       +-> T033(Ready)
                                       +-> T034(Ready) -> T035 / T036
```

After T032 merges, T037 becomes dependency-unblocked because T030 is already complete. T033 and T034 remain independent Ready widget work.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

T051's Release benchmark suite remains available independently under `tests/t051` and is consumed by T052; T032 must preserve it unchanged.

Linux CI retains X11/Xvfb/Mesa native smoke; macOS retains consumer-specific Objective-C symbol/isolation checks; sanitizer CI keeps the repository's current Skia/Fontconfig boundary policy. T042 lifecycle stress remains a separate required gate.

## Next state/widget actions

1. Require exact-head T032 normal CI and T042 Lifecycle Stress to complete green after current-main synchronization.
2. Refresh the mandatory `CODE_REVIEW.md` record against that exact head and resolve any Blocking/Important finding regression-first.
3. Merge PR #115 only when current-main synchronization, exact-head validation and final review are all satisfied; mark #32 Done/closed.
4. Move T037 to Ready if its other explicit dependencies are complete, then continue the highest-value dependency-unblocked widget work without taking platform/package/lifecycle tickets.