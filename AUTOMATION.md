# NativeUI development automation

**Status:** deterministic / serialized delivery model

This document is the source of truth for automated NativeUI development orchestration. It complements `AGENTS.md`, `CODE_REVIEW.md` and `CI_POLICY.md`; it never weakens their implementation, review, validation, privacy or merge requirements.

The former autonomous multi-worker Scheduler / Reporter control plane is retired. Automation must derive state from live GitHub issues, pull requests, reviews and checks instead of maintaining a second orchestration state machine.

## 1. Core principle

GitHub is the only durable source of truth.

The canonical delivery chain is:

```text
Issue
  -> implementation branch / Draft PR
  -> local TDD + coherent implementation batch
  -> exact-head CI qualification
  -> independent code review
  -> coherent review-fix batch when required
  -> exact-head requalification
  -> merge when green
  -> issue / CONTEXT.md / ROADMAP.md completion
```

No Scheduler generation, Reporter snapshot, worker event stream, reservation table or control issue may override live GitHub state.

## 2. Serialized execution

Automated development uses **one implementation lane at a time** until this model has demonstrated stable convergence on real tickets.

Rules:

1. Select one eligible ticket from GitHub.
2. Keep that ticket as the only source-changing automation assignment until it reaches a durable terminal point: merged, explicitly blocked, or deliberately parked by a documented planning decision.
3. Do not open fallback implementation work merely to keep workers busy.
4. CI waiting does not automatically authorize another source-changing ticket. Parallel source work is an explicit exception, not the default.
5. Resume an existing canonical PR before creating a replacement branch.
6. Keep the PR Draft while source/tests/build are changing.
7. Publish coherent batches; do not reproduce local RED/GREEN steps as remote micro-commits.

The objective is completed, reviewed, mergeable work — not worker occupancy.

## 3. Ticket selection

Use live issue metadata and the rules in `AGENTS.md`.

For automation, apply this order:

1. finish a current merge-near ticket before starting a new one;
2. otherwise choose the highest-priority `Ready` ticket whose explicit dependencies are Done;
3. among equal priorities, prefer the ticket with the largest downstream unblock value;
4. prefer an existing valid PR over starting equivalent work again;
5. if the selected ticket becomes genuinely blocked, record the blocker in GitHub and choose the next eligible ticket.

Do not infer dependencies from ticket number, stale automation issues, worker assignments or historical Scheduler generations.

## 4. Implementation worker

The implementation worker owns one ticket at a time.

Before writing:

