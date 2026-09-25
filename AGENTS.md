# AGENTS.md — NativeUI agent bootstrap

This file is the short operational contract for agents. Keep detailed architecture and review rules out of it.

Authoritative sources:

- GitHub Issues — scope, status, dependencies and acceptance criteria.
- [`.github/ISSUE_TEMPLATE/work-item.yml`](.github/ISSUE_TEMPLATE/work-item.yml) — ticket schema.
- [`DESIGN.md`](DESIGN.md) — architecture and subsystem contracts.
- [`CODE_REVIEW.md`](CODE_REVIEW.md) — detailed mandatory review gate.
- [`.github/workflows/README.md`](.github/workflows/README.md) — CI model.

## 1. Project boundary

NativeUI is a C++20 retained-mode UI toolkit for standalone and embedded/plugin views.

NativeUI owns UI composition, retained tree/lifetime, layout, input/focus, state, drawing/widgets, text, styling, invalidation, resources, packaging and tests.

NativeUI does not own audio/DSP, plugin-format semantics/automation, or a custom native windowing stack.

Pugl owns native windowing/embedding/event integration. Skia owns rendering.

## 2. Before editing

1. Read the complete GitHub issue and explicit dependencies.
2. Read `CODE_REVIEW.md`.
3. Read `DESIGN.md` for architecture/rendering/platform/public-API/ownership/threading work.
4. Inspect current `main` and reuse any existing active PR for the issue.
5. Run the baseline:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build
ctest --test-dir build --output-on-failure
```

Use documented `NATIVEUI_PUGL_SOURCE` / `NATIVEUI_SKIA_ROOT` overrides when dependencies already exist locally. Do not change dependency pins for local convenience.

## 3. Ticket gate

New issues use the canonical issue form; do not duplicate its schema here.

Status:

- `Ready`: dependencies Done + planning gate READY.
- `Doing`: implementation/review/required validation is active.
- `Blocked`: an explicit dependency or real external blocker prevents progress.
- `Done`: merged, reviewed, evidence/bookkeeping complete, issue closed.

Implementation starts only when:

```text
Planner verdict: READY
Open questions: None
Architecture decisions required: None
```

If issue, code, dependencies or architecture conflict, stop that scope with:

```text
NEEDS_DECISION
```

Never silently invent architecture, broaden scope, weaken an invariant/test, or reinterpret acceptance criteria.

## 4. Delivery workflow

```text
PLAN → IMPLEMENT → VALIDATE → INDEPENDENT REVIEW
                     ↑                ↓
                     └──── FIX ───────┘
                              ↓
                      HUMAN MERGE GATE
