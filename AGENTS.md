# AGENTS.md — NativeUI operational workflow

This file is the short bootstrap for agents working on NativeUI.

Do not turn it into a second architecture manual or code-review checklist.

Authoritative sources:

- GitHub Issues: work scope, status and dependencies.
- [`.github/ISSUE_TEMPLATE/work-item.yml`](.github/ISSUE_TEMPLATE/work-item.yml): canonical ticket schema.
- [`DESIGN.md`](DESIGN.md): architecture and subsystem contracts.
- [`CODE_REVIEW.md`](CODE_REVIEW.md): mandatory detailed review checklist and merge gate.
- [`.github/workflows/README.md`](.github/workflows/README.md): CI execution model.

## 1. Project boundary

NativeUI is a C++20 retained-mode UI toolkit for standalone applications and embedded/plugin views.

NativeUI owns UI composition, retained tree/lifetime, layout, input/focus, state/binding, drawing/widgets, text editing, styling, invalidation/animation, resources, packaging and tests.

NativeUI does **not** own audio/DSP, CLAP/VST3/AU/AAX semantics, plugin parameter automation, or a custom native windowing stack.

Pugl owns native windowing/embedding/event integration. Skia owns rendering.

## 2. Start every work session here

Before editing:

1. Read the complete GitHub issue and its explicit dependencies.
2. Read `CODE_REVIEW.md`.
3. Read `DESIGN.md` when architecture, rendering, platform integration, ownership, lifetime, threading or public API is affected.
4. Inspect current `main` and any existing PR for the issue.
5. Reuse the existing implementation branch/PR if one already exists.
6. Run the local baseline before source changes:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build
ctest --test-dir build --output-on-failure
```

Use documented `NATIVEUI_PUGL_SOURCE` / `NATIVEUI_SKIA_ROOT` overrides when dependencies already exist locally. Do not change dependency pins merely to make a local build convenient.

## 3. Ticket gate

The GitHub issue is authoritative. New issues must use the canonical issue form; do not duplicate its field schema here.

Status semantics:

- `Ready`: explicit dependencies are Done and the planning gate is READY.
- `Doing`: implementation/review/required validation is actively in progress.
- `Blocked`: an explicit dependency or real external blocker prevents progress.
- `Done`: implementation is merged, required review/evidence is complete, metadata is final, and the issue is closed.

A non-trivial ticket may enter implementation only when its planning gate has:

```text
Planner verdict: READY
Open questions: None
Architecture decisions required: None
```

If the issue, current code, `DESIGN.md`, dependencies or required invariants conflict, stop that scope and return:

```text
NEEDS_DECISION
```

Do not silently invent architecture, broaden scope, weaken an invariant, or reinterpret acceptance criteria.

## 4. Delivery workflow

Use this pipeline:

```text
PLAN
  ↓
IMPLEMENT
  ↓
VALIDATE
  ↓
INDEPENDENT REVIEW
  ↓
FIX / REVALIDATE / REREVIEW if needed
  ↓
