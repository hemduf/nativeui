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

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`, including the reviewed X11 failed-selection guard from #124 / PR #125.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline

`main` `352bdf0e734df46e8edcb53a0a81a07c9e0d7d6d` contains the completed T060 explicit Application/multi-window ownership model on top of the reviewed Pugl X11 correction.

Completed foundations relevant to the platform/package lane:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus explicit allocating provider adapter.
- #124 / PR #125: reviewed Pugl X11 `SelectionNotify.property == None` correction pinned into NativeUI.
- T060 / PR #118: one explicit `ui::Application` owns exactly one standalone `PUGL_PROGRAM` world; explicit `StandaloneWindow(Application&, ...)` instances borrow that world while retaining independent per-window UI/render/input state. `EmbeddedView` remains independent `PUGL_MODULE` ownership. The legacy standalone constructor remains pre-v1 only for later T069 removal.

T060 exact merge candidate `0e4cce56bd8874545794fdf1137d1c7ec5489dde` passed T060 Application Contract `34422787634`, T042 Lifecycle Stress `34422787683`, and normal CI `34422787695`. Final `CODE_REVIEW.md` review `5161881319` reported no Blocking/Important finding. PR #118 merged as `352bdf0e734df46e8edcb53a0a81a07c9e0d7d6d`; issue #72 is closed Done.

## Current platform/package frontier

```text
T053(done) -> T047(done) -> T048(done)
                         |-> T054(done)
T056(done) + T022(done) -> T057(done)

#64(done) -> T060(done) -> T065(active PR #133) -> T072 -> T064
                     |
                     +-> T066 (also depends on T043)

T043 ready
T044 ready
```

T059, T030, #64 and T042 are owned by other parallel lanes and must not be folded into unrelated package work. T052 is the separate release/lifecycle gate. T032/T033 are widget-lane work. T060 is now merged, so T065 no longer needs to wait for the Application ownership architecture; its existing PR must be synchronized with current `main` before native dispatcher exposure/wake integration continues.

## Active work — T065 / issue #77 / PR #133

The existing T065 stream already delivers the bounded Core dispatcher/timer engine and root integration. Its current reviewed contract includes per-owner bounded queues/timers, deterministic fake time, FIFO ordering, finite drain fairness, owner shutdown semantics and no process-global dispatcher.

Remaining T065 work after T060 merge:

1. refresh PR #133 from current `main` without duplicating the branch or widening scope;
2. expose one dispatcher per `StandaloneWindow` and one independent dispatcher per `EmbeddedView` while preserving T060 shared-world ownership;
3. implement the smallest thread-safe standalone wake mechanism and preserve embedded host-driven non-blocking polling;
4. add native wake/multi-owner/lifecycle coverage and the required `t065_ui_dispatcher` feature example/self-test;
5. run targeted T065 tests, normal CI, T042 regression coverage and Linux ASan+UBSan on the exact final head;
6. perform the mandatory final `CODE_REVIEW.md` pass, synchronize issue/CONTEXT/ROADMAP, refresh from current main and merge only when exact-head gates are green.

T072 remains blocked by T065. T064 remains blocked by T065 and T072.

## Build / validation

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The normal CI matrix additionally validates Linux X11, Windows/MSVC, macOS, Linux ASan+UBSan, package/relocation contracts, macOS consumer isolation and feature/platform smokes. Platform/lifecycle tickets additionally run their dedicated exact-head workflows where defined.

## Next actions

1. Resume existing T065 PR #133; do not create a duplicate implementation stream.
2. Synchronize it with T060-complete current main and re-check the exact diff before platform integration.
3. Continue strict RED -> GREEN -> REFACTOR cycles for native dispatcher exposure and wake semantics.
4. Keep T059, T030, #64, T042 and unrelated widget/release lanes untouched.
5. After T065 is merged, re-evaluate T072 as the next dependency-unblocked platform/package item; T064 remains explicitly excluded until its dependencies are complete and lane ownership permits it.
