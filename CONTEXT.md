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

`main` `c5270a1a971d1d409a53b3715df0340fc445fb33` contains the completed T060 explicit Application/multi-window ownership model and its completion-context synchronization on top of the reviewed Pugl X11 correction.

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

## Lifecycle qualification update — #139 / PR #140

T042's `standalone_supported_multi_instance` gate is being upgraded from the pre-T060 Decision-B marker to an executable stress of the now-supported T060 ownership path. The fixture keeps one explicit `ui::Application` / one `PUGL_PROGRAM` world alive across 50 deterministic A+B cycles, validates distinct native windows, independent resize/close state, destroys A while B survives and continues polling/resizing, and uses `QuitPolicy::ExplicitOnly` to exercise zero-window intervals without inventing a hidden singleton, mutable process-global owner or `thread_local` owner.

The legacy process-isolated standalone constructor stress remains as compatibility coverage until T069; independent simultaneous legacy `PUGL_PROGRAM` worlds remain forbidden by #64 Decision B. Embedded T042 stress remains unchanged.

Initial GREEN head `1b917ec157a938b997e0afc6979454495b90effb` passed T060 Application Contract `34431532321`, T042 Lifecycle Stress `34431532386` on Linux/X11, Windows, macOS and Linux ASan+UBSan, and normal CI `34431532332`. Mandatory review then found stale pre-T060 diagnostic wording and missing direct GDB/LLDB diagnosis for the new Application stress path. The correction stream updates those diagnostics before final exact-head qualification. #139 remains a blocking validation dependency for T052 / PR #120; after #139 merges, T052 must refresh from that new `main` and rerun its release qualification set.

## Current platform/package frontier

```text
T053(done) -> T047(done) -> T048(done)
                         |-> T054(done)
T056(done) + T022(done) -> T057(done)

#64(done) -> T060(done) -> T065(active PR #133) -> T072 -> T064
                     |
                     +-> #139(active PR #140; T042 post-T060 qualification)
                     +-> T066 (also depends on T043)

T043 ready
T044 ready
```

T059, T030 and unrelated widget work remain owned by their parallel lane and must not be folded into platform/lifecycle work. T052 is the separate release gate. T032/T033/T034 are widget-lane work. T060 is merged, so T065 no longer needs to wait for the Application ownership architecture; its existing PR must be synchronized with current `main` before native dispatcher exposure/wake integration continues.

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

1. Finish #139 / PR #140 exact-head diagnostic correction, final review and lifecycle/platform qualification; merge only when all required gates are green.
2. After #139 merges, T052 / PR #120 must refresh from the new lifecycle-qualified `main` before release qualification can complete.
3. Keep the existing T065 PR #133 as its own stream; do not duplicate it.
4. Keep unrelated widget/package lanes untouched.
