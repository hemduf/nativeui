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

The merged baseline is complete through T030 plus T023, #62, T053, #64 Decision B and T059. T031 is the current state/widget completion candidate.

- T053 / PR #88 merged as `ad83ed05f1687fea31255bcc77329fae0f4efb69`: `NativeUI::Core` and portable Pugl C remain generic; the Cocoa Pugl/OpenGL/IME bridge is compiled per final consumer. `cmake/NativeUIConsumerPlatform.cmake` derives `NUI_<fragment>_<digest12>_` from exact UTF-8 `CONSUMER_ID`, rejects duplicate target/identity registration at configure time and has no runtime registry.
- #64 / PR #90 merged as `b52d53eee65e5de91697e2c1f0685728b9646575`: independent overlapping `PUGL_PROGRAM` worlds are not a supported macOS multi-window ownership model. Legacy `StandaloneWindow(UI&, ...)` remains pre-v1/single-window; T060 owns one explicit `ui::Application` PROGRAM owner with multiple top-level windows.
- T059 / PR #89 merged as `6d84bc6b7817b0dcaea4eefd81833d765ad7685d`: the retained tree centrally resolves Visible/Hidden/Collapsed, inherited enabled/disabled and read-only state with focus/capture teardown, reentrancy handling and FocusScope restoration.
- T030 / PR #94 merged as `b32b09da473c70a857675e05f7fdc7c78e5f9361`: Button consumes the central T059 availability/focus/capture contract with platform-neutral pointer/keyboard activation and reentrancy-safe callbacks.

## T031 Checkbox / RadioButton — PR #95

T031 extends the standard widget set on top of T030/T059 without adding platform or global-group state.

Delivered contract:

- two-state `Checkbox` binds one `State<bool>&`, uses T030 pointer/Space activation semantics and intentionally ignores Enter;
- typed `RadioGroup<T>` / `RadioButton<T>` binds one selected `State<T>&`; option identity is the immutable value rather than component address;
- radio groups behave as one Tab stop, entering at the selected available option or first available fallback, with wrapped Left/Right/Up/Down navigation that skips unavailable options;
- T059 Disabled/Hidden/Collapsed behavior remains tree-owned; ReadOnly remains focusable/navigable while pointer/Space mutations are handled without capture/write and radio arrows move focus without changing selected state;
- Button-family press/capture state is shared through one platform-neutral interaction helper rather than duplicated;
- group identity and duplicate-live-value bookkeeping are group-owned only; there is no process-global registry, singleton, `thread_local` state or label/string group key;
- simultaneously-live duplicate option values in one group are rejected deterministically, while weak bookkeeping permits reuse after earlier components are destroyed/remounted;
- dedicated `nativeui_checkbox_radio_tests` and `examples/features/t031_checkbox_radio.cpp --self-test` cover typed values, no-match state, group isolation, reorder/remount, reentrancy, visual states and ReadOnly behavior.

TDD/review evidence:

- mandatory review found one Important exclusivity defect on an earlier head: duplicate live option values could both render selected;
- RED `ce4c4a12208ff20cf9726842003ead6a8291c2e9` made the feature self-test fail specifically on duplicate live values;
- GREEN code head `acf2176cb00accae2a78600ee1b68c136c7d2f26` added the group-owned weak live-option registry;
- CI #382 is fully green for that code head on Linux X11, Windows, macOS and Linux ASan+UBSan, including feature self-test and existing platform/runtime-isolation smokes;
- final `CODE_REVIEW.md` pass on the code head is clean with no remaining Blocking/Important finding.

This documentation commit becomes the final completion candidate and must receive its own exact-head matrix before merge.

## T047 install/export package — PR #92

T047 completes the low-level package/install boundary after T053.

Public install/build-tree contract:

```cmake
find_package(NativeUI CONFIG REQUIRED)

target_link_libraries(MyFinalTarget PRIVATE NativeUI::Core)
nativeui_attach_platform(
    TARGET MyFinalTarget
    CONSUMER_ID com.example.product
)
```

