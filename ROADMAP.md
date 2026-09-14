# NativeUI roadmap

**Updated:** 2026-09-14

NativeUI is a reusable C++20 desktop retained-mode UI toolkit: Pugl owns native views/events, Skia owns rendering, and NativeUI owns retained composition, layout, input/focus, widgets, styling, resources and packaging. GitHub Issues are the source of truth for exact ticket scope, status and dependencies.

## Execution rules

- explicit GitHub `Dependencies:` are hard gates;
- resume existing canonical branches/PRs before creating work;
- among Ready work, prefer priority and downstream unblock value;
- behavior/configuration changes use RED -> GREEN -> REFACTOR;
- implementation progress and issue conformity are separate; Done/merge requires an explicit issue-to-code/test evidence matrix with no unchecked requirement;
- code-changing tickets require exact-head normal validation and a final `CODE_REVIEW.md` record with no Blocking/Important finding;
- use `CI_POLICY.md`: Draft for active implementation, then Draft -> Ready only for a frozen executable candidate to trigger T042/T052 qualification;
- NativeUI-owned targets must compile with zero unapproved warnings and the default empty `NATIVEUI_ALLOWED_WARNINGS`;
- unrelated lanes may continue only within repository concurrency rules while another PR waits exclusively on external CI;
- every completion cycle synchronizes issue status, `CONTEXT.md` and this roadmap;
- feature tickets ship an interactive example plus deterministic `--self-test`.

## Current execution snapshot

Current `main` includes T128 / PR #296 merged as `ca65087b503aff04133394c5ef200cd7cae75e12`, T126 / PR #295 merged as `be216d7a41999113472a0457827d43caad28cf15`, T123 / PR #284 merged as `839a7b082f94e0bef3b688cc7bcc2074e6cbfb99`, T039 / PR #260 squash-merged as `1b998491306ae3fff9771339bedca7e14007f355`, T040 / PR #259 squash-merged as `df82860fd141a37140c67dc96e1326dbf9d87403`, T049 / PR #258 squash-merged as `24d5b2265360917a37e1ab7d5846a0348b305485`, T038 / PR #218 squash-merged as `8d81a0803a9c7f9b191d1fd4232d975adb39bf36`, T064 / PR #240 merged as `5e9798f6637af8d6275379002fa8116f167115f7`, plus completed T044, T072, T066, T062 with its post-merge completeness fix, T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060.

T128 / issue #289 / PR #296 is complete and merged. Frozen source head `ce58df7758e960c329f26394bad44ce4c7b4efc1` passed normal CI `34848275511`, T051 Release Benchmarks `34848275318`, T064 Desktop Services Contract `34848275472`, T065 Dispatcher Contract `34848275470`, T065 Platform Dispatcher `34848275371`, T066 Window Controls `34848275349`, then final-candidate T042 Lifecycle Stress `34853025047` and T052 v0.1 Release Gate `34853023976`. Independent final `CODE_REVIEW.md` review `5198631295` reported zero Blocking/Important findings. Accepted Dispatcher work now remains durable after a neighboring callback throws without retrying the begun callback or allocating a recovery queue; failing animation entries become terminal before propagation while sibling scheduling remains live; completion is at-most-once; and a real platform expose/paint exception is contained at the foreign event-loop boundary on Linux, Windows and macOS. T128 is satisfied as one pre-freeze T069 prerequisite and directly unlocks no child ticket.

T126 / issue #287 / PR #295 is complete and merged. Frozen executable head `4ae5dae65f86cc34bd3177f91ad3e6db7403b02d` passed normal CI `34844330034`, T064 Desktop Services Contract `34844329931`, Package Contracts `34844329951`, T066 Window Controls `34844330127`, then final-candidate T042 Lifecycle Stress `34846694493` and T052 v0.1 Release Gate `34846694353`. Independent final `CODE_REVIEW.md` review `5197980748` reported zero Blocking/Important findings. The delivered contract makes DesktopServices completion terminal before fallible Dispatcher marshalling, contains exceptions at AppKit/native/worker boundaries, makes retained panel cleanup deterministic and ensures queue rejection/allocation failure cannot strand request capacity or fall back to application code on a backend thread. T126 is now satisfied as one pre-freeze T069 prerequisite and directly unlocks no additional child ticket.