```

Rules:

- one implementation ticket = one branch/worktree = one PR;
- one writer per implementation worktree;
- planner/verifier/reviewer are read-only in multi-agent workflows;
- never duplicate an existing active PR;
- at most two independent implementation lanes;
- local builds are always serial across all lanes;
- no automatic merge unless explicitly requested by the human.

Research tickets end with `ADOPT`, `REJECT`, or `NEEDS_MORE_EVIDENCE`. `ADOPT` normally creates a separate production implementation ticket.

## 5. Implementation

Behavioral work uses local RED → GREEN → REFACTOR.

For each bounded batch:

1. inspect the complete affected surface;
2. add/update tests, including deterministic failure recovery where applicable;
3. confirm the expected RED when practical;
4. implement the bounded correction;
5. run targeted tests;
6. after injected failure, prove a subsequent normal operation still works;
7. run the relevant full local suite before publishing.

Never weaken/delete tests for green, mix unrelated refactors, or publish one commit per assertion/RED/review finding.

Prefer one coherent implementation commit, optionally one consolidated review-fix commit, plus completion/docs bookkeeping.

Keep PRs Draft while source/build/tests are actively changing.

## 6. Feature tickets

Every feature ticket ships a dedicated public-API example:

```text
examples/features/tNNN_<feature>.cpp
nativeui_example_tNNN_<feature>
```

It must support deterministic `--self-test`, return non-zero on failure, be registered with CTest when applicable, and compile against `NativeUI::Core` in display-less CI.

Infrastructure/refactor-only work does not need a meaningless example.

## 7. Build rules

Local compilation is serial:

```bash
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build <build-directory>
```

Never use `-j`, `--parallel`, or simultaneous local builds.

NativeUI-owned code must have zero unapproved warnings.

- Do not bypass warnings with local `-Wno-*`, `/wd*`, pragmas or target-level suppression.
- `NATIVEUI_ALLOWED_WARNINGS` is the only normal temporary opt-in; default is empty.
- Any exception must be narrow and documented.
- Final validation uses the default empty allowance unless the ticket explicitly records an approved exception.

## 8. Non-negotiable architecture

Do not violate these without an explicit architecture decision:

- widgets/layout do not depend on Pugl/AppKit/Win32/Xlib;
- normal rendering is Skia; Pugl owns native window/event integration;
- public geometry is logical; renderer framebuffer coordinates are physical;
- plugin/audio parameter semantics stay outside NativeUI;
- retained UI mutation is UI/main-thread confined unless explicitly documented otherwise;
- mutable instance-dependent process-global/singleton/thread-local state is forbidden;
- multiple NativeUI/plugin instances in one host process must coexist safely;
- throwing callbacks must not poison guards, transactions, queues or retained state;
- destructor-driven retained/native teardown is no-throw;
- partial construction cleans every acquired native/resource registration;
- deferred lifecycle work never falls back to unsafe synchronous execution just because enqueue failed;
- callbacks that can outlive a component require lifetime-safe identity/ownership;
- public headers do not leak backend/platform/plugin-SDK types without an approved low-level boundary;
- new widgets should not require a central component enum/switch;
- third-party acquisition goes through CMake + CPM with pinned/reproducible assets;
- Skia comes from pinned `hemduf/skia-builder` artifacts; NativeUI does not rebuild it;
- no SDL, GLFW, Qt, JUCE or NanoVG dependency.

Detailed architecture belongs in `DESIGN.md`.

### macOS embedding

The Objective-C/Pugl bridge is consumer-scoped. Final consumers use `nativeui_attach_platform(TARGET ... CONSUMER_ID ...)` with a stable unique identity.

Do not restore a global/manual `NATIVEUI_OBJC_RUNTIME_PREFIX` or a generic reusable precompiled Objective-C bridge shared across unrelated bundles. Runtime-visible Objective-C names must be collision-resistant per final consumer.

Detailed Objective-C runtime rules remain in `CODE_REVIEW.md`.

## 9. Validation

Before final review:

- run targeted tests;
- run the relevant complete local CTest suite;
- run ticket-required sanitizer/golden/headless/platform/benchmark checks;
- record exact commands/results and explicit environment limitations;
- run focused remote validation before merge when native/ABI/packaging/dependency correctness cannot be established locally.

Remote CI does not replace local validation or review.

## 10. Review

Every code-changing ticket passes the complete current `CODE_REVIEW.md`; do not copy that checklist here.

Final review uses the current PR head, issue, applicable `DESIGN.md`, this file, `CODE_REVIEW.md`, and recorded evidence.

Collect findings before corrective edits; fix compatible findings in coherent batches and re-review the resulting head.

Human merge gate requires:

```text
Blocking findings: 0
Important findings: 0
```

## 11. Definition of Done

A ticket is Done only when applicable requirements are complete:

- acceptance criteria and required tests/fault cases pass;
- feature example + `--self-test` exists when required;
- zero unapproved NativeUI warnings;
- required platform/sanitizer/golden/benchmark evidence is recorded;
- current complete `CODE_REVIEW.md` record exists;
- Blocking = 0 and Important = 0;
- multi-instance/global-state impact is assessed;
- docs/examples and `DESIGN.md` are updated when behavior/architecture changed;
- `ROADMAP.md` is updated when required by the ticket;
- PR is merged;
- issue metadata is final, `status:done` is applied, and the issue is closed completed.

Green code alone is not Done.

## 12. When blocked

Do not guess.

1. isolate the uncertainty in a minimal test/probe;
2. inspect the pinned dependency/API/source;
3. document the blocker or return `NEEDS_DECISION`;
4. continue another independent Ready ticket when possible.