Contract and implementation:

- only `NativeUI::Core` is exported as the reusable generic target; installed consumers must not receive `NativeUI::NativeUI` as a complete v1 native target;
- `nativeui_attach_platform` accepts an existing final `EXECUTABLE`, `MODULE_LIBRARY` or `SHARED_LIBRARY`; static/object/interface/imported/alias/nonexistent targets are rejected;
- one valid reverse-DNS `CONSUMER_ID` is required on every platform and all second attachments are deterministic configure errors;
- the public helper is implemented as a CMake macro so language enablement remains valid on supported CMake 3.24–4.4 while retaining the documented function-like call syntax;
- macOS delegates exact identity/prefix/bridge ownership to T053; Windows/Linux construct their generic Pugl/native layer behind the same public helper;
- the installed package carries pinned Skia assets and the required Pugl/NativeUI platform source machinery privately, so consumers need no separate Pugl installation and no repository-private source path;
- installed CMake paths are prefix-relative; tests copy the install tree to a new prefix and configure/build/run against the relocated package only;
- package tests scan installed CMake files for original source/build path leakage and deliberately remove one pinned Skia archive to prove an explicit NativeUI diagnostic instead of unpinned fallback;
- the installed package includes the project legal/licensing payload required by the package contract.

TDD/review history:

- RED/GREEN cycles cover target/identity validation, duplicate attach, build-tree/install-tree parity, external Core/native linkage, relocation and missing-pinned-dependency failure;
- macOS installed-package acceptance builds two independent MODULE consumers, audits class and metaclass prefixes and loads both in one Objective-C runtime;
- CODE_REVIEW.md pass is clean for instance isolation, configure-time-only bookkeeping, platform delegation, relocatability and public-target boundaries;
- pre-refresh head `69a22f1439991721524bb2bc7aa4c7be94ccbd19` passed CI #338 on Linux X11, Windows/MSVC, macOS including two-consumer runtime isolation, and Linux ASan+UBSan;
- the branch was first refreshed over merged T059 and then over current main `b32b09da473c70a857675e05f7fdc7c78e5f9361` after the independent T030 merge. T030 source/tests/public-header changes and root CMake registrations are preserved while the PR diff remains T047-owned plus mandatory recovery/roadmap bookkeeping.

The merge candidate must pass the exact current-head full matrix after the latest refresh. Do not merge a stale or red head.

## Current DAG frontier

```text
lifecycle:        #64(done) -> T042 -> T051 -> T052
platform/package: T053(done) -> T047(this merge) -> T048 -> T052
                                           |
                                           +-> T054
                                           +-> T056 -> T057
state/widgets:    T059(done) -> T030(done) -> T031(this merge)
                                         Ready independently: T032, T033, T034
```

After T031 merges, T032, T033 and T034 remain the immediate Ready widget candidates; T032 and T034 have the strongest downstream unblock value. T045/T069/T071 consume T031 later but remain blocked by additional explicit dependencies. After T047 merges, T048, T054 and T056 are dependency-unblocked. T052 still additionally requires T048, T051 and T042; T057 requires T056.

## Platform ownership reminders

- Normal standalone validation currently uses one `PUGL_PROGRAM` owner lifetime.
- Multiple independent `EmbeddedView` / `PUGL_MODULE` instances are supported and remain the current host/plugin coexistence path.
- T060 owns the future shared `Application` multi-window standalone model.
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

T031 additionally requires its focused widget suite, feature self-test/goldens, the full relevant core suite and Linux ASan+UBSan. T047 additionally runs public attach-contract CMake fixtures, build-tree and install-tree external consumers, relocated-install consumption, dependency-failure diagnostics, platform linkage and macOS two-consumer Objective-C isolation.

## Next state/widget action

