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

## Current baseline

Current `main` is `ce86c86e663ad5f8464224874039312143146300`, including merged T051 / PR #116.

Completed foundations relevant to the current widget lane:

- T059 / PR #89: generic inherited visibility/enabled/read-only availability model.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox + typed RadioGroup/RadioButton.
- T024: deterministic headless raster/golden foundation.
- T042 / PR #93: deterministic lifecycle stress used as a cross-lane exact-head gate.
- T051 / PR #116: reproducible Release performance benchmark/regression harness; T052 is now dependency-unblocked in the separate release lane.

## T033 ProgressBar / Meter — PR #123

T033 is the current retained-widget completion candidate. It adds display-only bounded-value widgets driven by externally owned `State<float>`.

Delivered contract:

- `ProgressBar` and `Meter` share one bounded display domain with finite `minimum < maximum` validation;
- finite external values clamp only for presentation; NaN/Inf display at the minimum fallback; application State is never normalized or rewritten by mount/paint;
- horizontal fill is left-to-right and vertical fill is bottom-to-top;
- widgets are non-focusable, ignore input, capture no pointer, schedule no timer and perform no hidden smoothing;
- State observation produces paint invalidation only while geometry is unchanged and unmount releases the subscription;
- optional formatter receives the effective presentation value and is presentation-only;
- deterministic headless coverage exercises minimum, midpoint, maximum, finite out-of-range and NaN states in both orientations;
- raster assertions compare semantic empty/full reference states captured by the same Skia surface, avoiding backend color-space byte assumptions;
- dedicated `examples/features/t033_progress_meter.cpp` provides interactive controls plus deterministic `--self-test`.

The exact-head review found no mutable global/singleton/`thread_local` state, no platform header/runtime leakage, no lifetime/capture/timer hazard and no application-State writeback. Normal CI and T042 lifecycle stress remain the final exact-head gates after completion-document synchronization.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052(ready)
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054
                                       +-> T056(done) -> T057
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |
                                       +-> T032(in review)
                                       +-> T033(completion candidate)
                                       +-> T034(ready) -> T035 / T036
```

T032 remains a separate existing widget stream and must be resumed rather than duplicated. T034 is the next currently Ready widget ticket once existing in-progress widget work is resolved according to `AGENTS.md`.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Linux CI retains X11/Xvfb/Mesa native smoke; macOS retains consumer-specific Objective-C symbol/isolation checks; sanitizer CI keeps the repository's current Skia/Fontconfig boundary policy. T042 lifecycle stress remains a separate exact-head gate.

## Next actions

1. Require exact-head normal CI and T042 lifecycle stress to complete green on the final T033 documentation-synchronized head.
2. Record the final mandatory `CODE_REVIEW.md` pass against that exact head and resolve any Blocking/Important finding.
3. Merge PR #123, mark #33 Done/closed, then re-evaluate the existing T032 stream before starting another widget ticket.
4. If T032 remains blocked by unrelated platform integration state, preserve lane ownership and continue the next dependency-unblocked widget work without modifying platform scope.