T123 / issue #281 / PR #284 is complete and merged. Frozen executable head `f2c5cada877df82d8936f9dd545d796c0ab5156e` passed normal CI `34837857267`, T066 Window Controls `34837857292`, then final-candidate T042 Lifecycle Stress `34841171641` and T052 v0.1 Release Gate `34841171493`. Independent final `CODE_REVIEW.md` review `5197372173` reported zero Blocking/Important findings. The delivered contract freezes deterministic lifetime-safe `State<T>` notification passes, recursive latest-write coalescing, observer add/remove behavior, subscription/source lifetime, explicit equality requirements, steady-state non-cloning listener storage and deterministic throwing-observer recovery/pending-write policy. T123 now unblocks T127 and the first decomposed T124 child T138.

T039 / issue #39 / PR #260 is complete and merged. Frozen executable head `3cf0c8e33cb472648cef880ece780dbda6b3fd38` passed normal CI `34778524773`, T066 Window Controls `34778524854`, then final-candidate T042 Lifecycle Stress `34779287474` and T052 v0.1 Release Gate `34779287449`. Independent final `CODE_REVIEW.md` review reported zero Blocking/Important findings. The delivered scoped-style contract provides typed lexical `StyleScope` inheritance, retained-ancestry resolution, exact outer/inner/component precedence, bounded paint-vs-layout invalidation, structural removal/restoration/reinsertion, T058 dynamic ancestry, sibling/two-UI isolation and deterministic headless acceptance evidence.

T040 / issue #40 / PR #259 is complete and merged. Frozen executable head `9bf6b9a49bf68ef4fe2bb38e1a602a0181aea39f` passed normal CI plus T051 Release Benchmarks, T064 Desktop Services, both T065 Dispatcher workflows and T066 Window Controls, then final-candidate T042 Lifecycle Stress `34770340801` and T052 v0.1 Release Gate `34770340828`. Independent final `CODE_REVIEW.md` review reported zero Blocking/Important findings. The delivered animation layer provides exact cubic easing, deterministic semi-implicit spring integration, one coalesced T065 wake per active context, explicit retained Paint-vs-Layout invalidation targets, per-context reduced motion, finite/invalid configuration rejection, teardown-safe cancellation and zero idle timer/redraw behavior.

T049 / issue #49 / PR #258 is complete and merged. Frozen executable head `58bad47cdc7095f261a812cd34fb38e23e2c5af2` passed normal CI `34765491619`, T066 Window Controls `34765491618`, then final-candidate T042 Lifecycle Stress `34767252432` and T052 v0.1 Release Gate `34767252389`. Independent final `CODE_REVIEW.md` review reported zero Blocking/Important findings. The delivered gallery is one standalone public-API-only visual catalogue covering required layout, text, standard controls, values, collections/navigation, Canvas/Image/SVG resources and representative interaction/style states, with deterministic self-test, static public/private boundary guards and instance-owned demo state.

T038 / issue #38 / PR #218 is complete and merged. Frozen executable head `401ff95808b96636b3edb644ef8dd5902f0db749` passed normal CI, T044 Native Pointer Capture, T066 Window Controls and T067 Virtual List Contract, then final-candidate T042 Lifecycle Stress `34758523505` and T052 v0.1 Release Gate `34758523484`. Independent final `CODE_REVIEW.md` review `5190751522` reported zero Blocking/Important findings. T038/T039/T040 styling work is now complete and no longer blocks the v1 release frontier.

T064 / issue #76 / PR #240 is complete and merged. Frozen current-main-synchronized executable head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed normal CI, T064 Desktop Services, T072 Linux D-Bus, T066 Window Controls, T065 Platform Dispatcher, T060 Application, T044 Native Pointer Capture, Package Contracts, T042 Lifecycle Stress and T052 v0.1 Release Gate. The final requirement-to-implementation/test review and current-main composition re-certification report no Blocking/Important finding.

