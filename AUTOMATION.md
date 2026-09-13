# NativeUI scheduled automation contract

This document defines the execution contract for scheduled GitHub-only automation. It supplements `AGENTS.md`, `CI_POLICY.md`, and `CODE_REVIEW.md`; it never weakens their acceptance, review, test, platform, warning, or merge requirements.

GitHub live state (`main`, issues, pull requests, exact-head checks) is authoritative when a status snapshot in `ROADMAP.md` or `CONTEXT.md` is temporarily stale.

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

## 2. Parallelism and ownership

At most three source-changing implementation lanes may be active at once, consistent with `AGENTS.md`.

A ticket/PR waiting only for remote CI, external review, or a final qualification gate does **not** consume an implementation lane. The issue remains `Doing`, but another dependency-ready independent ticket may use the freed implementation slot.

A lane is considered source-changing active only while it currently requires production/test/refactor/review-fix writes.

Current scheduled roles are intentionally staggered:

- Worker A: API/styles/material-source chain;
- Worker B: accessibility/renderer-core chain;
- Worker C: convergence/release/lighting/showcase chain;
- Worker D: qualification, final review, Ready transition, synchronization, merge and completion bookkeeping only;
- Worker E: dynamic spare/work-stealing when fewer than three source-changing lanes are active;
- project reporting: read-only health/progress reporting.

Domain assignments are ownership defaults, not permission to violate live `Dependencies:`. A worker must never create a competing PR for a ticket already owned by another worker.

## 3. Ticket lease and status state machine

`Doing` is the development lease.

Before the first source/test/branch write for a new ticket, a worker must:

1. re-fetch the issue and current `main`;
2. check for an existing branch/PR or other active owner;
3. verify every explicit `Dependencies:` item is Done;
4. transition the issue to `Doing` / `status:doing` and remove incompatible status labels;
5. re-fetch before writing and abort the claim if concurrent work appeared.

Status semantics:

- `Ready` / `status:ready`: all explicit dependencies Done and no real external blocker;
- `Doing` / `status:doing`: implementation, review, synchronization, CI/final qualification, including ordinary CI waiting;
- `Blocked` / `status:blocked`: an explicit dependency is not Done or a genuine external blocker prevents useful progress;
- `Done` / `status:done`: only after merge/completion bookkeeping is actually complete.

Body `## Status`, labels, issue state, PR state, `ROADMAP.md`, and `CONTEXT.md` must be reconciled when a transition is unambiguous. Multiple incompatible `status:*` labels are invalid.

## 4. Batch-first TDD and publication

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

## 5. Exact-head CI backpressure

If a PR's current exact head has relevant workflows queued or in progress:

- do not push another small correction to that PR;
- do not create a no-op commit to retrigger CI;
- use the wait for audit/review/evidence or another independent Ready ticket when an implementation slot is free;
- inspect the complete failing evidence before publishing a correction batch.

For infrastructure/transient failures on an unchanged candidate, prefer supported targeted job/failed-job reruns over manufacturing a new Git head.

Heavy T042/T052-style final qualification remains governed by `CI_POLICY.md` and is not an inner-loop validation mechanism.

## 6. Qualification and merge

Worker D is the normal transition owner for closeout:

1. confirm the implementation is frozen and normal/path-scoped CI is green;
2. perform/verify the final `CODE_REVIEW.md` review;
3. refuse Ready/merge while any Blocking/Important finding remains;
4. move Draft -> Ready only when `CI_POLICY.md` preconditions are satisfied;
5. wait for all applicable final-candidate gates;
6. re-fetch `main`, issue, PR and exact head immediately before merge;
7. merge only the exact expected head when mergeable and fully conformant;
8. transition the issue to Done/closed and reconcile dependent Ready tickets.

A project-state documentation-only change to `main` does not automatically invalidate an executable candidate when `CI_POLICY.md` says it does not alter the executable contract. Any branch synchronization that changes the PR head still requires the checks applicable to that new head.

## 7. Release gates

T071 and T121 are validation-only release gates. Runtime/API/build behavior fixes discovered during a release gate belong in a focused fix ticket/PR, followed by a new exact release-candidate SHA.

Completing v1.0 or v1.1 does not stop scheduled automation. Workers continue with the next dependency-ready planned backlog until no planned work remains.

## 8. Reporting and user visibility

Every Worker A-E run emits a compact report, including when no write occurred:

- action/ticket/PR/current status;
- changes or `no change`;
- validation/check state actually observed;
- real blocker or `none`;
- exact next action.

The separate hourly project report is read-only and covers progress, PR/CI detail, critical path, backlog continuation, status/document coherence, branch-protection/ruleset state when observable, worker health, lane utilization, and automation risks.

Do not claim that an operating-system push/email notification was delivered unless delivery is actually observable. Chat/task output and platform notification delivery are separate concerns.

## 9. Concurrency safety

Immediately before every repository write, re-fetch the canonical issue/PR/branch head. If it changed since inspection, do not overwrite, force-update, or race concurrent work. Re-evaluate the new head or switch to another safe task.

No scheduled worker may force-push or discard concurrent progress.

## 10. Server-side safety

Prompt-level merge rules are not a substitute for GitHub server-side branch protection. The project should enforce a `main` branch ruleset/branch protection with appropriate required checks and force-push/deletion protections when repository administration permits it.

Until such protection is present, workers must treat the merge gates in `AGENTS.md`, `CI_POLICY.md`, `CODE_REVIEW.md`, and this document as mandatory and use exact-head merge safeguards.