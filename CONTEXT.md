# NativeUI compact recovery context

**Updated:** 2026-09-13

## Mission and invariants

NativeUI is a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views. Pugl owns native windowing/embedding/event delivery; Skia owns rendering; NativeUI owns retained UI behavior, layout, input/focus, state, widgets, text, resources, packaging and tests. Plug-in APIs, DSP/audio and host parameter semantics remain out of scope.

Non-negotiable rules:

- widgets/layout stay platform-neutral and public geometry uses logical pixels;
- embedded polling is non-blocking;
- mutable instance-dependent production globals/singletons/`thread_local` state are forbidden;
- retained UI state is UI/main-thread confined unless explicitly documented otherwise;
- dependencies use CMake + CPM; Skia comes from pinned `skia-builder` binaries;
- macOS Objective-C runtime-visible platform classes are consumer-specific through the T053 identity contract;
- NativeUI-owned source-tree targets compile with zero unapproved warnings and an empty default `NATIVEUI_ALLOWED_WARNINGS`;
- behavior changes use RED -> GREEN -> REFACTOR and must not weaken tests or validation policy;
- `CODE_REVIEW.md`, `CI_POLICY.md`, a complete issue-to-code/test evidence matrix, `CONTEXT.md` and `ROADMAP.md` are merge gates for code-changing tickets.

## Pinned dependencies

- Pugl: `hemduf/pugl` commit `195f79b22644010c81a5e0c3231c591856787ec6`.
- Skia: `olilarkin/skia-builder` `chrome/m149`.
- macOS: universal GPU Release asset.
- Windows: x64 MSVC, `/MD` default and `/MT` selectable.
- Linux: x64 GPU Release, X11/OpenGL/Fontconfig.

## Current baseline and critical path

T038 / issue #38 / PR #218 is complete and squash-merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`. Frozen executable head `401ff95808b96636b3edb644ef8dd5902f0db749` passed normal CI plus T044/T066/T067 path-scoped contracts, then final-candidate T042 Lifecycle Stress `34758523505` and T052 v0.1 Release Gate `34758523484`. Independent final review `5190751522` recorded zero Blocking/Important findings. The completion delivers typed widget style/state families, deterministic shared interaction precedence, orthogonal focused/selected/checked/read-only state, equal-resolved no-invalidation, paint-vs-layout classification, geometry stability, instance isolation, representative goldens and deterministic feature self-tests.

T039 / issue #39 / PR #260 is complete and squash-merged as `1b998491306ae3fff9771339bedca7e14007f355`. Frozen executable head `3cf0c8e33cb472648cef880ece780dbda6b3fd38` passed normal CI `34778524773`, T066 Window Controls `34778524854`, final-candidate T042 Lifecycle Stress `34779287474` and T052 v0.1 Release Gate `34779287449`. Independent final review recorded zero Blocking/Important findings. The completion delivers typed lexical `StyleScope` inheritance, retained-ancestry resolution, exact outer/inner/component precedence, scoped paint-vs-layout invalidation, structural scope removal/restoration/reinsertion, T058 dynamic-descendant ancestry, sibling/two-UI isolation and deterministic headless acceptance evidence.

T049 / issue #49 / PR #258 is complete and squash-merged as `24d5b2265360917a37e1ab7d5846a0348b305485`. Frozen executable head `58bad47cdc7095f261a812cd34fb38e23e2c5af2` passed normal CI `34765491619`, T066 `34765491618`, final-candidate T042 Lifecycle Stress `34767252432` and T052 v0.1 Release Gate `34767252389`. Independent final review recorded zero Blocking/Important findings. The delivered gallery is a single-window public-API-only visual catalogue with deterministic self-test coverage, public/private include guards, focused-example indexing, representative interaction/style-state surfaces and instance-owned demo state.

T040 / issue #40 / PR #259 is complete and squash-merged as `df82860fd141a37140c67dc96e1326dbf9d87403`. Frozen executable head `9bf6b9a49bf68ef4fe2bb38e1a602a0181aea39f` passed normal CI plus T051/T064/T065/T066 path-scoped contracts, then final-candidate T042 Lifecycle Stress `34770340801` and T052 v0.1 Release Gate `34770340828`. Independent final review recorded zero Blocking/Important findings. T040 delivers exact cubic easing, deterministic semi-implicit spring integration, one coalesced T065 wake per active context, explicit retained Paint-vs-Layout invalidation routes, per-context reduced motion, finite/invalid configuration rejection, teardown-safe cancellation and zero idle timer/redraw behavior.

The immediate v1 frontier is now:

```text
T037(done) -> T038(done) -> T039(done) -----> T069 -> T070 -> T071 -> v1.0.0
                         \-> T040(done) ------/
T049(done) -----------------------------------------------> T071
T044(done) ------------------------------------------------> T071
```

T039 is no longer a source gate. All explicit T069 dependencies have been revalidated as completed after repairing completed-ticket body drift on T047 and T059. T069 / issue #81 is therefore the next P0 critical-path work item; its live product body/status transition from Blocked to Ready must be coherent before Delivery claims it.

T068 / issue #80 / PR #241 is explicitly deferred to NativeUI 1.2. It is P2/Blocked and does **not** block T069, T070, T071 or the NativeUI 1.0 release. Preserve its canonical Draft PR and do not consume a v1 source lane unless explicitly reprioritized.

