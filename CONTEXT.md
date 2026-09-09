# NativeUI compact recovery context

**Updated:** 2026-09-10

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

- Pugl: `hemduf/pugl` commit `d12d63815b8cfe3f36293d3791a418e8f558ff1b`. The independent P0 platform regression #124 / PR #125 owns any later Pugl pin change and stays outside the platform/package resource work.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` `005a207237a98726573160b8f58c9a9ebff262f9` contains the completed T054 native application package helper plus the earlier platform/package and lifecycle foundations.

Completed foundations relevant to this lane:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging with sorted immutable generated tables.

## Active platform/package ticket — T057 / issue #69 / PR #126

T057 adds the runtime-neutral lookup layer over T056 generated tables without introducing ownership, a global registry or a cache.

Current implementation contract:

- `ui::ResourceManager` borrows `std::span<const EmbeddedResourceEntry>` and performs one allocation-free O(N) validation pass;
- valid IDs are non-empty and strictly ascending by exact unsigned-byte lexicographic comparison;
- invalid tables fail atomically: direct lookup/enumeration APIs behave empty/false and retain only a small enum-backed diagnostic;
- `find()` uses binary search, is allocation-free and returns `ResourceView` spans pointing to the original storage;
- copy/move managers remain lightweight views over the same immutable storage; independent managers keep independent tables; concurrent read-only lookup needs no lock;
- `ResourceManagerProvider` is the explicit compatibility seam for existing `ResourceProvider` users and copies successful non-empty payloads into the required owned vector; that path is intentionally not real-time safe;
- ImageCache and SvgCache consume the adapter without manager-specific decoding APIs;
- the T056 external package consumer now constructs `ResourceManager` from the actual generated table in both build-tree and relocated install-tree validation;
- `t057_embedded_resources --self-test` covers direct lookup, provider copy semantics and SVG rendering; the new public header has an isolated compile probe.

TDD correction cycles covered empty-resource ownership semantics, exact generated-table byte ordering, allocation instrumentation, extensible public-header source contracts and the contain-fit SVG self-test coordinates.

Mandatory `CODE_REVIEW.md` pass on code head `2860f5ab7d1daa1e0aa2553f988a90b0f6b94ac9` found no Blocking/Important T057 finding. CI #599 has Linux ASan+UBSan and Linux X11 green while the remaining platform lanes complete. The separate T042 Linux/X11 `BadAtom` failure is owned by the excluded lifecycle/platform regression lane and is not part of T057.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057 (PR #126)
```

T059, T030, #64 and T042 are owned by other lanes and must not be taken by this platform/package lane. After T057 merges, re-read live GitHub dependencies before selecting the next platform/package ticket; do not infer readiness from milestone ordering alone.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The normal CI matrix additionally validates Linux X11, Windows/MSVC, macOS, Linux ASan+UBSan, the T047/T048/T054/T056 package contracts, relocated consumers and platform isolation checks. T057 is not mergeable until the exact documentation-complete head passes the relevant required matrix.

## Next actions

1. Finish CI feedback on the current T057 code candidate without modifying excluded T042/#124 work.
2. Commit the synchronized `CONTEXT.md`/`ROADMAP.md` completion state on the T057 branch.
3. Re-run the mandatory exact-head CODE_REVIEW.md pass and required matrix on that final completion head.
4. Refresh against live `main`; if acceptance remains satisfied and required executed checks are green, mark PR #126 ready, merge autonomously and close #69 Done.
5. Re-read the dependency graph and resume only the next dependency-unblocked platform/package ticket.