T072 / issue #84 / PR #185 is complete and merged. Its final executable candidate passed normal CI, T072 Linux D-Bus Contract, T060 Application Contract, T065 Platform Dispatcher, T067 Virtual List Contract, T042 Lifecycle Stress and T052 v0.1 Release Gate with a clean final `CODE_REVIEW.md` review.

T062 / issue #74 / PR #226 is complete and merged as `7269310995adad1e7b474b6cea319644fc8020f4`; PR #245 / `86c12e8472a82cd929a95c24705995dfa871bbf5` closes its post-merge completeness gaps.

T066 / issue #78 / PR #237 is complete and merged as `c0140e725ac33b0a3ca315124a3d20091b96a173`. Its frozen executable candidate passed normal/path-scoped validation plus final-candidate T042/T052 qualification, and the final requirement-to-implementation/test review recorded no Blocking/Important finding.

T044 / issue #44 / PR #145 is complete and merged as `d4a61888c2d6bc096774de5a71dba8ad82cdff29`. Frozen implementation/test head `b16861f8db07e5292ebbfd40e5f21c00234b0f2a` passed exact-head normal/path validation plus final-candidate T042/T052 qualification. Reviews `5185659103` and `5186332658` record the complete #44 Outcome A/B and `CODE_REVIEW.md` evidence matrix with no Blocking/Important finding.

T050 / issue #50 / PR #238 delivers the opt-in retained-tree debug inspector. Frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` is synchronized with its qualified baseline, preserves the T044/T064/T066 contracts, and passed normal macOS/Windows/Linux CI, Linux ASan+UBSan, Package Contracts, T066, T072 and the dedicated inspector OFF/ON matrix; the inspector-ON build also passed all 65 registered headless tests.

Current dependency frontier:

```text
style:             T037(done) -> T038(done) -> T039(done)
                                      +-------> T040(done) with T065(done)

gallery/release:   T038(done) -> T049(done) -----------------> T071

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(done) -> T064(done)
                   T041(done) -> T043(done) -> T066(done)

state/safety:       T123(done) -> T138(requalify current main) -> T139 -> T140 -> T141 -> T124 closeout
                    T123(done) -> T127(ready)
                    T126(done)
                    T128(done)
                    T125 + T129 + T130 + T131 + T132(requalify current main) in parallel where safe

release:            T124 + T125 + T127 + T129 + T130 + T131 + T132(done) -> T069 -> T070 -> T071 -> v1.0.0
                    T049(done) -----------------------------------------------------------> T071
                    T044(done) -----------------------------------------------------------> T071

post-1.0:           T068 is explicitly deferred to NativeUI 1.2 and does not block T069/T070/T071.
```

The historical platform/styling prerequisites are complete, but the September 14 pre-freeze safety audit added hard blockers before T069. T124 is an umbrella decomposed as T138 -> T139 -> T140 -> T141 -> T124 closeout. T127 remains a verified immediate unlock from T123. T126 and T128 are Done and remove independent prerequisites, but T069 remains Blocked until the remaining full pre-freeze blocker frontier is Done. T138 and T132 had clean source reviews/final-candidate launches before the T128 executable merge advanced main; they must requalify current-main composition before merge. T068/PR #241 stays parked for 1.2 and is excluded from the v1 release critical path.

## Milestone 0 — Baseline hardening

**Complete.** T001–T006 provide core/state tests, retained lifecycle, public-header split and invalidation foundations. #163 / PR #181 makes unapproved NativeUI-owned compiler warnings Blocking.

## Milestone 1 — Layout system

**Complete.** T007–T012 provide constraints, alignment/distribution, flex, grid, scroll state/layout and clipping/overflow foundations.

## Milestone 2 — Input, focus and gestures

**Complete baseline.** T013–T018 provide event propagation, focus scopes/restoration, pointer capture, wheel normalization, gestures, commands and drag/drop primitives.