1. Require the exact documentation-complete T031 head to pass Linux X11, Windows, macOS and Linux ASan+UBSan.
2. Refresh from `main` if it advances; preserve parallel-lane `CONTEXT.md`/`ROADMAP.md` changes and revalidate the exact merged candidate.
3. Mark PR #95 ready and merge only with no blocking `CODE_REVIEW.md` finding.
4. Close/update issue #31 as Done.
5. Re-read explicit priorities/dependencies and continue the next Ready state/widget ticket, preferring T032 or T034 by downstream unblock value.

## Next platform/package action

1. Require exact final T047 head CI green on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan.
2. Refresh from `main` again if it advances; resolve `CONTEXT.md`/`ROADMAP.md` and CMake overlaps without dropping upstream work, then revalidate that exact head.
3. Mark PR #92 ready and merge only with no blocking CODE_REVIEW.md finding.
4. Close/update issue #47 as Done.
5. Re-read explicit dependencies/priorities and continue the next Ready ticket in the platform/package lane; T048, T054 and T056 become eligible after T047.

## Lifecycle/stress lane update — #105

T042's deterministic lifecycle matrix exposed a constructor-time platform callback lifetime defect while exercising a focusable embedded view on macOS. Issue #105 / PR #106 fixes the defect without changing the public window/view API: each private `StandaloneWindow::Impl` and `EmbeddedView::Impl` is now its own per-instance `PlatformServices` bridge, so synchronous native callbacks during `ViewCore` construction never re-enter a public wrapper whose `impl_` has not yet been assigned.

- Exact PR #106 code head `bf8e2ff055b4fe5ed2c2bd415bb3818f925f366b` passed CI `34344755454` on Linux X11, Windows/MSVC, macOS including two-consumer Objective-C isolation and clipboard/multi-instance lifecycle smoke, and Linux ASan+UBSan.
- The stacked T042 validation head `97c2c19f09c60c537d538dc887660a0049fa35db` proves the original macOS `embedded_sequential_100` crash is GREEN; Linux X11 and Linux ASan+UBSan are also green.
- Windows reaches the standalone stress path and exposes a separate documented `puglShow(PUGL_SHOW_RAISE)` non-fatal status handling defect, tracked independently as #107. That defect is not folded into #105.
- #64 Decision B is unchanged: there is still no hidden application singleton, shared PROGRAM workaround, or mutable global/`thread_local` ownership state.

After #105 and the independent #107 status fix are merged, refresh T042/PR #93 from current `main`, rerun the exact-head lifecycle matrix on macOS/Windows/Linux X11 plus Linux ASan+UBSan, complete the mandatory review, then merge T042 only if all supported-path acceptance gates are green.

## Lifecycle/stress lane update — #103

T042 also exposed a production Linux/X11 renderer crash before stress-specific lifecycle behavior: the first native exposure under Xvfb/Mesa llvmpipe entered Skia's assembled GL extension discovery and crashed in `GrGLExtensions::init`. Issue #103 / PR #104 keeps the fix in the platform renderer rather than weakening T042.

- Linux now creates Skia's desktop-native GL interface from the Pugl-owned current GLX context. There is deliberately no Linux fallback to the assembled resolver that produced the crash; an unavailable interface returns through the existing controlled renderer-initialization failure path.
- macOS and Windows retain the existing assembled Pugl-proc interface path.
- CI `34343547392` passed the #103 code on Linux X11, Windows/MSVC, macOS and Linux ASan+UBSan. The Linux lane now includes a permanent Xvfb/Mesa llvmpipe real renderer/lifecycle smoke and passes it.
- A stacked T042 candidate reaches and completes Linux native lifecycle stress after this fix, confirming the stress harness is no longer stopped by first-exposure renderer initialization.
- No mutable GL/context global, singleton or `thread_local` state is introduced; #64 Decision B is unaffected.

After #103 merges, T042 still requires the independent #107 Windows show-status correction plus a fresh exact-head supported-path stress matrix before completion.
