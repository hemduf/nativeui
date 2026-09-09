# NativeUI compact recovery context

**Updated:** 2026-09-09

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, generic state, drawing/widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio, host parameter semantics and a custom native windowing stack remain out of scope.

Non-negotiable rules: widgets/layout stay platform-neutral; public geometry is logical pixels; embedded polling is non-blocking; mutable instance-dependent process globals/singletons/thread-locals are forbidden; retained UI state is UI/main-thread confined; dependencies use CMake+CPM; Skia comes from pinned skia-builder binaries; macOS Objective-C runtime-visible platform classes must be consumer-specific.

## Pinned dependencies

- Pugl: `lv2/pugl` commit `b7637149ebe53124e5be90559e02a0185bbcbd73`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Merged baseline

The merged baseline is complete through T029, including rendering resources T022/T023 and platform safety issue #62. TextInput/TextArea share the UTF-8 `TextEditModel`; T029 provides platform-neutral IME composition plus private Cocoa/IMM32/XIM bridges. `StandaloneWindow` uses `PUGL_PROGRAM`; `EmbeddedView` uses `PUGL_MODULE` and non-blocking polling. General clipboard text uses canonical `text/plain`.

Known independent limitation: multiple simultaneous `StandaloneWindow`/`PUGL_PROGRAM` worlds can crash on macOS; tracked in #64. Independent `EmbeddedView`/`PUGL_MODULE` multi-instance validation is green. Do not conflate #64 with T053.

## T053 — consumer-scoped macOS platform bridge

Issue #65 / PR #88 is the active platform/package lane.

T053 replaces the old global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` build contract. The architecture is now:

```text
NativeUI::Core                         generic, compiled once
Pugl common.c + internal.c            generic on macOS, compiled once
mac.m + mac_gl.m + Cocoa IME bridge   compiled per final consumer
```

Each final consumer has one stable non-empty `CONSUMER_ID`. `cmake/NativeUIConsumerPlatform.cmake` is the single source of truth for the frozen prefix algorithm:

```text
exact UTF-8 CONSUMER_ID bytes
  -> SHA-256
  -> first 12 lowercase hex
  + ASCII-only readable fragment (max 24)
  -> NUI_<fragment>_<digest12>_
```

Configure-time bookkeeping rejects duplicate target attachment and reuse of one identity by two different final targets. It is CMake generation state only; no runtime registry is emitted.

On macOS every consumer bridge compiles renamed `PuglWindow`, `PuglWindowDelegate`, `PuglWrapperView` and `PuglOpenGLView`. The symbol audit requires both class and metaclass symbols and rejects any unprefixed `Pugl*` Objective-C class/metaclass, so future pinned-source additions cannot silently escape the gate. The existing Cocoa IME dynamic subclass derives its name from the already consumer-prefixed Pugl class and remains per-view.

TDD evidence is recorded in PR #88: prefix vectors preceded the derivation helper; duplicate-identity/target failure tests preceded registration bookkeeping; integration then split generic Pugl C code from per-consumer Objective-C bridges. Vectors cover punctuation collisions, Unicode bytes, empty-after-sanitize, 24-character truncation and clean-configure determinism.

CODE_REVIEW.md review on PR #88 is clean: instance isolation, configure-time globals, threading/RT, lifetime/reentrancy, Objective-C runtime safety and platform integration have no remaining Blocking/Important finding. NativeUI-owned AppKit `MAX` macro use was removed after warning review and Objective-C bridge visibility was hardened.

Exact implementation head `fad1d96a17902fa18ee25187ed965ac212b7c89b` passed CI run #287 (`34320340643`) on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan. The macOS lane additionally passed the two-consumer symbol/runtime coexistence fixture and clipboard/multi-instance lifecycle smoke. Final documentation synchronization changed the branch afterward, so merge requires the same matrix to be green on the final exact documentation head after refreshing from current `main`.

## Packaging frontier

T047 / issue #47 depends on T053 and is the next task for this lane immediately after #65 merges. T047 owns the installed/public low-level contract:

```cmake
find_package(NativeUI CONFIG REQUIRED)

nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

T047 must export `NativeUI::Core`, install the T053 machinery relocatably, validate final target type + reverse-DNS identity, reject double attachment, preserve build-tree/install-tree parity, and prove external install-tree consumers on macOS/Windows/Linux. It must not export/document `NativeUI::NativeUI` as the complete v1 package target.

After T053, dependency state is:

```text
T053 complete -> T047 Ready
T047 + T053 -> T054
T047 -> T048 -> T052
T047 -> T056 -> T057
```

Other active lanes (T059, T030, #64, T042) are intentionally not owned by this platform/package lane.

## Build / validation

Normal source-tree build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No global `NATIVEUI_OBJC_RUNTIME_PREFIX` is required. NativeUI-owned source-tree final targets register their consumer identities internally. Core-only builds use `-DNATIVEUI_BUILD_PLATFORM=OFF`.

Offline dependency overrides remain:

```bash
cmake -S . -B build \
  -DNATIVEUI_PUGL_SOURCE=/path/to/pugl \
  -DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder
```

## Next action

1. Refresh PR #88 from current `main`, resolving documentation conflicts without dropping parallel-lane changes.
2. Require final exact-head Linux X11, Windows/MSVC, macOS (including two-consumer Objective-C runtime isolation + platform smoke), and Linux ASan+UBSan to pass.
3. Finalize PR/issue review/status, merge #88, close #65 Done and keep `ROADMAP.md` synchronized in that merge.
4. Immediately move T047/#47 from Blocked to Doing and continue its install/export package TDD stream.