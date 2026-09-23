# AGENTS.md — NativeUI development workflow

This file is the operational source of truth for any agent continuing NativeUI.

Development tickets live in [GitHub Issues](https://github.com/hemduf/nativeui/issues?q=is%3Aissue).
`tickets/` and `TICKETS.md` are optional local recovery copies excluded from Git.
A fresh clone must use GitHub for ticket descriptions, status and dependencies.

## 1. Mission

Build a generic C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views.

NativeUI owns:

- declarative C++ composition;
- component tree and lifetime;
- layout;
- input routing and focus;
- generic UI state/binding;
- drawing primitives and widgets;
- text editing;
- styling/theme;
- invalidation/animation;
- resources;
- cross-platform packaging and tests.

NativeUI does **not** own:

- CLAP/VST3/AU/AAX APIs;
- audio/DSP;
- plugin parameter IDs, normalization or automation;
- host-specific parameter gestures;
- a custom Win32/Cocoa/X11 windowing layer.

Pugl is the windowing/embedding layer. Skia is the renderer. Dependencies are acquired with CMake + CPM. Skia binaries come from `olilarkin/skia-builder`; NativeUI does not build Skia.

## 2. Mandatory recovery sequence

At the beginning of every work session, read in this order:

1. `AGENTS.md`
2. `CODE_REVIEW.md`
3. `CI_POLICY.md`
4. `CONTEXT.md`
5. `ROADMAP.md`
6. the [GitHub issue index](https://github.com/hemduf/nativeui/issues?q=is%3Aissue), including open and closed tickets
7. the selected GitHub issue, including its latest comments and dependencies
8. `DESIGN.md` when the ticket changes architecture/platform/rendering
9. `VALIDATION.md` when the ticket touches build/platform integration

`CODE_REVIEW.md` is a mandatory merge/Done gate for every code-changing ticket. Its plug-in-host rules apply even though NativeUI itself does not implement VST3/CLAP/AU DSP APIs. `CI_POLICY.md` is the mandatory source of truth for **when** remote validation runs; it changes CI cadence, never required coverage.

Then run the current baseline tests before editing code:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build
ctest --test-dir build --output-on-failure
```

On macOS, T053 makes the Objective-C/Pugl bridge **consumer scoped**. Do not configure a global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX`. Each final application/module/shared-library consumer must attach the platform bridge with one stable non-empty `CONSUMER_ID`; the single T053 CMake derivation helper turns that exact identity into the collision-resistant runtime prefix. NativeUI's own source-tree examples/tests already register distinct identities internally. T047 owns the installed/public `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)` entry point; do not invent a source-tree-only public alternative while that package work is incomplete.

There is deliberately no generic NativeUI/Pugl fallback prefix and no reusable precompiled macOS Objective-C bridge shared across unrelated final bundles. `NativeUI::Core` and the portable Pugl C layer remain generic; only the small Cocoa/OpenGL/IME Objective-C bridge is compiled per final consumer. Core-only macOS builds (`NATIVEUI_BUILD_PLATFORM=OFF`) require no consumer platform attachment.

If dependencies are already available locally, prefer the documented `NATIVEUI_PUGL_SOURCE` / `NATIVEUI_SKIA_ROOT` overrides rather than changing the dependency model.

## 3. Ticket selection and parallelism

Keep each branch/PR scoped to one ticket or one independently reviewable sub-unit. Inside that scope, group related fixes and tests into coherent batches instead of splitting every assertion, widget state or review finding into a separate commit. Independent tickets may be active concurrently when they do not depend on each other.

Explicit GitHub `Dependencies:` are the only hard ticket-to-ticket gates. Ticket numbers and milestone order are planning aids, not implicit dependencies.

Selection order for available work:

1. a ticket is `Ready` when all explicitly listed dependencies are `Done`;
2. prefer higher priority (`P0`, then `P1`, then `P2`);
3. among equal priorities, prefer work with the greatest downstream unblock value / critical-path impact;
4. use ticket number only as a tie-breaker;
5. if active work is waiting on CI, platform validation, review infrastructure or another external condition, document the wait and continue another independent `Ready` ticket;
6. never stack a branch on another feature branch unless the downstream ticket explicitly depends on that upstream ticket;
7. merge an independent PR as soon as its own Definition of Done is satisfied; a lower-numbered open ticket is not a reason to delay the merge.

Default concurrency limit: keep at most two independent implementation lanes in active coding/review at once to reduce rebase conflicts. Local builds remain strictly serial across lanes. A PR in review or awaiting optional remote evidence does not stop another independent Ready ticket.

### 3.1 Ticket status semantics

Use status labels consistently:

- `Ready`: every explicit dependency is `Done` and no external blocker prevents starting;
- `Doing`: implementation, review, rebase or required validation is actively in progress;
- `Blocked`: at least one explicit dependency is not `Done`, or a real external blocker is documented in the issue;
- `Done`: the ticket satisfies local acceptance, review and merge/completion bookkeeping and is closed as completed. Deferred cross-platform CI qualifies an integrated `main` SHA separately.

Do not use `Blocked` merely because a lower-numbered ticket remains open.

Before coding:

- set the GitHub issue status to `Doing` in its metadata and apply `status:doing`; synchronize optional local ticket copies if present;
- write a short implementation note in the ticket if the approach is not obvious;
- do not broaden scope to neighboring tickets unless required to keep the code buildable.

### 3.2 Mandatory merge requirement in every ticket

Every GitHub issue/ticket must end with this merge requirement (or an equivalent stricter wording):

```markdown
## Merge requirement

**Mandatory:** when the implementation for this ticket is merged, update `ROADMAP.md` in the same merge/completion cycle so it reflects the ticket's final status, delivered scope, dependency frontier and milestone progress.
```

This requirement is unconditional for ticket merges. Do not skip the `ROADMAP.md` update because the milestone number appears unchanged or because the implementation is small. The roadmap is the project-level execution snapshot and must stay synchronized with every merged ticket.

When creating or editing a ticket, preserve this section as the final section of the issue body.

### 3.3 Mandatory ticket creation format

Every new NativeUI GitHub issue must use the canonical work-item formalism defined by [`.github/ISSUE_TEMPLATE/work-item.yml`](.github/ISSUE_TEMPLATE/work-item.yml).

This rule applies regardless of how the issue is created. Creating a ticket through the GitHub API, an agent, automation, migration script or another integration does **not** exempt it from the Issue Form schema. When the GitHub Issue Form UI is bypassed, reproduce the same fields, section order, defaults and mandatory completion/review content in the generated issue body.

Every new ticket must contain, in this order:

1. `Type` — `Feature`, `Bug`, `Platform`, `Infrastructure`, `Refactor` or `Research`;
2. `Priority` — `P0`, `P1` or `P2`;
3. `Milestone` — the applicable `M0`–`M8` milestone or `Backlog`;
4. `Status` — `Ready`, `Doing` or `Blocked`, using section 3.1 semantics;
5. `Dependencies` — explicit ticket dependencies only, or `None`;
6. `Objective` — the observable result, not implementation detail;
7. `Scope` — included work, relevant architectural constraints, failure/recovery behavior and explicit exclusions where useful;
8. `Acceptance criteria` — observable/testable completion criteria, including relevant failure-path recovery semantics;
9. `Required tests` — targeted tests plus every applicable deterministic fault-injection, full-suite, feature-example, platform, sanitizer, headless or golden validation;
10. `Implementation / scheduling note` — technical direction, failure/transaction boundaries, blocker or dependency/parallelization rationale when useful;
11. `Completion protocol` — the standard TDD, test, review, metadata, `CONTEXT.md` and `ROADMAP.md` completion checklist;
12. `Mandatory code review record` — the complete structured `CODE_REVIEW.md` record required by section 5, including transactional state, scheduling/queue failure, exception/unwind, partial construction, performance/allocation and privacy when applicable;
13. `Merge requirement` — the mandatory final section from section 3.2.

The title should use `TNNN — Short imperative title` for numbered roadmap work. Use a precise category prefix only for deliberately unnumbered incident/regression tickets, while still preserving the same body formalism.

When creating a ticket programmatically:

- do not invent a reduced or ad-hoc issue body;
- do not omit review/test/completion sections because the task appears small;
- keep dependencies explicit instead of inferring them from ticket number or milestone;
- synchronize the selected priority/status with GitHub labels (`priority:P0|P1|P2`, `status:ready|doing|blocked`) and set the real GitHub milestone when applicable;
- preserve the `## Merge requirement` section as the final section of the issue body;
- do not place personal information in tests/examples/code/generated metadata or ticket fixtures.

If the canonical issue form changes, update this section in the same change so `AGENTS.md` and `.github/ISSUE_TEMPLATE/work-item.yml` never define different ticket contracts.

## 4. Development workflow — local TDD, coherent batches, no micro-commit churn

TDD remains mandatory for behavioral changes, but **RED/GREEN/REFACTOR are local development states, not required Git commits or remote pushes**.

Before editing a non-trivial ticket or closeout pass:

1. read all acceptance criteria and required tests;
2. build a short completeness matrix of the affected families/variants/invariants;
3. inspect the whole relevant surface once and collect the real gaps before starting correction work;
4. identify every user/component callback, allocation, enqueue/schedule operation, resource acquisition and native/foreign boundary crossed by the changed state transition;
5. identify explicit prepare/commit/recovery points for changed state machines;
6. group related gaps that share the same invariant or subsystem into one coherent implementation batch.

For each batch:

1. add or update the failing tests needed to describe the complete bounded behavior of that batch, including deterministic failure injection where the bug class cannot be reached reliably otherwise;
2. confirm the relevant tests fail locally for the expected reasons;
3. implement the complete bounded correction for that batch;
4. run the targeted tests until the whole batch is green;
5. after any injected exception/rejection/failure, test at least one subsequent normal operation so recovery is proven rather than inferred;
6. run the relevant core/full local suite before publishing the batch;
7. for rendering changes, add/update headless/golden and failed-paint/layout recovery coverage when available;
8. for platform/windowing changes, run the relevant local/platform smoke plus partial-construction/no-throw-teardown coverage when applicable.

### 4.0.1 Commit and push policy — mandatory

The default is **batch-first history**, not micro-commits.

- Do **not** create one commit per RED test, assertion, widget state, review finding, tiny helper, or GREEN fix.
- Do **not** push a known intermediate RED merely to use GitHub Actions as the inner development loop when the behavior can be reproduced locally.
- Keep local working-tree edits, fixup commits or temporary commits private until the bounded batch is coherent and locally validated; squash/fold them before publishing when practical.
- A published implementation commit must represent a coherent, reviewable unit: tests + implementation + any required refactor for one bounded acceptance slice.
- For a normal ticket, prefer **one implementation commit**, optionally **one consolidated review-fix commit**, and the final completion/docs update when needed. More commits are acceptable only when they are independently meaningful rollback/review units, not because TDD had multiple internal steps.
- Never create a new published commit solely to record that a single test changed from RED to GREEN.
- If a ticket is in closeout, perform **one complete acceptance/review audit**, collect all actionable gaps and fix them in one bounded local correction batch before requesting review.

Exceptions to the no-micro-commit rule are limited to:

- an intentionally isolated regression/bisect point that materially improves diagnosis;
- an independently revertible safety fix;
- a platform-only RED that cannot be reproduced locally and genuinely requires a remote platform to establish the failure.

When using an exception, state why the separate published commit is necessary in the PR/ticket.

Never silently weaken or delete a test merely to make a change pass.

## 4.1 Mandatory feature example executable

Every **feature ticket** must ship a dedicated example executable in addition to unit tests.

Requirements:

- add `examples/features/tNNN_<feature>.cpp`;
- register a real `nativeui_example_tNNN_<feature>` CMake executable;
- interactive/window mode must demonstrate the public API as a user would consume it;
- the same executable must support `--self-test` and return non-zero on failure;
- register the self-test with CTest when platform examples are built;
- compile every feature-example source against `NativeUI::Core` in display-less CI so API breakage is caught even when Pugl cannot run;
- keep examples concise and feature-focused rather than turning them into hidden integration tests.

Infrastructure-only tickets (build refactors, header splitting, CI plumbing) are not feature tickets and may update an existing example instead of adding a meaningless new executable. If uncertain, treat the ticket as a feature and add the example.

## 4.2 CI execution cadence — mandatory

Follow [`CI_POLICY.md`](CI_POLICY.md).

- Keep implementation PRs Draft while source/build/tests are changing. Mark them Ready after the Mac local gate and applicable review are complete; Ready no longer launches T042/T052.
- Build serially on the Mac, run targeted and full relevant local tests, and record exact commands/results before merge. The local gate and `CODE_REVIEW.md` findings determine ticket merge readiness.
- Remote CI does not block ordinary PR merges. `Main Smoke` and path-scoped dedicated workflows run after matching merges to `main`; full `CI` qualifies the integrated branch nightly or on demand.
- `T042 Lifecycle Stress` runs weekly/on demand. `T052 v0.1 Release Gate` runs for a frozen release candidate with an explicit approved benchmark baseline SHA. Release qualification is mandatory even though ordinary ticket merges are not held for the full remote matrix.
- For native, dependency, ABI or packaging changes whose correctness cannot be established on the Mac, run the relevant focused remote check before merge and record why it is needed.
- A failed post-merge check creates a priority regression and pauses affected-area merges until corrected; unrelated Ready work may continue. A shared Core/build failure pauses all executable merges.
- Keep dedicated workflows path-scoped and cancel obsolete same-ref runs. Do not create an always-on per-ticket matrix when normal CTest/CI coverage suffices.
- Any executable-contract change after release-candidate qualification requires a new candidate and applicable reruns. Pure project-state documentation does not change executable qualification.
- Existing issue checklists keep their required tests but follow the current `CI_POLICY.md` for remote timing; stale per-PR T042/T052 wording does not block an ordinary ticket merge.

This cadence changes when remote tests run. It does not weaken local testing, review or release coverage.

## 5. Review workflow

Every code-changing ticket must perform a final review against [`CODE_REVIEW.md`](CODE_REVIEW.md). This is mandatory, not proportional to change size. Documentation-only tickets must still consider any applicable architecture/workflow rules.

The review record in the GitHub issue or PR must contain **every applicable field required by `CODE_REVIEW.md`**, including at minimum:

- instance isolation;
- globals/statics;
- threading/real-time boundaries;
- lifetime/reentrancy;
- transactional state prepare/commit/recovery;
- scheduling/queue capacity/rejection/exception-before-enqueue when work is deferred;
- exception/unwind, `noexcept`, destructor and foreign-ABI behavior;
- partial construction/resource cleanup when native/registered resources are acquired;
- Objective-C runtime rules when applicable;
- platform integration;
- performance/allocation impact;
- privacy;
- exact tests/fault seams and remaining findings.

A bare "reviewed" or an old reduced review record is not sufficient.

**Review batching rule:** before starting corrective edits from a closeout/final review, inspect the complete ticket scope and collect all current Blocking/Important findings into one review record. Correct compatible findings in one coherent batch and re-review the resulting head. Do not publish one commit/push per finding unless the findings are genuinely independent rollback units or one correction must land before another can be understood.

The passes below complement `CODE_REVIEW.md`; they do not replace it.

### Pass A — correctness/API

Check:

- ownership and lifetime;
- dangling `State<T>`, `ScrollState`, invalidator or callback captures;
- callback self-destruction/top-level owner lifetime;
- bounds and invalid state handling;
- focus behavior;
- event consumption/propagation;
- logical vs physical coordinates;
- API consistency and naming;
- no accidental plugin/audio semantics;
- per-instance ownership and no implicit mutable global/singleton/thread-local instance state;
- all intentionally process-shared state is documented and safe for simultaneous instances;
- every changed state machine has explicit prepare/commit/recovery semantics;
- guards/counters/posted/pending flags restore their exact previous valid state on exception;
- accepted work is not silently lost/duplicated when a callback throws;
- queue rejection/throw cannot violate a documented deferred-safe-point contract;
- ordinary C++ vs foreign-ABI exception behavior is explicit.

### Pass B — UI/runtime quality

Check:

- unnecessary allocations on hot input/paint paths;
- excessive full-tree scans;
- invalidation scope;
- clipping and transforms;
- pointer capture lifecycle, including throwing cancellation paths;
- redraw-at-idle regressions;
- text/UTF-8 edge cases;
- reentrancy during callbacks;
- observer add/remove/recursive-write/throw behavior;
- failed layout/paint recovery and dirty-state preservation;
- UI state is not being mutated directly from an audio/real-time thread.

### Pass C — integration/platform

Required for substantial windowing/rendering/text-input changes. Check:

- standalone and embedded lifecycle;
- repeated attach/detach/open/close;
- multiple instances, including destroying one while another remains active;
- partial native construction failure after each meaningful acquisition step;
- no-throw teardown when retained callbacks fail;
- lifecycle-control scheduling under queue full/rejection/throw;
- macOS/Windows/Linux conditional code;
- Pugl API behavior at the pinned commit;
- Skia API behavior for `chrome/m149`;
- static-library link requirements;
- symbol/process coexistence inside a host;
- when Objective-C/Objective-C++ is present, the complete Objective-C runtime section of `CODE_REVIEW.md`.

### Pass D — Objective-C runtime safety

Mandatory whenever a change introduces or modifies `.m`, `.mm`, Objective-C runtime calls, categories, protocols or generated Objective-C-visible code.

Check at minimum:

- no generic Objective-C runtime class/protocol/category names;
- every generated/runtime class name uses a **consumer/plugin-specific** collision-resistant prefix;
- a NativeUI-only class prefix is not treated as sufficient when code can be statically linked into multiple plug-in bundles;
- categories on Apple/framework classes are avoided; category selectors are prefixed if an explicitly justified category is unavoidable;
- no method swizzling, `+load`, host-wide `NSApplication` mutation or implicit process-global instance state without an explicit architecture exception;
- AppKit/UI work stays on the host UI/main thread;
- ARC/bridging/block ownership is correct.

For significant architectural changes, perform all applicable passes and correct findings before marking the ticket done.

## 6. Architectural invariants

Do not violate these without an explicit architecture ticket:

- widgets and layout code do not include Pugl headers;
- widgets do not include Win32/AppKit/Xlib headers;
- normal rendering is Skia-based;
- Pugl owns native window creation/embedding/event pump;
- `PUGL_MODULE` polling never blocks;
- public component geometry is logical coordinates;
- the Skia framebuffer is physical pixels;
- new components do not require adding a central component `enum`/switch;
- plugin parameter semantics remain external;
- mutable instance-dependent process-global/singleton/thread-local state is forbidden;
- `State<T>`, `ScrollState` and normal retained UI mutation are UI/main-thread confined unless an API explicitly documents thread safety;
- callbacks that throw cannot leave NativeUI guard/transaction/queue state poisoned;
- destructors and destructor-driven retained/native teardown are no-throw;
- retained callbacks/invalidators that intentionally outlive a component use lifetime-safe owner/identity semantics;
- deferred lifecycle work never falls back to unsafe synchronous execution solely because ordinary queue enqueue failed;
- partial native construction leaves no registered callback/native resource behind;
- Objective-C runtime-visible classes generated/defined for plug-in embedding must use consumer/plugin-specific collision-resistant names;
- on macOS, generic Core/Pugl C code may be shared, but Objective-C Pugl/OpenGL/IME bridge sources are compiled per final consumer identity; no fixed framework-level runtime prefix or generic precompiled Objective-C bridge is reusable across unrelated final bundles;
- no SDL, GLFW, Qt, JUCE or NanoVG dependency;
- third-party acquisition goes through CPM;
- Skia is consumed from `skia-builder`, not rebuilt by NativeUI;
- dependency versions/assets/checksums remain pinned/reproducible.

## 7. Component extension rules

A normal new widget should be implementable as a new `Component` + DSL builder without modifying a central registry.

A new low-level capability may legitimately require core work, for example:

- a new input event class;
- a new layout primitive;
- a new generic painter operation;
- accessibility semantics;
- a new platform service.

When that happens, implement the generic capability first, then the widget.

## 8. Canvas rules

`Canvas` is a generic escape hatch, not a second widget framework.

- render callback receives local coordinates;
- input callback receives local pointer coordinates;
- `on_input()` makes the canvas focusable unless an explicit focus API supersedes this later;
- pointer capture must always be released on PointerUp/cancel/deactivation, including exceptional callback paths;
- reusable standard widgets should still become first-class components rather than permanent Canvas implementations.

## 9. Text rules

- `KeyDown` is for commands/navigation, not text insertion;
- committed text enters through `TextInput` events from Pugl;
- maintain UTF-8 correctness;
- advanced IME composition/pre-edit remains a platform-extension task until implemented explicitly;
- clipboard behavior stays asynchronous where the platform API requires it.

## 10. Build/dependency workflow

CMake + CPM is mandatory.

### 10.0 Local builds must be serial

Parallel local compilation can exhaust memory. This is a mandatory local execution constraint:

- never pass `-j`, `-jN`, `-j N` or `--parallel` to local build commands, including CMake, Make and Ninja;
- use `CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build <build-directory>` for every local CMake build, including targeted builds, so generators such as Ninja cannot silently use their parallel default;
- run only one local build at a time; do not start simultaneous builds in different terminals, worktrees or agent tasks;
- do not copy CI parallel-build flags into local commands. Remote CI concurrency remains governed by `CI_POLICY.md` and the workflows.

### 10.1 Compiler warning policy

NativeUI-owned code must compile with **zero unapproved warnings**. `nativeui_enable_project_warnings()` treats warnings as errors (`-Werror` on Clang/GCC, `/WX` on MSVC); do not weaken that policy on individual NativeUI targets.

The only normal way to accept a diagnostic temporarily is the explicit CMake cache setting `NATIVEUI_ALLOWED_WARNINGS`, whose default is empty. An accepted Clang/GCC diagnostic is named without the `-W` prefix (for example `deprecated-declarations`); an MSVC diagnostic uses its four-digit warning code. The configure invocation is the approval record, for example:

```bash
cmake -S . -B build -DNATIVEUI_ALLOWED_WARNINGS=deprecated-declarations
```

Rules:

- fix the warning instead of allowing it whenever the code is under NativeUI control;
- never add a warning to the default allowed set;
- never bypass this policy in NativeUI-owned code with `-Wno-*`, `/wd*`, diagnostic pragmas, `COMPILE_WARNING_AS_ERROR=OFF`, or equivalent target/source-local suppression;
- if a warning genuinely must be accepted, keep the allowance as narrow as possible, document the reason in the ticket/PR, and require the explicit CMake opt-in;
- dependency-boundary/platform suppressions for third-party source must remain narrowly scoped and documented; any new third-party exception must be exposed as an explicit CMake opt-in rather than silently broadening suppression;
- CI and completion validation use the default empty `NATIVEUI_ALLOWED_WARNINGS` unless a ticket records an explicitly approved exception.

A build that emits an unapproved NativeUI warning is failed work, not a successful build with a note.

### Pugl

- source dependency;
- exact pinned commit;
- compiled statically inside NativeUI/final consumers;
- only required platform + OpenGL backend sources;
- Windows/Linux keep one generic Pugl platform target;
- macOS compiles `common.c`/`internal.c` once as generic platform C code, while `mac.m`, `mac_gl.m` and NativeUI's Cocoa IME bridge are compiled into a small final-consumer bridge;
- macOS consumer identity is the source of truth: derive runtime names only through T053's frozen `nativeui_compute_objc_runtime_prefix()` algorithm and attach each final target exactly once with its stable `CONSUMER_ID`;
- never require or restore a global/cache `NATIVEUI_OBJC_RUNTIME_PREFIX` as a normal consumer path;
- never publish a generic precompiled static macOS Objective-C platform archive whose runtime names cannot vary per final application/plug-in consumer.

### Skia

- binary dependency from `olilarkin/skia-builder` release assets;
- exact tag/asset/SHA256;
- support both CPM-flattened archive layout and manual extraction layout;
- do not introduce GN/Ninja/depot_tools into NativeUI.

When updating either dependency, create a dedicated ticket and update:

- `THIRD_PARTY.md`;
- `VALIDATION.md`;
- `CONTEXT.md`;
- build matrix results.

## 11. Definition of Done

A ticket is `Done` only when:

- acceptance criteria are met;
- required tests exist and pass, including deterministic failure-path tests required by `CODE_REVIEW.md` for the changed domain;
- every feature ticket has a dedicated executable example with a passing `--self-test`;
- NativeUI-owned targets build with the default empty `NATIVEUI_ALLOWED_WARNINGS` and emit no compiler warnings; any explicitly approved exception is recorded in the ticket/PR and requires the corresponding CMake opt-in;
- all applicable review passes are complete;
- the **complete current** mandatory `CODE_REVIEW.md` review record is present in the issue or PR — old reduced records are insufficient — and all Blocking/Important findings are corrected;
- the exact local Mac validation and applicable review evidence required by `CI_POLICY.md` are recorded; any risk-specific pre-merge remote check required by that policy is green;
- multi-instance/global-state impact is explicitly assessed for every code change;
- transactional state, scheduling/queue failure, exception/unwind, partial construction, lifetime/reentrancy, performance/allocation and privacy fields are explicitly assessed when applicable;
- Objective-C runtime naming/prefix strategy is recorded whenever Objective-C/Objective-C++ code is touched;
- docs/API examples are updated when behavior changed;
- the GitHub issue is marked `Done` with `status:done` and closed as completed; optional local ticket copies are synchronized if present;
- `CONTEXT.md` is updated with current state and next recommended ticket;
- `ROADMAP.md` is updated in the same merge/completion cycle for **every merged ticket**, including final status, delivered scope, dependency-frontier changes and milestone progress;
- the issue body still ends with the mandatory `## Merge requirement` section.

A ticket must not be considered complete merely because its code and local tests are green if the roadmap synchronization or current review record is missing. `Done` does not imply that the ticket SHA passed deferred cross-platform CI; release candidates require the full green remote evidence in `CI_POLICY.md`.

## 12. End-of-session compaction

Before ending any session, update `CONTEXT.md` so another agent can resume without chat history.

Keep it compact. It must contain:

- current architecture and pinned dependency versions;
- current build/test state;
- last completed ticket;
- ticket currently in progress, if any;
- exact known failures/blockers;
- next recommended ticket;
- important temporary decisions that are not yet in `DESIGN.md`.

Do not turn `CONTEXT.md` into a changelog. Move durable decisions to `DESIGN.md` and completed history to ticket files.

## 13. Git workflow when a repository is available

For each ticket:

- branch/PR scope should match one ticket or one independently reviewable sub-unit;
- keep the PR in Draft during active source/build/test iteration;
- perform fine-grained RED/GREEN cycles locally, but publish coherent batches rather than the internal TDD steps;
- commit tests with the implementation they validate in the same coherent batch whenever practical;
- default published history is one implementation commit plus, if needed, one consolidated review-fix commit and completion/docs bookkeeping; use more commits only for independently meaningful rollback/review units;
- use local fixup/squash workflows freely to keep the remote branch readable;
- avoid drive-by formatting or unrelated refactors;
- before requesting review or merge, build the affected surface and run targeted plus full relevant local tests, including fault-injection recovery paths;
- complete the mandatory review and mark the locally validated PR Ready for review; merge when the ticket gate is satisfied without waiting for ordinary remote CI;
- after merge, monitor the `main` smoke, path-scoped and nightly integration results and follow `CI_POLICY.md` on failures;
- freeze and qualify a separate exact release-candidate SHA with full remote gates before release;
- update the GitHub issue, `CONTEXT.md` and `ROADMAP.md` in the final merge/completion cycle;
- do not merge/close the ticket with a stale roadmap or stale reduced code-review record.

Independent branches should start from `main`, not from another feature branch, unless an explicit ticket dependency requires stacking. Rebase or merge `main` only when needed to validate integration or resolve conflicts.

Suggested commit style for a coherent batch:

```text
core(TNNN): complete dirty-region aggregation
layout(TNNN): add min/max constraints and coverage
input(TNNN): complete focus-scope behavior
widget(TNNN): implement slider contract and tests
platform(TNNN): harden embedded Pugl lifecycle
```

Avoid histories such as `test: add RED`, `fix: make RED green`, `test: next case`, `fix: next case` for a sequence that is one bounded acceptance slice. Keep that sequence local and publish the completed slice.

## 14. When blocked

Do not guess around a platform/API uncertainty.

1. isolate the uncertainty in a minimal test or probe;
2. inspect the pinned Pugl/Skia API/source and current VST3/CLAP/Objective-C runtime documentation when the uncertainty concerns plug-in embedding;
3. document the blocker in the ticket and `CONTEXT.md`;
4. continue another independent `Ready` ticket if possible.

A remote/platform wait on one ticket must not become a global project stop when independent `Ready` work exists. Post-merge failures block affected-area merges as defined by `CI_POLICY.md`.
