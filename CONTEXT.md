# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent process globals/singletons/`thread_local` state are forbidden;
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

Current `main` before T032 merges is `3a87070ae1b236a9d68f73e20f489ca5256da2ae`.

Important completed work relevant to the current frontier:

- T053 / PR #88: consumer-scoped macOS platform bridge.
- #64 / PR #90: standalone PROGRAM-world ownership Decision B.
- T059 / PR #89: generic component availability/read-only model.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed `RadioGroup<T>` / `RadioButton<T>`.
- T042 / PR #93: deterministic supported-path lifecycle stress.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)`.
- T048 / PR #99: relocated external Core/standalone/embedded consumer qualification.
- T056 / PR #111: deterministic CMake binary resources, merged as `3a87070ae1b236a9d68f73e20f489ca5256da2ae`.

T056 exact-head qualification is green on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus the dedicated T042 lifecycle matrix. Its last CMake correction only changes the T047 CTest timeout from 30s to 60s because the unchanged macOS contract legitimately needs about 41s.

## T032 Slider / RangeSlider — PR #115

T032 is the current widget completion candidate. The branch was structurally refreshed from T056-complete `main` rather than replaying its stale umbrella/CMake files, so merged T031/T042/T056 work is preserved.

### Shared numeric domain

`detail::SliderDomain` is the sole numeric policy for Slider and RangeSlider:

- finite `minimum < maximum` and finite non-negative optional step are required;
- user edits normalize by quantize-then-clamp;
- continuous keyboard increment is exactly 1% of the range and Shift uses 0.1% of the range; stepped widgets use exactly one step and Shift has no effect;
- external finite values clamp only for display and are not silently step-snapped;
- external NaN/Inf maps to a deterministic safe effective value for geometry without rewriting bound State during mount/paint.

### Slider

`ui::Slider` binds directly to `State<float>&` and supports:

- horizontal/vertical pointer mapping, drag capture, release/cancel;
- Arrow/Home/End keyboard editing through the same domain;
- T059 Disabled/Hidden/Collapsed availability and ReadOnly consume-without-write semantics;
- optional `formatter(...)` which is presentation-only and receives the effective displayed value;
- explicit retained visual states Normal/Hover/Pressed/Focused/Disabled/ReadOnly;
- all hover/drag/visual bookkeeping per component instance.

Interaction/capture/invalidation bookkeeping is completed before synchronous `State::set()` calls so an observer may immediately change availability without a duplicate write or post-callback use of stale input context.

### RangeSlider

`ui::RangeSlider` binds to `State<RangeValue>&` and reuses the same numeric domain. It:

- clamps/falls back/sorts external state only for effective rendering and never rewrites it merely because it is out of range/non-finite;
- selects the nearest thumb from the raw pointer position before quantizing the value written to State; exact ties keep the previous active thumb or default to lower;
- retains active thumb identity through drag;
- prevents thumb crossing for every user write;
- supports horizontal/vertical pointer input plus Arrow/Home/End keyboard editing;
- renders two thumbs and the selected interval using the same six-state visual policy;
- keeps drag/hover/active-thumb state per instance.

### Completion regressions and example

The completion candidate registers and executes:

- existing `widget_tests.cpp` Slider/RangeSlider input/T059 coverage;
- `t032_slider_value_contract.cpp` for the pure domain;
- `t032_slider_completion_tests.cpp` for NaN/Inf render-only behavior, reentrancy, Hidden/Collapsed capture cancellation, raw-position nearest-thumb selection and formatter no-write behavior;
- `t032_slider_visual_tests.cpp` for the six state variants, focus/hover/press, both orientations and two-thumb RangeSlider geometry;
- `examples/features/t032_slider.cpp` as the required interactive feature demo and deterministic `--self-test`.

No platform-native widget/control, mutable process-global registry, singleton or `thread_local` state is introduced by T032.

## Current DAG frontier

```text
lifecycle:        #64(done) -> T042(done) -> T051(Ready) -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054(Ready)
                                      +-> T056(done) -> T057(Ready)
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032(this merge) -> T037
                                      +-> T033(Ready)
                                      +-> T034(Ready) -> T035 / T036
```

After T032 merges, T037 becomes Ready because T030 is already complete. T033/T034 remain independent Ready widget work. Package/release work remains independent.

## Build / validation

Source-tree Release:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Offline dependency overrides:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

The final T032 merge candidate must pass normal CI on Linux X11/Windows/macOS/Linux ASan+UBSan plus the existing T042 Lifecycle Stress matrix. The normal platform jobs also keep T047/T048/T056 package/relocation gates and native platform smokes, so the widget merge cannot silently regress current packaging/platform ownership.

## Next actions

1. Complete exact-head T032 normal CI and T042 Lifecycle Stress after these documentation changes.
2. Perform the mandatory exact-head `CODE_REVIEW.md` pass and resolve any Blocking/Important finding regression-first.
3. Merge PR #115, close #32 as Done and change T037 / #37 from Blocked to Ready.
4. Continue independent Ready work by repository priority/dependency-unblock rules; do not serialize package/platform work behind the widget lane.
