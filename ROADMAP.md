# NativeUI roadmap

**Updated:** 2026-09-12

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

The pre-T066 merge baseline is current `main` at `c6575cf8fe4301086af088642fc93ba21e61f5e5`. It includes completed T072, T062 with its post-merge completeness fix, T063, T035, T043, T061, T067, T045, T037, T058, T036, T065, T034 and T060. This completion cycle adds T066 / PR #237.

T072 / issue #84 / PR #185 is complete and merged. Its final executable candidate passed normal CI, T072 Linux D-Bus Contract, T060 Application Contract, T065 Platform Dispatcher, T067 Virtual List Contract, T042 Lifecycle Stress and T052 v0.1 Release Gate with a clean final `CODE_REVIEW.md` review.

T062 / issue #74 / PR #226 is complete and merged as `7269310995adad1e7b474b6cea319644fc8020f4`; PR #245 / `86c12e8472a82cd929a95c24705995dfa871bbf5` closes its post-merge completeness gaps. It is no longer a T068 blocker.

T066 / issue #78 / PR #237 is completion-qualified. Frozen executable candidate `d6da7c472d7819269e87f0cdbccf6ac99ebfb174` is synchronized with current `main`; current completion head adds only project/release documentation. Normal/path-scoped validation is green (CI `34686088854`, T066 `34686088812`, T060 `34686088852`, T065 `34686088808`, T072 `34686088814`, Package Contracts `34686088902`) and final-candidate T042 `34686364025` plus T052 `34686364059` are green. Reviews `5185850366` and `5186026531` record the complete #78 requirement-to-code/test matrix with no Blocking/Important finding.

Current dependency frontier:

```text
critical UI:       T034(done) -> T036(done) -> T045(done) -> T067(done) -> T068
                                                   T058(done) ------------^

dynamic/overlay:   T058(done) -> T061(done)
                                      |-> T035(done) --------------------> T068
                                      |-> T063(done) --------------------> T068
                                      +-> T062(done) --------------------> T068

style:             T037(done) -> T038 -> T039
                                +-> T040 with T065(done)

lifecycle/release: #64(done) -> T060(done) -> #139(done) -> T052(done)
                   T042(done) -> T051(done) ---------------------> T052(done)

platform/package:  T053(done) -> T047(done) -> T048(done)
                                       |-> T054(done)
                                       +-> T056(done) + T022(done) -> T057(done)

critical platform: T060(done) -> T065(done) -> T072(done) -> T064(active PR #240)
                   T041(done) -> T043(done) -> T066(done, PR #237)
                                      |-------> T068

release:            all explicit convergence -> T068 -> T069 -> T070 -> T071 -> v1.0.0
```

T064 is the remaining primary platform prerequisite on merged/Done T072. T044 remains an explicit T071 dependency and is qualified independently.

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

**Complete in PR #142.** Public/component geometry remains logical while native/framebuffer geometry is physical; per-view finite-positive scale, authoritative configure snapshots, exact request/echo behavior, fractional conversion, embedded parent authority and reentrancy-safe preferred-size notification are qualified.

### T044 — OS pointer capture qualification

**Active in PR #145.** This remains evidence-gated and is a T071 release dependency rather than a T068/T069 critical prerequisite. The implementation has an explicit per-platform Outcome A/B matrix: macOS remains no-change Outcome A; Windows uses a minimal per-view retained/native release bridge with real system pointer injection evidence; Linux/X11 uses retained/native release plus the narrow per-view focus visibility seam required by pinned Pugl. The branch is synchronized with current main at `608de6c2fce6f960b92c6534699b599fbc318863`; normal/path-scoped requalification precedes a fresh Ready transition and heavyweight T042/T052 gates.

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

Delivered foundations include T047/T048 package consumption, T051 performance qualification, T052 v0.1 release gate, T054 application helper, T056 binary data, T057 ResourceManager, T058 dynamic composition, T061 overlay/portal infrastructure, T067 fixed-height virtualized ListView, T062 Tooltip, T063 Dialog, T072 Linux D-Bus transport and T045 semantic architecture.

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

**Complete in PR #185 / issue #84.** It provides the sole Linux D-Bus layer for T064 and T068:

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

### T064 — Cross-platform DesktopServices

**Active in PR #240 / issue #76.** Public coordinator semantics, fixed native macOS/Windows/Linux backends, T072-only Linux portal integration, 1 chooser / 16 URL owner-local capacity and the fake-only deterministic feature self-test are implemented. A concrete Windows native smoke timeout on the first all-native fixture is being requalified on exact head `0ce66349d84483c22ca4d9b1c7f98c47a8b76e6e` with a real owner HWND, message pumping and owner-aware modal discovery; production DesktopServices behavior was not changed by that test recovery.

### T068 convergence

T068 starts only after **all** explicit issue #80 dependencies are Done. It implements T045 semantics through immutable per-view snapshots and native NSAccessibility/UIA/AT-SPI2 bridges, shares T067 virtual metadata, routes mutations through T065 and uses T072 as the sole Linux D-Bus transport.

### Final v1 release path

```text
T036(done) -> T045(done) -> T067(done) ---------------------\
T058(done) -> T061(done) -> T035(done) -> T063(done) --------+--> T068 -> T069 -> T070 -> T071 -> v1.0.0
                         \-> T062(done) ---------------------/
T065(done) -> T072(done) -> T064 ---------------------------+
T043(done) ---------------------> T066(done) ----------------/
other explicit T069 dependencies ---------------------------/
```

T069 is the final v1 public API freeze and cannot start until every explicit issue #81 dependency is complete. T070 validates the reference application/Getting Started against the frozen API. T071 is validation/release-only on one exact RC SHA; defects found there return to their canonical fix ticket.

## Immediate cross-lane plan

1. Merge T066 / PR #237 after this same-cycle project-state synchronization; final T042/T052 qualification is green.
2. Finish T064 / PR #240, then merge after exact-head native/platform/completeness/final-candidate qualification.
3. Re-qualify T044 / PR #145 on current main and complete its final Ready-only T042/T052 release-dependency gate.
4. Continue T038 -> T039 and T040 in the styling lane as capacity permits.
5. Start T068 only when every explicit issue #80 dependency is genuinely Done; keep T069/T070/T071 dependency-gated.

## Prioritization rule

```text
core correctness
  -> layout/input/render/text foundations
  -> widgets/styles/overlays/platform services
  -> accessibility convergence and public API freeze
  -> reference package/release
```

Independent tickets may progress concurrently only when explicit dependencies, shared-file conflict risk and repository concurrency rules allow it.