## Milestone 3 — Rendering and graphics

**Complete.** T019–T024 provide transforms, paths, gradients, images, SVG/resources, caches and deterministic rendering/golden tests.

## Milestone 4 — Text system

**Complete.** T025–T029 provide text editing, Label/fonts/fallback, TextArea, UTF-8 selection/navigation and platform IME composition bridges.

## Milestone 5 — Standard widget set

**Complete for the v1 standard-widget scope.**

- T030 Button — PR #94.
- T031 Checkbox/Radio — PR #95.
- T032 Slider/RangeSlider — PR #115.
- T033 ProgressBar/Meter — PR #123.
- T034 ScrollView — PR #135.
- T035 ComboBox/PopupMenu — PR #225; policy over T061 with immutable open snapshots, exact keyboard/pointer/focus semantics, deterministic goldens and standalone/embedded smoke.
- T036 ListView/Tabs — PR #155; #212 / PR #213 adds deterministic hover presentation.

## Milestone 6 — Styling, theme and animation

**Complete. T037, T038, T039 and T040 are Done.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 / PR #218 provides the typed shared `VisualState`, per-widget style families, deterministic interaction precedence, state-aware paint-vs-layout invalidation, geometry stability/isolation evidence, representative goldens and dedicated self-tests. T039 / PR #260 provides typed lexical retained `StyleScope` inheritance, ancestry-authoritative dynamic insertion/removal, structural restoration evidence and scoped invalidation. T040 / PR #259 provides the deterministic tween/spring animation layer over T065, explicit retained Paint/Layout invalidation routes and per-context reduced-motion policy without introducing another scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers, T045 accessibility architecture and T043 resize/scale negotiation.

### T043 — Resize/scale negotiation

**Complete in PR #142.** Public/component geometry remains logical while native/framebuffer geometry is physical; per-view finite-positive scale, authoritative configure snapshots, exact request/echo behavior, fractional conversion, embedded parent authority and reentrancy-safe preferred-size notification are qualified.

### T044 — OS pointer capture qualification

**Complete in PR #145 / issue #44.** The evidence-gated decision procedure is finalized per platform:

- **macOS: Outcome A.** Pinned Pugl capture is sufficient; no T044 macOS capture backend was added. Native CGEvent outside-view motion/up, focus-loss, destruction and isolation evidence is green.
- **Windows: Outcome B.** Pinned Pugl owns acquisition; T044 records the concrete HWND at retained capture start and releases only that same HWND on retained cancellation. Outside-view evidence uses real `SetCursorPos` + `SendInput`, so delivery depends on native capture rather than direct window messaging.
- **Linux/X11: Outcome B.** X11 ButtonPress supplies the active grab; retained cancellation explicitly releases it with `XUngrabPointer`/`XSync`. A narrow per-view focus visibility seam exposes the FocusIn/FocusOut transitions consumed internally by the pinned Pugl XIC path so mandatory focus-loss cancellation remains deterministic.

All capture bookkeeping remains per view, widgets stay platform-neutral, exactly-once up/cancel and teardown/isolation paths are covered, Linux sanitizer coverage is green, and no Objective-C runtime-visible class/category/swizzle/+load was introduced. Final normal/path and T042/T052 gates are green with no Blocking/Important review finding.

### T066 — Standalone window controls

**Complete in PR #237 / issue #78.** V1 scope is limited to logical min/max constraints, runtime title/show/hide/size control and deterministic close lifecycle on T060 Application-owned windows:

