# NativeUI roadmap

**Updated:** 2026-09-11

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

Current `main` is `07690842bab94f2d975685d28e0d2b0c1c0fc9d4` and includes T072 / PR #185 plus the completed T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060 foundations.

T063 is complete. Its executable candidate `e907bd3119f59116522b1e672e690703c845f1e6` passed normal CI and the completion candidate passed T042 Lifecycle Stress and T052 v0.1 Release Gate with a clean final `CODE_REVIEW.md` review.

T072 / issue #84 / PR #185 is merged on `main` as `07690842bab94f2d975685d28e0d2b0c1c0fc9d4`. Exact candidate `b5658be1cc4af872642811790bd5821149a885f5` passed normal CI `34631977571`, T072 Linux D-Bus Contract `34631977574`, T060 Application Contract `34631977534`, T065 Platform Dispatcher `34631977512`, T067 Virtual List Contract `34631977511`, T042 Lifecycle Stress `34637714563` and T052 v0.1 Release Gate `34637714484`. Reviews `5181043847` and `5182564487` record the complete #84 evidence matrix and no remaining Blocking/Important finding. T064 is now Ready and should start immediately.

T062 / issue #74 / PR #226 is implementation-complete on `feat/t062-tooltip`, synchronized with `main`, with full Release and ASan/UBSan local suites, deterministic self-test and native standalone/EmbeddedView platform smoke green. It delivers the last dynamic/overlay convergence item before T068; exact-head normal/path-scoped CI and Ready-only T042/T052 qualification are the remaining merge steps.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062(complete, PR #226) -------> T068

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(done) -> T064
                   T041(done) -> T043(done) -> T066
                                      |-------> T068

release:            all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T064 depends on merged/Done T072 and is now Ready. T066 is independently unblocked by completed T043 and must progress in parallel rather than waiting for T072. T044 remains an explicit T071 dependency and is qualified independently.

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

**T037 complete; T038/T039/T040 remain.**

T037 / PR #151 provides typed per-UI Theme values and representative control theme binding. T038 owns typed widget variants, T039 scoped style inheritance, and T040 animation must reuse T065 instead of introducing another scheduler.

## Milestone 7 — Platform and embedded robustness

Delivered foundations include plug-in host isolation, standalone ownership Decision B, T042 lifecycle stress, T053 Objective-C runtime identity, T060 Application ownership, T065 Dispatcher/timers, T045 accessibility architecture and T043 resize/scale negotiation.

### T043 — Resize/scale negotiation

**Complete in PR #142.** Public/component geometry remains logical while native/framebuffer geometry is physical; per-view finite-positive scale, authoritative configure snapshots, exact request/echo behavior, fractional conversion, embedded parent authority and reentrancy-safe preferred-size notification are qualified. T066 is independently unblocked.

### T044 — OS pointer capture qualification

**Active in PR #145.** This remains evidence-gated and is a T071 release dependency rather than a T068/T069 critical prerequisite. Each supported platform must have an explicit Outcome A/B based on the same outside-view drag/release/focus-loss/two-view fixture. Dedicated CI is now path-scoped to the actual T044 capture/platform files. The pinned Pugl X11 backend consumes translated FocusIn/FocusOut for XIC bookkeeping instead of application dispatch, so the branch's per-view focus proxy is retained only to make the required X11 focus-loss capture-cancel evidence observable; it must remain isolated and duplicate-safe.

### T066 — Standalone window controls

**Active in PR #237 and independently unblocked by T043.** Scope is limited to logical min/max constraints, runtime title/show/hide/size control and deterministic accepted-close/veto/destructor semantics on T060 Application-owned windows. Its dedicated three-platform contract and T060/T065 regressions are green; normal Windows CI is requalifying the destroy-A/resize-surviving-B integration path before any production correction is accepted.

## Milestone 8 — Packaging, virtualization, overlays and release convergence

Delivered foundations include T047/T048 package consumption, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView, T062 Tooltip, T063 Dialog and T045 semantic architecture.

### Build/example registration hardening

