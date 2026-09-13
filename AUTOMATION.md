# NativeUI scheduled automation contract

This document defines the execution contract for scheduled GitHub-only automation. It supplements `AGENTS.md`, `CI_POLICY.md`, and `CODE_REVIEW.md`; it never weakens their acceptance, review, test, platform, warning, or merge requirements.

GitHub live state (`main`, issues, pull requests, exact-head checks) is authoritative when a status snapshot in `ROADMAP.md`, `CONTEXT.md`, or the scheduler control plane is temporarily stale.

## 1. Environment boundary

Scheduled workers may run without a repository checkout, shell, Docker, compiler, or arbitrary network access. They must use the GitHub connector and the repository's existing GitHub Actions workflows.

When a local step required by the normal developer workflow cannot be executed in this environment:

- never claim it passed;
- record `local execution unavailable in connector-only scheduled environment` when evidence needs to be stated;
- preserve test-first intent by defining/adding the relevant tests before the implementation change inside the same coherent batch;
- publish tests + implementation + required refactor as one qualification-worthy head rather than publishing an intentional RED head only to obtain remote evidence;
- use GitHub Actions as the first executable validation layer available to the scheduled worker;
- do not merge until all required executable evidence available through the project CI/final gates is actually green.

The connector-only mode changes where execution evidence is obtained, not the Definition of Done.

## 2. Dynamic scheduler and worker pool

NativeUI automation uses a persistent GitHub control plane in issue `#250` (`AUTOMATION — NativeUI scheduler control state`). The scheduler recomputes the live dependency DAG every cycle and writes the current assignment generation there. The control plane is scheduling metadata only and never overrides explicit issue `Dependencies:` or repository policy.

Every scheduler generation also creates one human-readable GitHub issue titled `AUTOMATION CYCLE — GNNN — ...`. The current cycle is open with `Status: Doing` / `status:doing`; the scheduler closes the previous cycle as `Done` / `status:done` before creating the next one. These cycle issues are an audit journal only.

`#250` and any issue containing `<!-- nativeui-orchestrator-cycle -->` or whose title starts with `AUTOMATION CYCLE —` are **control-plane records, not product work items**. They must be excluded from:

- the product dependency DAG;
- product Ready/Doing/Blocked/Done counts;
- backlog and release percentages;
- ticket leases and worker claims;
- source-changing lane utilization;
- work-stealing candidates;
- critical-path calculations;
- stall/handoff detection for implementation work.

At most three source-changing implementation lanes may be active at once, consistent with `AGENTS.md`.

A product ticket/PR waiting only for remote CI, review, audit, final qualification, or merge does **not** consume a source-changing lane. The product issue remains `Doing`, but another dependency-ready independent product ticket may use the freed implementation slot.

Implementation workers form a dynamic pool:

- Worker A affinity: API, styles, paint sources, materials and shaders;
- Worker B affinity: renderer core, platform, accessibility and performance;
- Worker C affinity: convergence, application/release, lighting and showcase work;
- Worker E: general burst/work-stealing worker.

Affinities are scheduling preferences, not exclusive ownership. The scheduler may assign any dependency-ready product ticket to any implementation worker when that improves throughput without creating unsafe overlap.

Supporting workers are specialized:

- Worker A2: audit, completeness, review evidence and closeout analysis only; it never changes source/tests/build/workflow heads;
- Worker D: qualification, final review, status repair, Ready transition, safe synchronization, merge and completion bookkeeping only;
- project reporting: read-only health/progress and scheduler-efficiency reporting.

The scheduler records per generation at least: current `main`, open product PR heads, lane utilization, primary/fallback assignments, A2 target, D queue, Ready queue, CI-wait tickets, critical-path blockers, status inconsistencies, handoffs and stalls.

Workers must re-fetch live GitHub state before acting and may reject a stale assignment. Live GitHub always wins over `#250`.

## 3. Ticket status, lease, claim and handoff

`Doing` is the development lease for **product tickets only**.

### 3.1 Canonical product-status read

Before a product ticket may be assigned, claimed, continued after a scheduler handoff, or used to unlock a dependency, read all three sources together:

1. GitHub issue open/closed state and state reason;
2. the issue body's `## Status` value;
3. every `status:*` label on the issue.

The only coherent product states are:

