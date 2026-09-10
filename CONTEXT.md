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

`main` `c2cc83b35ee8cdf469df03d40a93ca2194e6923f` contains the completed T057 ResourceManager in addition to the established platform/package foundations.

Completed foundations relevant to this lane:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging with sorted immutable generated tables.
- T057 / PR #126: immutable non-owning `ResourceManager` plus explicit allocating `ResourceManagerProvider` adapter.

## T057 completed state

T057 is merged and issue #69 is closed Done.

Delivered contract:

- `ui::ResourceManager` borrows `std::span<const EmbeddedResourceEntry>` and performs one allocation-free O(N) validation pass;
- valid IDs are non-empty and strictly ascending by exact unsigned-byte lexicographic comparison;
- invalid tables fail atomically: direct lookup/enumeration APIs behave empty/false and retain only a small enum-backed diagnostic;
- `find()` uses binary search, is allocation-free and returns `ResourceView` spans pointing to original storage;
- copy/move managers remain lightweight immutable views with no registry/cache/mutex; independent managers remain isolated and concurrent read-only lookup is safe for live immutable backing storage;
- `ResourceManagerProvider` is the explicit compatibility seam for `ResourceProvider`; successful non-empty loads copy into the owned vector and are intentionally not real-time safe;
- ImageCache and SvgCache consume the adapter without manager-specific decoding APIs;
- the actual T056 generated table is consumed by build-tree and relocated install-tree external consumers;
- `t057_embedded_resources --self-test` covers direct lookup, provider copy semantics and SVG integration; the public header has an isolated compile probe.

TDD correction cycles covered empty-resource ownership semantics, exact generated-table byte ordering, allocation instrumentation, extensible public-header contracts and deterministic SVG contain-fit self-test sampling.

Final evidence:

- exact PR head `20ec256241c9419b5a4d60f8f68968f4433d2855`;
- CI #600 passed Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan plus package/relocation contracts;
- final `CODE_REVIEW.md` pass reports no Blocking/Important finding and no unresolved review thread;
- PR #126 merged as `c2cc83b35ee8cdf469df03d40a93ca2194e6923f`;
- issue #69 is closed with `status:done` and a completed review/validation record.

## Current DAG frontier

```text
lifecycle/release: #64(done) -> T042(done) -> T051(done) -> T052
platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |
                                       +-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)
```

T059, T030, #64 and T042 are owned by other lanes and must not be taken by this platform/package lane. #124 / PR #125 is also an independent Pugl/X11 regression stream and must not be folded into unrelated package/resource work.

The next platform/package selection must be made from live GitHub dependency/status data. T064/T072 remain blocked by T065, while T065 is dependency-ready but shares an event-loop integration seam with the active T060 stream; re-check current branches/PRs and conflict risk before starting it. Platform-hardening tickets such as T043/T044 are separate issue scopes and should only be taken if they are the live unowned choice for this lane.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The normal CI matrix additionally validates Linux X11, Windows/MSVC, macOS, Linux ASan+UBSan, T047/T048/T054/T056 package contracts, relocated consumers and platform isolation checks. T057 exact-head CI #600 is green in every required lane.

## Next actions

1. Keep T057 closed unless a real regression is discovered.
2. Re-read live open PRs/issues before selecting the next platform/package item; resume existing work rather than duplicating it.
3. Do not absorb T059, T030, #64, T042 or the independent #124 Pugl regression.
4. Prefer a dependency-unblocked platform/package item that does not conflict with an active parallel branch; if T060 remains active, reassess T065 overlap before opening a competing implementation.
5. Apply strict TDD, mandatory `CODE_REVIEW.md`, exact-head platform validation, and `CONTEXT.md`/`ROADMAP.md` synchronization on the selected ticket.