- finite/order validation, initial clamp and atomic runtime constraint updates preserve T043 native-authoritative configure semantics;
- UTF-8 title and idempotent show/hide do not recreate or remount UI;
- user/native close supports veto, accepted close is deferred through T065, and programmatic `request_close()` bypasses veto;
- reentrant `request_close()` inside veto wins over Cancel and duplicate close requests cannot duplicate `on_closed`;
- direct C++ destruction is callback-silent, suppresses pending accepted-close completion and still unregisters exactly once through T060 quit policy;
- close callbacks run outside internal teardown/Application locks and per-window state remains isolated;
- native macOS/Windows/Linux control/close coverage, pure state tests, two-window destroy-A/survivor-B coverage and deterministic feature `--self-test` are qualified;
- final normal/path and T042/T052 gates are green with no Blocking/Important review finding.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T049 component gallery, T050 debug inspector, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView, T062 Tooltip, T063 Dialog, T064 DesktopServices, T072 Linux D-Bus transport and T045 semantic architecture. T123 is complete as the hardened state-observer baseline, T126 is complete as the hardened DesktopServices exceptional-completion boundary, and T128 is complete as the hardened Dispatcher/Animation exceptional-recovery boundary; the remaining pre-freeze safety blockers must converge before T069.

### T049 — Public component gallery

**Complete in PR #258 / issue #49, merged as `24d5b2265360917a37e1ab7d5846a0348b305485`.** One standalone public-API-only gallery keeps the focused feature examples canonical while providing a discoverable manual-QA surface for layout/containers, text, standard controls, values, collections/navigation, Canvas/Image/SVG and representative T038 interaction/style states. Its deterministic `--self-test` constructs/renders all retained gallery sections, exercises representative input/state changes and a second independent gallery instance. Static discovery coverage rejects private `nativeui/detail`, Pugl, Skia and platform includes. Frozen executable head `58bad47cdc7095f261a812cd34fb38e23e2c5af2` passed normal CI, T066 and final T042/T052 qualification with zero Blocking/Important final-review findings.

### T050 — Debug inspector / overlay

PR #238 / issue #50 adds a passive, opt-in diagnostic layer rather than a second retained-tree system:

- build option `NATIVEUI_ENABLE_INSPECTOR` defaults OFF, with runtime activation and per-UI inspector state compiled only when enabled;
- each `ui::UI` owns enabled/selected state independently; no current-inspector singleton, mutable process registry or `thread_local` instance state is introduced;
- immutable value snapshots expose NodeId, parent/depth/order, stable debug label, logical bounds, effective clip, layout/paint dirty state, focus/capture and effective T059 availability without leaking raw runtime pointers;
- stale/destroyed NodeId queries are safe and previously copied snapshots remain self-contained;
- enabling/disabling or changing the selected NodeId requests one paint invalidation only and never creates layout dirtiness or a timer/tick loop;
- normal root and T061 application overlay content paint first; the diagnostic pass then draws node/clip/dirty/selection/focus/capture information and restores Painter/SkCanvas state;
- the inspector receives no hit testing, pointer/keyboard input or focus and does not consume T061 overlay slots;
- deterministic tests cover hierarchy/child order, dirty state/regions, stale IDs, focus/capture/clip/availability, selected-node emphasis, keyboard/pointer non-interception, canvas-state preservation, no-continuous-redraw behavior and two-UI isolation;
- infrastructure scope intentionally requires deterministic inspector/headless fixtures rather than a new normal feature API example;
- frozen executable head `fc0cd452bc442e83ae89fac04d227922cfc4a450` passed inspector OFF/ON, the full 65-test inspector-ON headless suite, normal CI on macOS/Windows/Linux, Linux ASan+UBSan, Package Contracts, T066 and T072 with no Blocking/Important review finding.

### Build/example registration hardening

**Complete in PR #235 / issue #234.** Canonical `examples/features/tNNN_<feature>.cpp` sources are auto-discovered by root CMake with deterministic ordering, `CONFIGURE_DEPENDS` and malformed-name rejection.

### T061 — Generic overlay / portal layer

**Complete in PR #216.** One generic retained in-view overlay/portal layer per UI using T058 structural reconciliation, including deterministic placement, modal focus/capture behavior, anchor tracking, no-click-through dismissal, reentrant show/close safety and standalone/EmbeddedView parity.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219 with startup regression closure in PR #233.** The required 100k example is constrained to the real viewport, startup materialization remains bounded, and the production `--self-test` is executed by dedicated CI.

### T063 — Dialog

