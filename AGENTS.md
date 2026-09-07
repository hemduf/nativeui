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
3. `CONTEXT.md`
4. `ROADMAP.md`
5. the [GitHub issue index](https://github.com/hemduf/nativeui/issues?q=is%3Aissue), including open and closed tickets
6. the selected GitHub issue, including its latest comments and dependencies
7. `DESIGN.md` when the ticket changes architecture/platform/rendering
8. `VALIDATION.md` when the ticket touches build/platform integration

`CODE_REVIEW.md` is a mandatory merge/Done gate for every code-changing ticket. Its plug-in-host rules apply even though NativeUI itself does not implement VST3/CLAP/AU DSP APIs.

Then run the current baseline tests before editing code:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

On macOS, any platform build must additionally provide a **consumer/application/plugin-specific** Objective-C runtime prefix, normally derived from the final bundle identifier:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_OBJC_RUNTIME_PREFIX=ComVendorProduct_
```

There is deliberately no generic NativeUI/Pugl fallback prefix: a static platform library with one framework-level Objective-C class prefix can still collide when copied into several plug-in bundles loaded by the same host. Core-only macOS builds (`NATIVEUI_BUILD_PLATFORM=OFF`) do not require the prefix.

If dependencies are already available locally, prefer the documented `NATIVEUI_PUGL_SOURCE` / `NATIVEUI_SKIA_ROOT` overrides rather than changing the dependency model.

## 3. Ticket selection and parallelism

Keep each branch/PR scoped to one small ticket or one independently reviewable sub-unit. Independent tickets may be active concurrently when they do not depend on each other.

Explicit GitHub `Dependencies:` are the only hard ticket-to-ticket gates. Ticket numbers and milestone order are planning aids, not implicit dependencies.

Selection order for available work:

1. a ticket is `Ready` when all explicitly listed dependencies are `Done`;
2. prefer higher priority (`P0`, then `P1`, then `P2`);
3. among equal priorities, prefer work with the greatest downstream unblock value / critical-path impact;
4. use ticket number only as a tie-breaker;
5. if active work is waiting on CI, platform validation, review infrastructure or another external condition, document the wait and continue another independent `Ready` ticket;
6. never stack a branch on another feature branch unless the downstream ticket explicitly depends on that upstream ticket;
7. merge an independent PR as soon as its own Definition of Done is satisfied; a lower-numbered open ticket is not a reason to delay the merge.

Default concurrency limit: keep at most three implementation lanes in active coding/review at once to reduce rebase conflicts. A PR that is merely waiting on CI does not consume an implementation lane if another independent ticket can progress safely.

### 3.1 Ticket status semantics

Use status labels consistently:

- `Ready`: every explicit dependency is `Done` and no external blocker prevents starting;
- `Doing`: implementation, review, rebase or required validation is actively in progress;
- `Blocked`: at least one explicit dependency is not `Done`, or a real external blocker is documented in the issue;
- `Done`: the ticket satisfies the Definition of Done and is closed as completed.

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
7. `Scope` — included work, relevant architectural constraints and explicit exclusions where useful;
8. `Acceptance criteria` — observable/testable completion criteria;
9. `Required tests` — targeted tests plus every applicable full-suite, feature-example, platform, headless or golden validation;
10. `Implementation / scheduling note` — technical direction, blocker or dependency/parallelization rationale when useful;
11. `Completion protocol` — the standard TDD, test, review, metadata, `CONTEXT.md` and `ROADMAP.md` completion checklist;
12. `Mandatory code review record` — the structured `CODE_REVIEW.md` record required by section 5;
13. `Merge requirement` — the mandatory final section from section 3.2.

The title should use `TNNN — Short imperative title` for numbered roadmap work. Use a precise category prefix only for deliberately unnumbered incident/regression tickets, while still preserving the same body formalism.

When creating a ticket programmatically:

- do not invent a reduced or ad-hoc issue body;
- do not omit review/test/completion sections because the task appears small;
- keep dependencies explicit instead of inferring them from ticket number or milestone;
- synchronize the selected priority/status with GitHub labels (`priority:P0|P1|P2`, `status:ready|doing|blocked`) and set the real GitHub milestone when applicable;
- preserve the `## Merge requirement` section as the final section of the issue body.

If the canonical issue form changes, update this section in the same change so `AGENTS.md` and `.github/ISSUE_TEMPLATE/work-item.yml` never define different ticket contracts.

## 4. Development workflow — TDD and small units

For every behavioral change:

1. add or update the smallest failing test first;
2. confirm the test fails for the expected reason;
3. implement the smallest change that makes it pass;
4. run the targeted test;
5. run the complete core test suite;
6. for rendering changes, add/update a headless/golden test when available;
7. for platform/windowing changes, run the relevant platform smoke test when available.

Prefer several small commits/patches over one large rewrite.

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

## 5. Review workflow

Every code-changing ticket must perform a final review against [`CODE_REVIEW.md`](CODE_REVIEW.md). This is mandatory, not proportional to change size. Documentation-only tickets must still consider any applicable architecture/workflow rules.

The review record in the GitHub issue or PR must explicitly cover instance isolation, globals/statics, threading/real-time boundaries, lifetime/reentrancy, Objective-C runtime rules when applicable, platform integration and tests. A bare "reviewed" is not sufficient.

The passes below complement `CODE_REVIEW.md`; they do not replace it.

### Pass A — correctness/API

Check:

- ownership and lifetime;
- dangling `State<T>` or callback captures;
- bounds and invalid state handling;
- focus behavior;
- event consumption/propagation;
- logical vs physical coordinates;
- API consistency and naming;
- no accidental plugin/audio semantics;
- per-instance ownership and no implicit mutable global/singleton/thread-local instance state;
- all intentionally process-shared state is documented and safe for simultaneous instances.

### Pass B — UI/runtime quality

Check:

- unnecessary allocations on hot input/paint paths;
- excessive full-tree scans;
- invalidation scope;
- clipping and transforms;
- pointer capture lifecycle;
- redraw-at-idle regressions;
- text/UTF-8 edge cases;
- reentrancy during callbacks;
- UI state is not being mutated directly from an audio/real-time thread.

### Pass C — integration/platform

Required for substantial windowing/rendering/text-input changes. Check:

- standalone and embedded lifecycle;
- repeated attach/detach/open/close;
- multiple instances, including destroying one while another remains active;
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
- `State<T>` and normal retained UI mutation are UI/main-thread confined unless an API explicitly documents thread safety;
- Objective-C runtime-visible classes generated/defined for plug-in embedding must use consumer/plugin-specific collision-resistant names;
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
- pointer capture must always be released on PointerUp/cancel/deactivation;
- reusable standard widgets should still become first-class components rather than permanent Canvas implementations.

## 9. Text rules

- `KeyDown` is for commands/navigation, not text insertion;
- committed text enters through `TextInput` events from Pugl;
- maintain UTF-8 correctness;
- advanced IME composition/pre-edit remains a platform-extension task until implemented explicitly;
- clipboard behavior stays asynchronous where the platform API requires it.

## 10. Build/dependency workflow

CMake + CPM is mandatory.

### Pugl

- source dependency;
- exact pinned commit;
- compiled statically inside NativeUI;
- only required platform + OpenGL backend sources;
- on macOS, every compiled Objective-C runtime class from the Pugl bridge must be renamed with `NATIVEUI_OBJC_RUNTIME_PREFIX`, and that prefix must be unique to the final consumer/plugin binary;
- never publish a generic precompiled static macOS platform archive whose Objective-C runtime prefix cannot vary per final plug-in consumer.

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
- required tests exist and pass;
- every feature ticket has a dedicated executable example with a passing `--self-test`;
- no newly introduced compiler errors/warnings attributable to NativeUI remain on the tested platform;
- all applicable review passes are complete;
- the mandatory `CODE_REVIEW.md` review record is present in the issue or PR and all blocking findings are corrected;
- multi-instance/global-state impact is explicitly assessed for every code change;
- Objective-C runtime naming/prefix strategy is recorded whenever Objective-C/Objective-C++ code is touched;
- docs/API examples are updated when behavior changed;
- the GitHub issue is marked `Done` with `status:done` and closed as completed; optional local ticket copies are synchronized if present;
- `CONTEXT.md` is updated with current state and next recommended ticket;
- `ROADMAP.md` is updated in the same merge/completion cycle for **every merged ticket**, including final status, delivered scope, dependency-frontier changes and milestone progress;
- the issue body still ends with the mandatory `## Merge requirement` section.

A ticket must not be considered complete merely because code and CI are green if the roadmap synchronization step has not been performed.

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

- branch/commit scope should match one ticket or one independently reviewable sub-unit;
- commit tests with the implementation they validate;
- avoid drive-by formatting or unrelated refactors;
- before merge, run the complete relevant test set;
- perform and record the mandatory `CODE_REVIEW.md` review before merge;
- update the GitHub issue, `CONTEXT.md` and `ROADMAP.md` in the final merge/completion cycle;
- do not merge/close the ticket with a stale roadmap.

Independent branches should start from `main`, not from another feature branch, unless an explicit ticket dependency requires stacking. Rebase or merge `main` only when needed to validate integration or resolve conflicts.

Suggested commit style:

```text
core: add dirty-region aggregation
layout: add min/max constraints
input: add focus scopes
widget: add slider
render: add gradient paint
platform: harden embedded Pugl lifecycle
```

## 14. When blocked

Do not guess around a platform/API uncertainty.

1. isolate the uncertainty in a minimal test or probe;
2. inspect the pinned Pugl/Skia API/source and current VST3/CLAP/Objective-C runtime documentation when the uncertainty concerns plug-in embedding;
3. document the blocker in the ticket and `CONTEXT.md`;
4. continue another independent `Ready` ticket if possible.

A CI/platform wait on one branch must not become a global project stop when independent `Ready` work exists.

Do not redesign the whole toolkit to work around a single unverified platform issue.

## 15. Iteration ZIP artifact

For this project, one completed ticket is one development iteration unless explicitly regrouped.

At the end of every completed iteration:

1. finish the ticket completion protocol;
2. update the GitHub issue, `ROADMAP.md` and `CONTEXT.md`; the roadmap update is mandatory for every merged ticket, not only when milestone scope changes;
3. include `AGENTS.md`, `CODE_REVIEW.md`, `CONTEXT.md`, roadmap, plan, source, tests and CMake files; optional local ticket exports may be included in the recovery ZIP but remain excluded from Git;
4. exclude build directories, downloaded dependencies and generated binaries;
5. create a versioned/recoverable ZIP named with the completed ticket, for example `nativeui_T011.zip`;
6. provide that ZIP to the user as the recovery snapshot for that iteration.

The ZIP is a recovery snapshot, not a substitute for Git history and not a dependency gate for unrelated `Ready` work. It must be sufficient for another agent to resume by following section 2.