**Complete in PR #235 / issue #234.** Canonical `examples/features/tNNN_<feature>.cpp` sources are auto-discovered by root CMake with deterministic ordering, `CONFIGURE_DEPENDS` and malformed-name rejection.

### T061 — Generic overlay / portal layer

**Complete in PR #216.** One generic retained in-view overlay/portal layer per UI using T058 structural reconciliation, including deterministic placement, modal focus/capture behavior, anchor tracking, no-click-through dismissal, reentrant show/close safety and standalone/EmbeddedView parity.

### T067 — Fixed-height virtualized ListView

**Complete in PR #219 with startup regression closure in PR #233.** The required 100k example is constrained to the real viewport, startup materialization remains bounded, and the production `--self-test` is executed by dedicated CI.

### T063 — Dialog

**Complete in PR #227 / issue #75.** Dialog remains policy over T061 rather than a second modal/window manager. It provides one active Dialog slot per UI, validated action/result IDs, Default/Cancel semantics, styleable backdrop, bounded centered sizing, fixed title/actions, T034 scrolling for overflow, no-click-through pointer behavior, trapped focus/restoration, child-first Enter behavior, Escape Cancel/Dismissed policy, exact-once completion, safe controller/UI teardown, per-UI isolation and close-before-callback reentrancy.

### T062 — Tooltip

**Complete in PR #226 / issue #74.** A text-only retained decorator over T061/T065: exact 500 ms default delay with zero-delay checkpoint deferral, one per-instance timer/eligibility controller, no warm-up or cross-anchor reuse, shared hover/focus delay with full restart on new transitions, a `NonModal`/`Auto`/`Ignore` overlay that is non-focusable and non-hit-testable, deterministic dismissal (pointer/focus loss, PointerDown, Escape, Hidden/Collapsed/Disabled, removal, deactivation, overlay opening) with suppression until a new eligibility transition, and semantic help published through the T045 `Component::semantics()` seam independently of the rendered overlay. The native window `Impl` objects now expose the per-view T065 dispatcher so retained policies work through real Pugl events; deterministic unit coverage, an example self-test and standalone + EmbeddedView platform smoke are part of normal CI.

### T072 — Bounded Linux D-Bus transport

**Completion-ready in PR #185 / issue #84.** The exact current-main-synchronized candidate is fully qualified and provides the sole Linux D-Bus layer for T064 and T068:

- system `libdbus-1` only, no public D-Bus API/type leakage;
- one private session connection + one joinable I/O thread per transport;
- only the explicit process-wide `dbus_threads_init_default()` once-initialization;
- exact 1,024/256/256 request/subscription/object-path limits with registered client ownership;
- exactly-once request terminal states and T065-only UI callback marshalling;
- teardown-safe queued callback suppression;
- bounded signal/object-path services and a closed owned C++ value codec;
- immutable T068 provider reads on the D-Bus thread, with mutation/actions marshalled to UI;
- one lazy shared transport per T060 Application and the independently owned EmbeddedView boundary required by T068;
- package/install integration and Linux prerequisite documentation.

After T072 merges, T064 becomes Ready and should start immediately.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) ---------------------\
T058(done) -> T061(done) -> T035(done) -> T063(done) --------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
                         \-> T062(complete, PR #226) --------/
T065(done) -> T072(done) -> T064 ---------------------------+
T043(done) ---------------------> T066 ----------------------/
other explicit T069 dependencies ---------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket.

## Immediate cross-lane plan

1. Qualify and merge T062 / PR #226 (exact-head normal/path-scoped CI, mandatory review, Ready-only T042/T052 gates); T068's last dynamic/overlay dependency then reaches Done.
2. Start T064 immediately on merged T072.
3. Continue T066 independently and treat exact Windows CI evidence as authoritative for the remaining integration regression.
4. Reconcile/qualify T044 independently when primary platform streams are externally waiting.
5. Continue T038 -> T039 and T040 in the styling lane as capacity permits.
6. Keep T068 blocked until every explicit issue #80 dependency is genuinely Done; keep T069/T070/T071 dependency-gated.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

Independent tickets may progress concurrently only when explicit dependencies, shared-file conflict risk and repository concurrency rules allow it.