- read `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `CONTEXT.md`, `ROADMAP.md` and the complete live issue;
- re-fetch the PR head/base and current `main`;
- verify the issue is still eligible and no newer merge/review changed the plan;
- build the required issue-to-code/test completeness matrix.

During implementation:

- use local TDD and deterministic failure seams where required;
- make one coherent bounded correction rather than a sequence of symptom patches;
- preserve multi-instance safety, exception/recovery behavior and privacy requirements;
- never put personal information in tickets, code, tests, examples, fixtures or generated metadata;
- run the relevant local tests before publishing a qualification head.

At source completion:

- synchronize with current `main` only when composition requires it;
- run the required exact-head checks;
- complete the self-review record required by `CODE_REVIEW.md`;
- hand the frozen exact head to independent review.

## 5. Independent review worker

Review is a distinct phase and must be independent from the worker that produced the exact head being reviewed.

The reviewer:

1. re-fetches the exact PR head, base, diff, issue, checks and existing review threads;
2. reviews the complete bounded ticket scope against `CODE_REVIEW.md` rather than only the latest patch;
3. records all current Blocking / Important findings in one pass;
4. may apply a bounded, unambiguous correction directly when doing so reduces latency;
5. if the reviewer modifies the head, that reviewer cannot provide the final approval for the replacement head;
6. a different independent reviewer must review the replacement exact head.

A review is valid only for the exact head it inspected. Any executable source/test/build/workflow change invalidates prior head-scoped approval and qualification evidence.

## 6. Merge gate

A PR may merge only when all applicable conditions are true:

- issue acceptance criteria are satisfied;
- required local and remote tests are green for the exact candidate head;
- mandatory `CODE_REVIEW.md` review is complete;
- there are no unresolved Blocking / Important findings;
- required independent approval targets the exact current head;
- the PR is composition-compatible with current `main`;
- no unresolved review thread blocks completion;
- `CONTEXT.md` and `ROADMAP.md` completion bookkeeping is included as required by `AGENTS.md`;
- privacy review confirms no personal information was introduced.

Merge immediately once the complete gate is green. Do not wait for a Scheduler tick or Reporter cycle.

## 7. Failure and recovery

Automation must be restartable from GitHub alone.

If an agent/process stops at any point, the next run reconstructs state from:

- issue state, labels, milestone and dependencies;
- open PRs and exact heads;
- CI/check results;
- submitted reviews and unresolved review threads;
- `CONTEXT.md` and `ROADMAP.md` only as compact project context, never as a competing control plane.

There is no persistent worker reservation, generation counter, scheduler database or reporter-owned lifecycle state.

When repeated review/CI cycles expose the same failure family, stop patching symptoms. Re-audit the full invariant/failure family and produce one consolidated correction batch.

## 8. Reporting

Reporting is **read-only and stateless**.

A report may summarize live GitHub state, but it must not own assignments, transitions or completion decisions. Every report is reconstructed from current GitHub data.

Useful fields:

- current implementation ticket and PR;
- exact head and CI state;
- current review state and unresolved findings;
- explicit blockers;
- next eligible ticket after the current one reaches a terminal point;
- issue-to-merge elapsed time and number of review/fix cycles.

Do not report worker occupancy as progress.

## 9. Watchdog

A watchdog, if enabled, is read-only and condition-based. It may flag:

- a `Doing` ticket with no active PR or documented work;
- a PR with no meaningful progress for a configured period;
- a frozen green head missing independent review;
- a review-approved head with failed/stale CI;
- an unresolved Blocking / Important review finding;
- a merged PR whose issue / `CONTEXT.md` / `ROADMAP.md` bookkeeping is incomplete.

The watchdog does not create implementation assignments or mutate product state automatically.

## 10. Parallelism policy

The current automation limit is:

```text
implementation/source-changing lanes: 1
independent review: on demand for the frozen candidate
reporting/watchdog: read-only only
scheduler: disabled
persistent reporter state: disabled
fallback work: disabled
```

Review activity may happen while the implementation lane is frozen for review, but no second source-changing ticket starts automatically.

Parallel implementation may be reintroduced only after a deliberate documentation change backed by measured evidence that serialized delivery is stable and that added concurrency improves merge throughput without increasing stale-head, rebase, review or CI churn.

## 11. Pilot and success criteria

Validate this model across the next 5–10 real tickets before increasing autonomy.

Track:

- issue selected -> first coherent PR head;
- first coherent head -> independent review;
- review -> merge;
- total issue -> merge time;
- qualification heads per ticket;
- review-fix cycles per ticket;
- stale-head/rebase invalidations;
- number of merges requiring manual orchestration recovery.

The model is successful when tickets reliably reach reviewed green merge states with fewer repeated heads and less coordination overhead than the retired multi-worker scheduler.

## 12. Retired control-plane artifacts

The following concepts are historical and must not be used for current assignments:

- Scheduler generations and cycle issues;
- W1/W2/W3/W4 lane occupancy;
- worker reservation / fallback events;
- Reporter-owned state;
- `#250` scheduler snapshots;
- `AUTOMATION CYCLE` issues;
- mandatory T122 idle-worker fallback.

`AUTOMATION_FLOW_OVERRIDE.md` and `AUTOMATION_CAPACITY_OVERRIDE.md` are retained only as retirement notices pointing back to this document.

## 13. Current delivery plan

The live queue is maintained in `AUTOMATION_STATUS.md`. That file is a human-readable plan derived from GitHub, not an orchestration database. If it conflicts with live GitHub state, GitHub wins.