- `Ready`: issue open + body `Status: Ready` + exactly one `status:ready` label;
- `Doing`: issue open + body `Status: Doing` + exactly one `status:doing` label;
- `Blocked`: issue open + body `Status: Blocked` + exactly one `status:blocked` label;
- `Done`: issue closed as completed + body `Status: Done` + exactly one `status:done` label.

A missing status label, multiple incompatible `status:*` labels, disagreement between body and label, or an issue state inconsistent with the body is `status_incoherent`.

For new work, `status_incoherent` is a scheduling gate: the scheduler must not newly assign/claim that ticket. It records the inconsistency in `#250` and puts it in Worker D's repair queue. Worker D repairs it only when the intended semantic state is unambiguous from explicit dependencies, active PR state and live GitHub evidence.

A control-plane cycle issue may itself use `status:doing` / `status:done`, but that state is never interpreted as a product lease.

### 3.2 Product claim

Before the first source/test/branch write for a new product ticket, a worker must:

1. re-fetch the issue and current `main`;
2. check for an existing branch/PR or other active owner;
3. verify every explicit `Dependencies:` item is Done;
4. verify the product status triplet is coherent and currently `Ready`;
5. transition the issue to `Doing` / `status:doing` and remove incompatible status labels;
6. re-fetch before writing and abort the claim if concurrent work appeared or status became incoherent.

Do not use `Blocked` merely because a lower-numbered ticket remains open.

### 3.3 Handoff

A scheduler handoff must preserve the same canonical PR. Reassignment is allowed only when the current owner is genuinely stalled: no new head or other useful progress across at least two scheduler generations, no exact-head CI queued/in-progress, and no explicit blocker explaining the wait. A handoff must be recorded in `#250`; no worker may create a competing PR.

Body `## Status`, labels, issue state, PR state, `ROADMAP.md`, and `CONTEXT.md` must be reconciled when a transition is unambiguous. Multiple incompatible `status:*` labels are invalid.

## 4. Scheduler cycle journal

Each scheduler run creates exactly one orchestration-cycle issue after computing the new plan.

Cycle order:

1. read `current_cycle_issue` from `#250`;
2. if that issue is still open and contains `<!-- nativeui-orchestrator-cycle -->`, transition it to `Done` / `status:done` and close it as completed;
3. increment the scheduler generation;
4. recompute the complete live product DAG, CI backpressure, worker capacity and assignments;
5. create a new `AUTOMATION CYCLE — GNNN — YYYY-MM-DD HH:MM TZ` issue using the canonical issue-form sections;
6. set the new cycle issue to `Doing` / `status:doing`;
7. write the same generation and the new `current_cycle_issue` number into `#250`;
8. re-fetch `#250` and the current cycle issue before finishing the cycle.

The cycle issue must show at least:

- generation and timestamp;
- observed `main` SHA;
- source-changing lanes used out of three;
- Worker A/B/C/E modes, primary tickets, fallbacks and reasons;
- Worker A2 target;
- Worker D queue;
- product Ready queue;
- CI-wait product tickets;
- `status_incoherent` product tickets;
- critical-path blockers;
- handoffs/stalls;
- the principal scheduling change from the previous generation.

The cycle issue is deliberately visible to humans on GitHub. It is not part of product scope, does not require product code review, does not update `ROADMAP.md`, and must never be selected by implementation workers.

## 5. Scheduling priority and work stealing

The scheduler first enforces explicit dependencies and coherent product status, then ranks dependency-ready work by:

1. priority (`P0 > P1 > P2`);
2. critical-path and downstream-unblock impact;
3. number/value of dependants unlocked;
4. proximity to a release or merge;
5. expected effort and conflict risk;
6. worker affinity as a tie-breaker.

Each implementation worker may receive a `primary` and, when safe, a `fallback`. A fallback may be used only while the primary waits solely on CI/review/final gates or when the control plane explicitly authorizes the switch.

Worker E provides burst capacity after merges and other unlock events. If the current control-plane generation becomes stale because live GitHub has just changed, a worker may perform limited work stealing only after re-fetching `#250`, confirming a dependency-ready unassigned product ticket with coherent status, and ensuring the global maximum of three source-changing lanes will not be exceeded.

If no legal Ready product work exists, standby/audit is correct. Workers must never invent scope just to fill a lane.

## 6. Batch-first TDD and publication

Scheduled workers follow the batch-first policy in `AGENTS.md` and `CI_POLICY.md`.

For one bounded acceptance slice:

