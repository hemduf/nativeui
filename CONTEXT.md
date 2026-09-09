# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral;
- public geometry is logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent process globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined;
- dependencies use CMake + CPM;
- Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific;
- `CODE_REVIEW.md`, exact-head validation, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current merged baseline

Current `main` before the T042 merge is `8da758242fa3b2c4fa2402c7a08a33b05619440a`.

Important completed work:

- T053 / PR #88: consumer-scoped macOS platform bridge, merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`.
- #64 / PR #90: Decision B ownership boundary for standalone PROGRAM worlds, merged as `b52d53eee65e5de91697e2c1f0685728b9646575`.
- T059 / PR #89: generic component availability, merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`.
- T030 / PR #94: Button, merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`.
- T047 / PR #92: relocatable low-level package + `nativeui_attach_platform()`, merged as `df569e874539aaafb8600960938465f733a46f19`.
- T048 / PR #99: relocated external-consumer qualification, merged as `ed81a201ea459ea2443ae51f27dfcac7af5d7e63`.
- #103 / PR #104: Linux/X11 native Skia GL integration, merged as `b6515836cd8bf571c66f5297a76fdb80d9924f83`.
- #105 / PR #106: constructor-time platform callback lifetime fix, merged as `b1dd7393b95adc4667224cd3da4cfb15dfc3f746`.
- #86 / PR #87: reviewed Pugl drag/drop integration and real Finder delivery correction, merged as `0c4778278c86d2b8946ece593e684f4203ba38db`.
- #107 / PR #108: documented non-fatal standalone raise handling, merged as `e05ae703509a6971772b3bd5c53a3d7c71d7632a`.
- T031 / PR #95: Checkbox + typed `RadioGroup<T>` / `RadioButton<T>`, merged as `8da758242fa3b2c4fa2402c7a08a33b05619440a`.

T031 keeps radio-group identity and duplicate-live-value bookkeeping group-owned, with no mutable global/singleton/`thread_local` registry. It preserves T059 Disabled/Hidden/Collapsed/ReadOnly behavior and the shared Button-family activation contract.

## T042 lifecycle/stress completion — PR #93

T042 converts lifecycle and instance-isolation requirements into deterministic stress fixtures for the currently supported ownership model:

1. `headless_tree_lifecycle_1000`;
2. `embedded_sequential_100`;
3. `embedded_two_live_50` with destroy-A/continue-B isolation;
4. `embedded_capture_focus_teardown` with active focus/text/capture;
5. `standalone_sequential_50` using process-isolated PROGRAM lifetimes;
6. `standalone_supported_multi_instance`, which explicitly records #64 Decision B and leaves shared top-level multi-window ownership to T060.

The harness uses bounded deterministic pumps/timeouts, no random sleeps, no broad sanitizer suppression, and no hidden Application/singleton workaround. A+B fixtures independently exercise state, focus, capture, invalidation and clipboard-request bookkeeping; weak sentinels reject stale callbacks after destruction.

Production defects found while building the matrix were split and fixed independently in #103, #105 and #107. PR #93 is therefore stress/workflow + completion documentation only; it does not carry those production fixes.

Mandatory review `5155464378` is clean:

- instance isolation: PASS;
- globals/statics: PASS;
- threading/RT: PASS;
- lifetime/reentrancy: PASS;
- Objective-C runtime: PASS / no new runtime surface;
- platform integration: PASS;
- remaining Blocking/Important findings: none.

Validation history before the final T031 synchronization:

- T042 Lifecycle Stress `34362009081` / run #11: GREEN on Linux X11, Windows, macOS and Linux ASan+UBSan on head `acd86e2e678f3a3f3dc7596164f46b3b9902dac4`.
- Normal CI `34362008979` / run #435: GREEN on the same head after a macOS job rerun. The first macOS attempt had one unchanged T047 configure-contract timeout at 30.17 s; the same script had passed directly in the same job and the exact rerun passed CTest without changing the test, timeout or assertions.

While those gates were running, T031 / PR #95 advanced `main`. T042 was therefore synchronized again from `8da758242fa3b2c4fa2402c7a08a33b05619440a`; the final post-sync head must rerun both normal CI and the dedicated T042 lifecycle matrix before merge.

## Current DAG frontier

```text
lifecycle:        #64(done) -> #107(done) -> T042(this merge) -> T051 -> T052
platform/package: T053(done) -> T047(done) -> T048(done) -> T052
                                      |
                                      +-> T054
                                      +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031(done)
                                      |
                                      +-> T032
                                      +-> T033
                                      +-> T034 -> T035 / T036
```

After T042 merges:

- T051 becomes Ready because T024 and T042 are complete;
- T052 remains blocked only by T051 because T047/T048/T042 are satisfied;
- T054 and T056 remain independent Ready platform/package work;
- T057 remains dependent on T056;
- T032, T033 and T034 remain independent widget work after merged T031.

## Platform ownership reminders

- Normal standalone validation currently uses one `PUGL_PROGRAM` owner lifetime.
- Multiple independent `EmbeddedView` / `PUGL_MODULE` instances are supported and remain the current host/plugin coexistence path.
- T060 owns the future shared `ui::Application` multi-window standalone model.
- No global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` is part of the normal package contract.

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

T047/T048 package validation additionally exercises source contracts, relocation, external Core/standalone/embedded consumers and macOS Objective-C consumer isolation. Linux CI retains the Xvfb/Mesa llvmpipe renderer/lifecycle smoke introduced by #103.

## Next actions after T042

1. Close #42 as Done after the exact final head passes normal CI and T042 Lifecycle Stress and PR #93 is merged.
2. Change T051 from Blocked to Ready.
3. Continue independent Ready work according to priority and downstream unblock value; do not serialize unrelated widget/platform lanes behind lifecycle work.