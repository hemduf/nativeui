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

`main` includes the completed T060 explicit Application/multi-window ownership model and T032 Slider/RangeSlider. T032 / PR #115 was squash-merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1` after exact-head CI, T042 Lifecycle Stress and T060 Application Contract all passed.

Completed foundations relevant to current parallel lanes:

- T053 / PR #88: consumer-scoped macOS Objective-C bridge identity.
- T047 / PR #92: relocatable low-level package exposing `NativeUI::Core` plus `nativeui_attach_platform()`.
- T048 / PR #99: relocated external consumers and macOS two-consumer isolation.
- T051 / PR #116: reproducible Release benchmark and regression policy.
- T054 / PR #119: `nativeui_add_application()` high-level native application package helper.
- T056 / PR #111: deterministic `nativeui_add_binary_data()` packaging.
- T057 / PR #126: immutable non-owning `ResourceManager` plus explicit allocating provider adapter.
- #124 / PR #125: reviewed Pugl X11 `SelectionNotify.property == None` correction pinned into NativeUI.
- T060 / PR #118: one explicit `ui::Application` owns exactly one standalone `PUGL_PROGRAM` world; explicit `StandaloneWindow(Application&, ...)` instances borrow that world while retaining independent per-window UI/render/input state. `EmbeddedView` remains independent `PUGL_MODULE` ownership. The legacy standalone constructor remains pre-v1 only for later T069 removal.
- T059 / PR #89: inherited availability/read-only state.
- T030 / PR #94: Button.
- T031 / PR #95: Checkbox and typed RadioGroup/RadioButton.
- T032 / PR #115: Slider and RangeSlider.

## State/widget lane — T032 complete, T033 active

T032 provides one shared numeric domain for finite range/step validation, clamp/quantization, safe external-state presentation and keyboard increments. Wide finite binary32 ranges use double intermediates so accepted endpoints cannot overflow the internal span. Slider and RangeSlider share `SliderTrackAxis`, so rendered thumb travel and pointer mapping use the same horizontal/vertical geometry. RangeSlider chooses the nearest thumb from the raw mapped pointer position before quantization, preserves the active thumb through drag and enforces no crossing. T059 remains the sole Disabled/Hidden/Collapsed capture-cancellation authority; ReadOnly mutating input is consumed without state mutation. Interaction bookkeeping and invalidation complete before synchronous `State::set()` boundaries, and the implementation adds no mutable process-global/singleton/`thread_local` state.

Final T032 candidate `9b6022e8f4f1ea0c579c55813bdd209d6343a9b1` passed CI `34433500137`, T042 Lifecycle Stress `34433500199` and T060 Application Contract `34433500185`. Mandatory final `CODE_REVIEW.md` review `5162634478` found no Blocking/Important issue. PR #115 merged as `7228ea9e78ab244027337a0e34e73ccbbf33eac1`; issue #32 is Done.

T033 / issue #33 / PR #123 is the existing next widget stream. Its pre-refresh head `7797e2e3cbeff15c23d5250fb27be21735e1d29c` is implementation-complete and passed normal CI, T042 Lifecycle Stress and T060 Application Contract. Because T032 merged after that head, T033 must be synchronized conflict-aware with current `main`, preserving both widget sets, then rerun exact-head validation and final review before merge. Do not create a second T033 implementation stream.

## Lifecycle qualification update — #139 / PR #140

T042's `standalone_supported_multi_instance` gate is being upgraded from the pre-T060 Decision-B marker to an executable stress of the now-supported T060 ownership path. The fixture keeps one explicit `ui::Application` / one `PUGL_PROGRAM` world alive across 50 deterministic A+B cycles, validates distinct native windows, independent resize/close state, destroys A while B survives and continues polling/resizing, and uses `QuitPolicy::ExplicitOnly` to exercise zero-window intervals without inventing a hidden singleton, mutable process-global owner or `thread_local` owner.

The legacy process-isolated standalone constructor stress remains as compatibility coverage until T069; independent simultaneous legacy `PUGL_PROGRAM` worlds remain forbidden by #64 Decision B. Embedded T042 stress remains unchanged.

Initial GREEN head `1b917ec157a938b997e0afc6979454495b90effb` passed T060 Application Contract `34431532321`, T042 Lifecycle Stress `34431532386` on Linux/X11, Windows, macOS and Linux ASan+UBSan, and normal CI `34431532332`. Mandatory review then found stale pre-T060 diagnostic wording and missing direct GDB/LLDB diagnosis for the new Application stress path. The correction stream updates those diagnostics before final exact-head qualification. #139 remains a blocking validation dependency for T052 / PR #120; after #139 merges, T052 must refresh from that new `main` and rerun its release qualification set.

## Current dependency frontier

```text
state/widgets:     T059(done) -> T030(done) -> T031(done)
                                       |-> T032(done)
                                       |-> T033(active PR #123)
                                       +-> T034 -> T035 / T036

lifecycle/release: #64(done) -> T060(done) -> #139(active PR #140) -> T052
                   T042(done) -> T051(done) -------------------------> T052

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

platform/event:    T060(done) -> T065(active PR #133) -> T072 -> T064
```

T033/T034 and subsequent standard widgets belong to the state/widget lane. #139/T052 and T065/T072/T064 remain separate streams and must not be duplicated by widget work.

## Active platform/event work — T065 / issue #77 / PR #133

The existing T065 stream already delivers the bounded Core dispatcher/timer engine and root integration. Its reviewed contract includes per-owner bounded queues/timers, deterministic fake time, FIFO ordering, finite drain fairness, owner shutdown semantics and no process-global dispatcher. It must remain a separate implementation stream from widget work.

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

1. Resume existing T033 / PR #123 and synchronize it conflict-aware with current `main` without creating a duplicate implementation stream.
2. Preserve T032 while resolving the shared `CMakeLists.txt`, `widgets.hpp` and `widget_tests.cpp` integration points, then confirm the resulting PR diff remains T033-only relative to current main.
3. Rerun exact-head normal CI, T042 Lifecycle Stress and T060 Application Contract for T033; repeat the mandatory final `CODE_REVIEW.md` pass and merge only when all gates are green.
4. After T033, re-evaluate T034 and other dependency-ready widget work by explicit dependency/priority/unblock value.
5. Keep #139/T052, T065/T072/T064 and unrelated package/lifecycle work untouched by this lane.