**Complete in PR #227 / issue #75.** Dialog remains policy over T061 rather than a second modal/window manager. It provides one active Dialog slot per UI, validated action/result IDs, Default/Cancel semantics, styleable backdrop, bounded centered sizing, fixed title/actions, T034 scrolling for overflow, no-click-through pointer behavior, trapped focus/restoration, child-first Enter behavior, Escape Cancel/Dismissed policy, exact-once completion, safe controller/UI teardown, per-UI isolation and close-before-callback reentrancy.

### T062 — Tooltip

**Complete in PR #226 / issue #74, with PR #245 closing post-merge completeness gaps.** A text-only retained decorator over T061/T065: exact 500 ms default delay with zero-delay checkpoint deferral, one per-instance timer/eligibility controller, no warm-up or cross-anchor reuse, shared hover/focus delay with full restart on new transitions, a `NonModal`/`Auto`/`Ignore` overlay that is non-focusable and non-hit-testable, deterministic dismissal and suppression, semantic help through T045 semantics, and native standalone/EmbeddedView qualification.

### T072 — Bounded Linux D-Bus transport

**Complete in PR #185 / issue #84.** It provides the sole Linux D-Bus layer for T064 and future T068 1.2 integration:

- system `libdbus-1` only, no public D-Bus API/type leakage;
- one private session connection + one joinable I/O thread per transport;
- only the explicit process-wide `dbus_threads_init_default()` once-initialization;
- exact 1,024/256/256 request/subscription/object-path limits with registered client ownership;
- exactly-once request terminal states and T065-only UI callback marshalling;
- teardown-safe queued callback suppression;
- bounded signal/object-path services and a closed owned C++ value codec;
- immutable provider-read boundary for future T068 read-only queries, with mutation/actions marshalled to UI;
- one lazy shared transport per T060 Application and the independently owned EmbeddedView boundary required by T068;
- package/install integration and Linux prerequisite documentation.

### T064 — Cross-platform DesktopServices

**Complete in PR #240 / issue #76, merged as `5e9798f6637af8d6275379002fa8116f167115f7`.** The delivered bounded service contract is:

- callback-only open-file, open-files, save-file, select-directory and absolute HTTP(S) URL operations;
- callbacks always cross the owning T065 Dispatcher, including immediate Busy/ResourceLimit/Unsupported/InvalidArgument failures and accepted inline backend completion;
- exact one active chooser and sixteen active URL requests per DesktopServices owner, monotonic non-zero owner-local IDs, stale/cross-owner cancellation isolation, exactly-once completion and capacity release before application callback;
- exact filter/suggested-filename/cardinality/path/URL validation with no hidden request queue;
- `StandaloneWindow::desktop_services()` is lazy and view-owned; built-in EmbeddedView remains side-effect-free/Unsupported unless an embedding owner injects a backend;
- macOS uses `NSOpenPanel` / `NSSavePanel` + `NSWorkspace`, with no new NativeUI Objective-C runtime-visible class/category/swizzle/+load;
- Windows uses `IFileOpenDialog` / `IFileSaveDialog` on request-owned joinable STA workers, `ShellExecuteW` for URL launch and request-local same-STA cancellation without GIT/process registry/raw cross-apartment dialog state;
- Linux/X11 uses XDG Desktop Portal FileChooser/OpenURI through T072 only, preserves T072 hard-quota `ResourceLimit`, uses parent-window routing and Request.Close, and adds no GTK/Qt/zenity/shell fallback or overflow transport;
- native macOS/Windows/Linux qualification, package integration and `examples/features/t064_desktop_services.cpp --self-test` are complete.

Frozen current-main-synchronized head `ac1ebe13fe61e39bd5ba9bdb4cd5e75bfb795ca7` passed exact-head normal/path-scoped validation and final-candidate T042/T052. The final requirement-to-implementation/test matrix and `CODE_REVIEW.md` audit report zero remaining Blocking/Important findings.

### T126 — DesktopServices exceptional completion hardening