The v1 platform prerequisite lane is complete: T065, T072, T043, T064 and T066 are Done. T044 is also complete as an independent T071 release prerequisite. Completed retained/dynamic/widget foundations include T034, T035, T036, T037, T038, T039, T040, T045, T049, T058, T059, T060, T061, T062, T063 and T067.

## Completed foundations relevant to v1

- T030–T036: standard widget baseline through ListView/Tabs.
- T037 / PR #151: typed per-UI Theme values and representative theme binding.
- T038 / PR #218: typed widget visual-state/style resolution and invalidation contract, merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`.
- T039 / PR #260: typed retained lexical StyleScope inheritance with structural removal/restoration and dynamic ancestry evidence, merged as `1b998491306ae3fff9771339bedca7e14007f355`.
- T040 / PR #259: deterministic per-context tween/spring animation layer over T065, merged as `df82860fd141a37140c67dc96e1326dbf9d87403`.
- T043 / PR #142: logical/native resize and scale negotiation.
- T044 / PR #145: evidence-gated native pointer-capture qualification.
- T045 / PR #210: backend-neutral accessibility semantic architecture and virtual collection contract.
- T047/T048: relocatable package and external-consumer qualification.
- T049 / PR #258: single-window public component gallery, merged as `24d5b2265360917a37e1ab7d5846a0348b305485`.
- T050 / PR #238: opt-in per-UI retained-tree debug inspector.
- T051/T052: performance regression policy and v0.1 qualification gate.
- T053: consumer-scoped macOS Objective-C runtime identity.
- T054/T056/T057: application/package/resource helpers.
- T058 / PR #154: bounded retained dynamic composition.
- T060: explicit Application ownership and multi-window lifecycle.
- T061 / PR #216: generic retained overlay/portal stack.
- T062 / PR #226 + PR #245: Tooltip contract and post-merge completeness closure.
- T063 / PR #227: modal Dialog policy over T061.
- T064 / PR #240: bounded cross-platform DesktopServices using T072 on Linux.
- T065 / PR #133: bounded UI-thread Dispatcher/timers.
- T066 / PR #237: standalone window controls and deterministic close lifecycle.
- T067 / PR #219 + PR #233: fixed-height virtualized ListView with bounded 100k materialization behavior.
- T072 / PR #185: sole v1 Linux `libdbus-1` transport used by T064 and reserved for future T068 1.2 integration.

## Delivered platform/service contracts

T064 provides callback-only file/directory/save/HTTP(S) operations with bounded per-owner request capacity, T065-only callback delivery, teardown-safe exactly-once completion, AppKit/Win32/XDG Portal backends and no shell/GTK/Qt fallback. T072 is the only Linux D-Bus transport and keeps connection/thread/request/subscription/object-path ownership bounded and instance-safe. T066 provides min/max/title/show/hide/resize plus vetoable user close, programmatic close and exactly-once close completion with two-window isolation. T044 keeps widgets platform-neutral while validating outside-view capture behavior and cancellation on macOS/Windows/Linux.

## Styling completion and next work

T037, T038, T039 and T040 are Done. Do not reopen completed styling tickets for later convenience; new behavior belongs to its owning ticket.

- **T039 Done:** explicit lexical/subtree StyleScope inheritance with typed inheritable overrides, ancestry-authoritative dynamic insertion/removal and bounded descendant invalidation.
- **T040 Done:** deterministic cubic tweens + semi-implicit spring solver, T065-only active scheduling, explicit retained invalidation targets and per-context reduced-motion policy.
- **T049 Done:** single-window public-API component gallery with deterministic self-test and public-boundary qualification.

T069 is now the final v1 public API freeze frontier. T070 validates the external reference application and Getting Started against that frozen API. T071 is validation/release-only on one exact RC SHA; defects discovered there return to focused product tickets rather than being fixed inside the release gate.

## Validation policy

Normal source-tree Release validation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

During active development, keep code-changing PRs Draft and run normal CI plus only path-relevant dedicated checks. Once production source/tests/build/workflows are frozen, obtain green normal CI and the mandatory `CODE_REVIEW.md` review, then transition Draft -> Ready to launch heavyweight final-candidate T042/T052 qualification. A later executable/build/workflow change invalidates that candidate. Pure project-state completion documentation does not invalidate an otherwise green frozen executable candidate.

## Automation / integration recovery

GitHub live state is authoritative. `#250` is Scheduler-only control state; current `AUTOMATION CYCLE — GNNN` issue comments are the Worker → Scheduler event bus. Delivery W1/W2/W3 are interchangeable with at most three source-changing lanes. Integration owns independent final review, exact-head CI diagnosis, qualification, merge and completion/unlock bookkeeping; it never implements product features. Reporter is read-only.

T039, T040 and T049 are merged. The next source-changing critical-path lane is T069 after its issue metadata is coherently Ready. Completed-ticket metadata drift on T047/#47, T059/#71, T056/#68, T043/#43 and T052/#52 has been repaired to coherent Done. T068/PR #241 remains parked for 1.2.

## Next actions

1. Make T069/#81 coherently Ready after the verified T039 merge and completed-dependency revalidation, then let the reserved Delivery lane claim it atomically.
2. Service any new T069 SOURCE_READY/CI/review/merge transition before background cleanup.
3. After T069 completes, re-evaluate T070 directly; its known completed metadata prerequisites are now coherent. After T070, re-evaluate T071 against the same rule.
4. Keep T068/PR #241 parked for NativeUI 1.2.
5. Continue through T069 -> T070 -> T071 after dependencies are genuinely and coherently Done; automation continues beyond release gates into the post-1.0 backlog.