1. audit the complete relevant surface before editing;
2. collect all compatible gaps/findings;
3. define/add tests for the complete batch;
4. implement the complete bounded behavior;
5. refactor within scope;
6. publish one coherent qualification head;
7. let the exact head qualify remotely;
8. consolidate compatible review/CI corrections into one further batch when necessary.

Do not create one published commit per assertion, widget state, helper, RED/GREEN step, review finding, or CI symptom.

## 7. Exact-head CI backpressure

If a product PR's current exact head has relevant workflows queued or in progress:

- do not push another correction to that PR;
- do not create a no-op commit to retrigger CI;
- mark the primary as CI-wait in the control plane;
- use the freed implementation slot on an assigned independent fallback when available;
- otherwise use the wait for audit/review/evidence;
- inspect complete failing evidence before publishing a correction batch.

For infrastructure/transient failures on an unchanged candidate, prefer supported targeted job/failed-job reruns over manufacturing a new Git head.

Heavy T042/T052-style final qualification remains governed by `CI_POLICY.md` and is not an inner-loop validation mechanism.

## 8. Audit, qualification and merge pipeline

Worker A2 audits the product PR selected by the scheduler as closest to freeze/merge. It verifies acceptance coverage, required tests, non-goals, completeness matrix, examples/self-tests, exact-head evidence and `CODE_REVIEW.md` concerns. Findings must be consolidated by severity rather than emitted as micro-review churn. A2 may emit a `PASS` verdict only for the exact head it inspected and never edits the executable candidate.

Worker D is the normal transition owner for product closeout and status repair:

1. repair unambiguous `status_incoherent` product tickets before they are newly assigned;
2. confirm implementation is frozen and normal/path-scoped CI is green;
3. perform/verify the final `CODE_REVIEW.md` review, using A2 evidence where applicable;
4. refuse Ready/merge while any Blocking/Important finding remains;
5. move Draft -> Ready only when `CI_POLICY.md` preconditions are satisfied;
6. wait for all applicable final-candidate gates;
7. re-fetch `main`, issue, PR and exact head immediately before merge;
8. merge only the exact expected head when mergeable and fully conformant;
9. transition the product issue to Done/closed and reconcile dependent Ready product tickets.

Worker D must never manage or close scheduler-cycle issues; the scheduler owns their lifecycle.

A project-state documentation-only change to `main` does not automatically invalidate an executable candidate when `CI_POLICY.md` says it does not alter the executable contract. Any branch synchronization that changes the PR head still requires the checks applicable to that new head.

## 9. Release gates and continuation

T071 and T121 are validation-only release gates. Runtime/API/build behavior fixes discovered during a release gate belong in a focused fix ticket/PR, followed by a new exact release-candidate SHA.

Completing v1.0 or v1.1 does not stop scheduled automation. The scheduler continues assigning dependency-ready planned or future product backlog until no planned work remains.

## 10. Reporting and efficiency measurement

Every implementation/support worker run emits a compact report with action/assignment, actual changes, validation observed, blocker and exact next action.

The separate hourly project report is read-only and covers progress, product PR/CI detail, critical path, backlog continuation, status/document coherence, branch-protection/ruleset state when observable, scheduler generations, cycle journal health, worker health, lane utilization, work stealing, handoffs/stalls and automation risks.

Product metrics must exclude `#250` and orchestration-cycle issues. Scheduler/control-plane activity is reported separately and must never inflate product progress.

Scheduler efficiency should be measured using observable data such as source-lane utilization, CI-finish-to-next-action latency, merge-to-claim latency, substantive heads/merges per hour, avoidable idle workers, collisions avoided, stale assignments corrected and status inconsistencies repaired.

Do not claim that an operating-system push/email notification was delivered unless delivery is actually observable. Chat/task output and platform notification delivery are separate concerns.

## 11. Concurrency safety

Immediately before every repository write, re-fetch the canonical product issue/PR/branch head. If it changed since inspection, do not overwrite, force-update, or race concurrent work. Re-evaluate the new head or switch to another safe task.

No scheduled worker may force-push or discard concurrent progress.

## 12. Server-side safety

Prompt-level merge rules are not a substitute for GitHub server-side branch protection. The project should enforce a `main` branch ruleset/branch protection with appropriate required checks and force-push/deletion protections when repository administration permits it.

Until such protection is present, workers must treat the merge gates in `AGENTS.md`, `CI_POLICY.md`, `CODE_REVIEW.md`, and this document as mandatory and use exact-head merge safeguards.