**Complete in PR #295 / issue #287, merged as `be216d7a41999113472a0457827d43caad28cf15`.** The T064 service now has explicit exceptional completion semantics: native/backend completion is terminal before fallible UI marshalling; `Dispatcher::post()` rejection and exception-before-enqueue are terminal delivery drops with deterministic capacity release and no backend-thread callback fallback; AppKit completion blocks contain C++ exceptions and retained panels are released exactly once through exceptional paths; Windows worker completion boundaries contain coordinator exceptions; Linux/T072 continues through the common contained path. Exact-head normal/path checks and T042/T052 final qualification are green, and independent review `5197980748` records zero Blocking/Important findings.

### T128 — Dispatcher/Animation exceptional recovery hardening

**Complete in PR #296 / issue #289, merged as `ca65087b503aff04133394c5ef200cd7cae75e12`.** Dispatcher callbacks that have not begun remain durable across a neighboring throwing callback and preserve FIFO ordering ahead of reentrant/newer posts without a second recovery queue; shutdown still discards pending work deterministically. Animation write/invalidation/completion failures terminalize the begun entry before propagation and preserve or deterministically terminate sibling scheduling so no active entry is stranded without progress. Reduced-motion/immediate exceptional paths are deterministic and completion remains at-most-once. Exact-head CI/T051/T064/T065/T066 plus T042/T052 final qualification are green, the cross-platform native exception-boundary smoke executes on Linux/Windows/macOS, and independent review `5198631295` records zero Blocking/Important findings.

### T068 — Native accessibility bridges (post-1.0)

T068 is explicitly deferred to NativeUI 1.2 and no longer blocks T069, T070, T071 or the NativeUI 1.0 release. Preserve the existing canonical Draft PR #241; do not consume a v1 delivery lane unless the ticket is explicitly reprioritized.

### Final v1 release path

```text
T123(done) -> T138(requalify current main) -> T139 -> T140 -> T141 -> T124 closeout --+
T123(done) -> T127 ---------------------------------------------------------------|
T126(done) -----------------------------------------------------------------------|
T128(done) -----------------------------------------------------------------------|
T125 + T129 + T130 + T131 + T132(requalify current main) -------------------------+-> T069 -> T070 -> T071 -> v1.0.0
T049(done) -----------------------------------------------------------------------------------> T071
T044(done) -----------------------------------------------------------------------------------> T071
```

T123, T126 and T128 are complete. T127 remains a verified Ready unlock from T123. T124 is an umbrella/integration parent and must not be treated as one monolithic implementation ticket. T128 directly unlocks no additional child; it removes one independent pre-freeze prerequisite. Because T128 changed executable Dispatcher/Animation behavior after the prior T138 and T132 candidate compositions were launched, those candidates require current-main requalification before merge. T069 is the final v1 public API freeze but remains blocked until the remaining safety frontier above is Done. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket.

## Immediate cross-lane plan

1. Requalify T138/#303 on current main, then merge/mark Done and unlock T139 only after exact current-composition normal/path/T042/T052 evidence is green.
2. Requalify T132/#294 on current main before merge; its previous candidate composition predates the T128 executable merge and therefore is not a valid final merge certificate.
3. Assign T127/#288 when a compatible source lane is free and continue T125/T129/T130/T131 closeout, giving merge/qualification work priority over metadata cleanup.
4. Advance the decomposed T124 chain strictly as `T138 -> T139 -> T140 -> T141 -> T124 closeout`; do not claim the umbrella parent for monolithic feature work.
5. Keep T069/#81 Blocked until the complete remaining pre-freeze safety frontier is Done and synchronized on main; then perform one whole-surface API-freeze audit before Ready/final gates.
6. After T069 completes, re-evaluate T070 directly; after T070, re-evaluate T071 on the exact frozen/RC baseline.
7. Keep T068/PR #241 parked for 1.2.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> pre-freeze safety/lifetime convergence
  -> public API freeze
  -> reference package/release
```

Independent tickets may progress concurrently only when explicit dependencies, shared-file conflict risk and repository concurrency rules allow it.