HUMAN MERGE GATE
```

Rules:

- one implementation ticket = one independently reviewable branch/worktree = one PR;
- one writer owns an implementation worktree at a time;
- planner/verifier/reviewer roles are read-only when using multi-agent workflows;
- never create a duplicate PR for existing active work;
- independent tickets may run concurrently, but keep at most two implementation lanes active;
- local builds are always serial, even across worktrees;
- never auto-merge unless the human explicitly requests it.

Research tickets end with exactly one decision: `ADOPT`, `REJECT`, or `NEEDS_MORE_EVIDENCE`. A positive research result does not silently become a production change; create/specify a follow-up implementation ticket unless production scope was already frozen in the research ticket.

## 5. Implementation policy

Behavioral work uses local RED → GREEN → REFACTOR.

Work in coherent batches:

1. inspect the complete bounded scope first;
2. add/update tests that demonstrate the required behavior and relevant failure recovery;
3. confirm the targeted RED for the expected reason when practical;
4. implement the complete bounded correction;
5. run targeted tests;
6. prove the next normal operation still works after injected failure/rejection/exception;
7. run the relevant full local suite before publishing.

Do not:

- weaken/delete tests to obtain green;
- mix unrelated refactors into the ticket;
- publish one commit per assertion, RED step or review finding;
- use GitHub Actions as the inner development loop when the failure is reproducible locally.

Prefer one coherent implementation commit, optionally one consolidated review-fix commit, plus completion/docs bookkeeping when needed.

Keep implementation PRs Draft while source/build/tests are actively changing.

## 6. Feature examples

Every feature ticket ships a dedicated executable example:

```text
examples/features/tNNN_<feature>.cpp
nativeui_example_tNNN_<feature>
```

It must:

- exercise the public API;
- support deterministic `--self-test`;
- return non-zero on self-test failure;
- be registered with CTest when platform examples are built;
- compile against `NativeUI::Core` in display-less CI.

Infrastructure/refactor-only work does not require a meaningless example.

## 7. Build and warning rules

Local compilation is serial:

```bash
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build <build-directory>
```

Never pass `-j`, `--parallel`, or run simultaneous local builds.

NativeUI-owned code must compile with zero unapproved warnings.

- Do not disable warnings locally with `-Wno-*`, `/wd*`, pragmas, or target-level warning bypasses.
- `NATIVEUI_ALLOWED_WARNINGS` is the only normal temporary opt-in; its default remains empty.
- Any approved exception must be narrow and documented in the ticket/PR.
- Completion validation uses the default empty allowed-warning set unless the ticket records an approved exception.

## 8. Architectural invariants

Do not violate these without an explicit architecture decision:

- widgets/layout do not include Pugl, AppKit, Win32 or Xlib headers;
- normal rendering is Skia-based;
- Pugl owns native window creation/embedding/event integration;
- public geometry is logical; renderer framebuffer coordinates are physical;
- plugin parameter/audio semantics remain outside NativeUI;
- normal retained UI mutation is UI/main-thread confined unless explicitly documented otherwise;
- mutable instance-dependent process-global/singleton/thread-local state is forbidden;
- multiple NativeUI/plugin instances in one host process must coexist safely;
- callbacks that throw must not poison guards, transactions, queues or retained state;
- destructors and destructor-driven retained/native teardown are no-throw;
- partial native/resource construction must clean up every acquired registration/resource;
- deferred lifecycle work must not fall back to unsafe synchronous execution solely because enqueue failed;
- retained callbacks that can outlive a component require lifetime-safe owner/identity semantics;
- new widgets should not require a central component enum/switch;
- public headers must not leak Skia, Pugl, AppKit, Win32, Xlib or plugin SDK types without an explicitly approved low-level boundary;
- third-party acquisition goes through CMake + CPM;
- dependency versions/assets/checksums remain pinned and reproducible;
- Skia is consumed from the pinned `hemduf/skia-builder` artifacts, not rebuilt inside NativeUI;
- no SDL, GLFW, Qt, JUCE or NanoVG dependency.

Detailed architecture belongs in `DESIGN.md`.

## 9. macOS / Objective-C embedding

The Objective-C/Pugl bridge is consumer-scoped.

Each final application/module/shared-library consumer uses:

```cmake
nativeui_attach_platform(
    TARGET <target>
    CONSUMER_ID <stable-unique-id>
)
```

Do not restore a global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX`.

Do not ship a generic reusable precompiled Objective-C bridge whose runtime class names are shared by unrelated plugin/application bundles.

Runtime-visible Objective-C names used for embedding must be collision-resistant per final consumer. Detailed Objective-C runtime review remains in `CODE_REVIEW.md`.

## 10. Input/text extension rules

Keep these semantic boundaries stable:

- `KeyDown` handles commands/navigation, not committed text insertion;
- committed text uses `TextInput` / composition commit paths;
- IME preedit remains separate from `KeyDown`;
- UTF-8 correctness is mandatory;
- pointer capture is released on up/cancel/deactivation, including exceptional callback paths;
- `Canvas` is an escape hatch, not a second widget framework;
- reusable standard controls should become normal components.

## 11. Validation and CI

Before final review:

- run targeted tests for the changed behavior;
- run the relevant complete local CTest suite;
- run required sanitizer/golden/headless/platform/benchmark checks named by the ticket;
- record exact commands and results;
- explicitly record environment/platform checks that could not be run.

For correctness that cannot be established locally (native platform, ABI, packaging, dependency integration), run the focused remote check required by the ticket/workflow before merge.

Remote CI does not replace local validation or review.

## 12. Review

Every code-changing ticket must pass the complete current `CODE_REVIEW.md`.

Do not copy its detailed checklist into this file.

Final review must evaluate the current PR head against:

- the GitHub issue;
- `AGENTS.md`;
- `DESIGN.md` when applicable;
- `CODE_REVIEW.md`;
- recorded test/platform/benchmark evidence.

Collect findings before fixing them. Fix compatible findings in one coherent batch and review the resulting head again.

A PR is not at the human merge gate while any unresolved `Blocking` or `Important` finding remains.

## 13. Definition of Done

A ticket is Done only when all applicable items are true:

- acceptance criteria are met;
- required tests/fault cases exist and pass;
- feature example + `--self-test` exists when required;
- NativeUI-owned code builds with zero unapproved warnings;
- required platform/sanitizer/golden/benchmark evidence is recorded;
- complete current `CODE_REVIEW.md` review record exists;
- Blocking findings = 0;
- Important findings = 0;
- multi-instance/global-state impact was assessed;
- docs/examples were updated when behavior/API changed;
- `DESIGN.md` was updated when architecture changed;
- `ROADMAP.md` was updated when the ticket requires it;
- PR is merged;
- GitHub issue metadata is final, `status:done` is applied, and the issue is closed as completed.

Green code alone is not Done.

## 14. When blocked

Do not guess around platform/API uncertainty.

1. isolate the uncertainty in a minimal test/probe;
2. inspect the pinned dependency/API/source;
3. document the blocker or return `NEEDS_DECISION`;
4. continue another independent Ready ticket when possible.

A wait on one ticket must not stop unrelated Ready work.
