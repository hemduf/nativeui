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

Current `main` before the T060 integration is `ce86c86e663ad5f8464224874039312143146300`.

Completed lifecycle/platform foundations relevant to T060:

- #64 / PR #90: **Decision B** — a standalone application owns one explicit application-level `PUGL_PROGRAM` world; multiple independent PROGRAM worlds are not the supported multi-window model.
- T042 / PR #93: deterministic supported-path lifecycle stress for headless, embedded and legacy standalone ownership paths.
- T053 / T047 / T048: consumer-scoped macOS bridge plus relocatable low-level package/external-consumer qualification.
- T051 / PR #116: deterministic performance harness and regression policy; T052 is now independently in progress as the v0.1 release gate.

## T060 explicit Application ownership — PR #118

T060 is the current lifecycle/stress-lane implementation candidate. It turns #64 Decision B into the public standalone ownership model without introducing a hidden singleton or process-global application registry.

Delivered contract on the current branch:

- `ui::Application` is non-copyable/non-movable, owns exactly one `PUGL_PROGRAM` world and is UI/main-thread confined;
- v1 standalone construction is `StandaloneWindow(Application&, UI&, WindowDesc)`; explicit windows borrow the Application world and keep their own UI/ViewCore state;
- Application-local registration uses monotonically increasing IDs and no raw process-global/window registry;
- Application must outlive every attached standalone window; destroying it with a registered live window terminates deterministically rather than allowing a dangling borrowed world;
- `Application::poll()` maps negative/zero/positive finite timeouts to blocking/non-blocking/bounded Pugl updates and rejects NaN/Inf as terminal errors;
- `QuitPolicy::OnLastWindowClosed` is the default; `ExplicitOnly` suppresses last-window auto-quit;
- `request_quit()` is idempotent, makes the Application non-runnable and does not destroy/close live windows;
- PUGL native close and NativeUI quit-key paths notify the same per-Application window bookkeeping;
- embedded views remain independent `PUGL_MODULE` instances with non-blocking host polling;
- the old `StandaloneWindow(UI&, ...)` constructor remains only as a deprecated pre-v1 compatibility path and still owns its own world; it does not hide a shared Application.

Dedicated `tests/t060` acceptance coverage includes simultaneous A+B, destroy-A/continue-B, repeated secondary-window creation/destruction, ExplicitOnly, direct and callback-requested quit, negative/zero/positive polling, NaN/Inf rejection, rejected-window behavior on a terminal Application and deterministic Application-before-window lifetime failure. `examples/features/t060_multi_window_application.cpp` supplies interactive and `--self-test` coverage.

A dedicated `T060 Application Contract` workflow validates Linux/X11, Windows/MSVC, macOS and Linux ASan+UBSan. Earlier RED runs exposed and corrected missing outer-project Pugl-source resolution and a callback-test setup dependency on native configure/focus timing; the callback fixture now establishes retained layout/activation directly so it tests callback reentrancy rather than an OS focus race.

## Current DAG frontier

```text
standalone lifecycle: #64(done) -> T042(done) -> T060(in review)
release baseline:     T042(done) + T047(done) + T048(done) + T051(done) -> T052(doing)
platform/package:     T053(done) -> T047(done) -> T048(done)
state/widgets:        T059(done) -> T030(done) -> T031(done)
```

T060 remains separate from T052: T052 explicitly does not require the multi-window Application API for the v0.1 developer-preview gate.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Dedicated T060 native acceptance project:

```bash
cmake -S tests/t060 -B build-t060 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_SOURCE_DIR="$PWD"
cmake --build build-t060 --parallel
ctest --test-dir build-t060 --output-on-failure
```

Linux runs the native tests under Xvfb/Mesa; macOS and Windows use their native platform backends. The dedicated sanitizer configuration enables Linux ASan+UBSan for the same T060 contract.

## Next actions

1. Require the exact current T060 head to pass the dedicated Linux/X11, Windows, macOS and Linux ASan+UBSan contract matrix plus the repository normal CI and T042 lifecycle-stress workflow.
2. Complete the mandatory `CODE_REVIEW.md` pass against that exact head and resolve every Blocking/Important finding.
3. Update `DESIGN.md` and `ROADMAP.md` with the final explicit Application ownership contract.
4. Refresh from current `main` again if it advances, revalidate the exact merge candidate, then merge PR #118 and mark #72 Done/closed.
