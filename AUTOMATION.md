# NativeUI scheduled automation contract

This document defines the execution contract for autonomous GitHub-only NativeUI delivery. It supplements `AGENTS.md`, `CI_POLICY.md`, `CODE_REVIEW.md`, `AUTOMATION_STATUS.md`, `AUTOMATION_CODE_REVIEW_GATE.md`, and `AUTOMATION_CAPACITY_OVERRIDE.md`; it never weakens product acceptance, review, test, warning, platform, privacy, or merge requirements.

GitHub live state is authoritative. `ROADMAP.md`, `CONTEXT.md`, scheduler snapshots, task output, and worker events are recovery/coordination data and must never override live issues, pull requests, exact heads, explicit dependencies, executed checks, or durable review evidence.

`AUTOMATION_STATUS.md` is the canonical state-machine and status vocabulary. When this file uses a phase/event name, its exact semantics come from that document.

## 1. Objective

The automation exists to finish the planned NativeUI backlog with maximum safe throughput and minimal human intervention.

It must:

- make scheduling decisions autonomously from the complete live project graph;
- keep at most four simultaneous product source-changing lanes;
- reserve worker capacity for mandatory peer review instead of starving review behind development;
- preserve one canonical issue/branch/PR stream per product ticket;
- use coherent batch-first TDD and exact-head executable evidence;
- perform independent peer review before final qualification and merge;
- permit bounded reviewer-owned corrections without permitting self-certification of the corrected head;
- merge autonomously only when the full merge invariant is proven from live GitHub evidence;
- continue after release gates until the planned backlog is exhausted;
- maintain durable GitHub-to-GitHub communication between workers;
- provide separate read-only project reporting.

## 2. Environment boundary

Scheduled workers may run without a repository checkout, shell, Docker, compiler, or arbitrary network access. In that case they use the GitHub connector and existing GitHub Actions.

When local execution required by the normal developer workflow is unavailable:

- never claim it passed;
- record `local execution unavailable in connector-only scheduled environment` where relevant;
- preserve TDD intent in the coherent batch;
- do not publish intentional RED heads merely to use GitHub Actions as the inner development loop;
- publish tests + implementation + required refactor as one qualification-worthy head;
- use GitHub Actions as the first executable validation layer available to the scheduled worker.

The environment changes where evidence is obtained, not the Definition of Done.

## 3. Operating model

Exactly seven NativeUI automation roles are active:

1. **Scheduler** — decision engine and control-plane owner; never changes product source/tests and never merges.
2. **Delivery W1** — interchangeable end-to-end implementation/review worker.
3. **Delivery W2** — interchangeable end-to-end implementation/review worker.
4. **Delivery W3** — interchangeable end-to-end implementation/review worker.
5. **Delivery W4** — interchangeable end-to-end implementation/review worker.
6. **Integration** — final closeout/qualification/merge worker; does not implement product features.
7. **Reporter** — read-only project/orchestration reporting.

Delivery affinities are tie-breakers only. No ticket family is permanently owned.

Maximum simultaneous product source-changing lanes: **4**.

Review, CI wait, preflight and final qualification do not consume a source lane. `REVIEW_FIX` **does** consume a source lane because it changes source/tests/build behavior.

When a mandatory peer review is ready and all Delivery workers would otherwise source-edit, the effective schedule becomes **3 source lanes + 1 reviewer**. If that reviewer enters `REVIEW_FIX`, it becomes **3 source lanes + 1 review-fix source lane**, still totaling four source-changing lanes.

## 4. GitHub blackboard

GitHub is the shared memory for every automation role.

### 4.1 Product issues

Product issues define scope, non-goals, priority, explicit dependencies, acceptance criteria, required tests and the product status/lease.

Product status is intentionally small: `Ready`, `Doing`, `Blocked`, `Done` only. See `AUTOMATION_STATUS.md`.

Do not create product status labels for CI/review/merge phases.

### 4.2 Product pull requests

PRs and exact-head Actions define:

- canonical implementation stream;
- current exact head and observed base composition;
- executable evidence;
- durable self-review/peer-review evidence;
- review threads/findings;
- mergeability.

A PR phase such as `SOURCE_READY`, `PEER_CODE_REVIEW`, `REVIEW_FIX`, `FINAL_QUALIFICATION` or `MERGE_READY` is control-plane state, not product issue status.

### 4.3 `#250` scheduler control state

Issue `#250` is the compact machine snapshot written **only by the Scheduler**. Delivery, Integration and Reporter never modify it.

It records at least:

- committed generation/current cycle;
- main SHA;
- W1/W2/W3/W4 primary/mode/secondary assignments;
- source-changing lane count;
- per-PR phase;
- `source_worker`;
- `review_lock_worker` when review is active;
- `review_fix_worker` when applicable;
- current `peer_reviewer`;
- `reviewed_head`;
- CI waits;
- Integration queue;
- Ready queue;
- critical path;
- status inconsistencies;
- stalls/handoffs;
- conditional `on_unlock` reservations.

### 4.4 Current cycle issue

Each committed generation has exactly one current `AUTOMATION CYCLE — GNNN — ...` issue.

Its body is the human-readable plan. Its comments are the Worker -> Scheduler event bus.

`#250` and cycle issues are control-plane only and are excluded from product backlog/status percentages/dependencies/leases/source-lane utilization/release readiness.

## 5. Worker event protocol

Meaningful worker transitions are comments on the current cycle issue referenced by `#250`.

Every event starts with:

```text
<!-- nativeui-worker-event:v1 -->
```

and contains at least:

```yaml
generation: <current committed generation>
assignment_generation: <assignment generation|null>
worker: W1|W2|W3|W4|Integration
kind: <event kind>
ticket: TNNN|null
issue: <number|null>
pr: <number|null>
head: <exact SHA|null>
base: <observed target/base SHA|null>
result: <concise factual result>
next_action: <concrete next action>
```

Canonical event kinds are exactly the result vocabulary from `AUTOMATION_STATUS.md`:

- `CLAIMED`
- `PROGRESS`
- `PREFLIGHT_READY`
- `CI_WAIT`
- `CI_FAILED_PRODUCT`
- `CI_FAILED_INFRA`
- `SOURCE_READY`
- `REVIEW_PASS`
- `REVIEW_FIX_REQUIRED`
- `REVIEW_FIX_APPLIED`
- `REVIEW_BLOCKED`
- `REWORK_REQUIRED`
- `QUALIFICATION_WAIT`
- `QUALIFICATION_FAILED_PRODUCT`
- `QUALIFICATION_FAILED_INFRA`
- `MERGE_READY`
- `MERGED`
- `BLOCKED`
- `HANDOFF_REQUEST`

Assignment modes such as `PEER_CODE_REVIEW`, `REVIEW_FIX`, and `SECOND_PEER_CODE_REVIEW` are Scheduler modes, not event kinds.

Rules:

- emit only meaningful state/head/phase changes; no heartbeat spam;
- PR/review/qualification events include exact head SHA;
- review/qualification/merge events include observed base SHA when composition matters;
- if live head differs, the event is stale;
- `REVIEW_PASS` is valid only with a durable head-valid PR review containing `<!-- nativeui-peer-code-review:v1 -->`;
- `REVIEW_FIX_APPLIED` records old head, new head, review-fix worker and corrected findings;
- detailed code findings belong in the PR/review, not only in cycle events;
- `MERGED` must be verified against live GitHub before any dependent claim.

### 5.1 Generation race rule

Immediately before posting any worker event, re-fetch `#250` and its current cycle.

If the generation changed while the worker was running:

1. revalidate live issue/PR/head and the new generation assignment/reservation;
2. if still relevant, post to the new current cycle using the new generation and preserving `assignment_generation`;
3. if the new plan invalidates the scheduling consequence, keep technical findings durable in the PR/issue but do not emit misleading stale control-plane state;
4. never post a new coordination event only to a superseded cycle.

## 6. Product status and leases

Coherent product states are:

- Ready = open + body Ready + exactly `status:ready`;
- Doing = open + body Doing + exactly `status:doing`;
- Blocked = open + body Blocked + exactly `status:blocked`;
- Done = closed/completed + body Done + exactly `status:done`.

A closed `not_planned`/duplicate issue is terminal non-delivery and does not satisfy a dependency unless explicitly removed/replaced.

Any disagreement is `status_incoherent`; a new claim is forbidden until the inconsistency is repaired or explained.

`Doing` remains the product lease through implementation, CI, review, review-fix, final qualification and merge closeout.

## 7. Scheduler

Every cycle, inspect the whole relevant project:

- policy docs including `AUTOMATION_STATUS.md` and `AUTOMATION_CODE_REVIEW_GATE.md`;
- `#250` and current cycle events;
- open product issues/dependencies/priorities/status;
- open PRs/exact heads/reviews/threads/mergeability;
- exact-head workflows/checks;
- recently merged work needed to recompute unlocks.

Then:

1. validate worker events against live state;
2. rebuild the dependency DAG/frontier;
3. classify each Doing ticket/PR using canonical phases from `AUTOMATION_STATUS.md`;
4. enforce any active review lock before assigning source work;
5. reserve mandatory review capacity before opening new source/fallback work;
6. compute available source capacity out of four;
7. rank Ready/source work by priority, critical-path/unlock value, proximity to merge/release, effort and conflict risk;
8. assign each Delivery worker `primary`, `mode`, `secondary`, with objective reason;
9. assign Integration an ordered closeout/qualification/merge queue;
10. publish safe `on_unlock` reservations only after accounting for mandatory review capacity;
11. record status inconsistencies, stalls and handoffs.

### 7.1 Mandatory review service rule

After a head-valid code-changing `SOURCE_READY`:

1. assign `PEER_CODE_REVIEW` as primary to a worker different from the current-head author in the same Scheduler generation;
2. prefer a worker already in CI/review/final wait;
3. if all workers source-edit, reserve one by allowing it to finish only its current coherent batch, then review before starting another batch;
4. while mandatory review is unassigned/unstarted, do not open a new source fallback/lane;
5. lack of reviewer capacity is an orchestration defect, not a valid delay.

Target service level: `SOURCE_READY -> review assignment` in the same Scheduler cycle, then execution on that reviewer's next run.

### 7.2 Review lock

Entering `PEER_CODE_REVIEW` creates a control-plane review lock on that PR.

- source worker must not push while review lock is active;
- reviewer is read-only unless explicitly in `REVIEW_FIX` mode;
- `REVIEW_FIX` keeps the lock and allows only that worker to source-edit the PR for one bounded correction batch;
- `REVIEW_FIX` counts as one source-changing lane;
- after `REVIEW_FIX_APPLIED`, the PR remains frozen and is scheduled for `SECOND_PEER_CODE_REVIEW` by another worker;
- `REVIEW_BLOCKED` releases source ownership only when Scheduler routes `REWORK_REQUIRED` to the source worker;
- `REVIEW_PASS` keeps ordinary source edits frozen and hands control to Integration.

A source write that races an active review lock is a P0 orchestration defect and invalidates downstream head-scoped review state.

### 7.3 Reviewer-owned correction

If peer review finds a bounded, unambiguous defect that satisfies `AUTOMATION_CODE_REVIEW_GATE.md`, Scheduler keeps the same reviewer in `REVIEW_FIX` instead of round-tripping to the original source worker.

The reviewer may add regression tests and one coherent correction batch. It then emits `REVIEW_FIX_APPLIED` on the new head.

The review-fix author can never emit final `REVIEW_PASS` for the head it modified. Scheduler assigns `SECOND_PEER_CODE_REVIEW` to a different worker before any final qualification.

If the defect requires a product/API decision, large redesign, scope expansion, broad ambiguous refactor or concurrent conflict, use `REVIEW_BLOCKED` / `REWORK_REQUIRED` instead.

### 7.4 Work-conserving priority

When no mandatory review is waiting, useful secondary work priority is:

1. independent peer review;
2. CI failure diagnosis;
3. substantive preflight;
4. non-conflicting T122 documentation fallback;
5. standby only when nothing useful/legal exists.

A run that merely reports unchanged state is not productive.

### 7.5 Stall/handoff

Absence of a commit alone is not a stall. CI execution, durable review/preflight evidence or explicit blocker count as progress.

Source handoff is allowed only after at least two committed generations with no useful progress, no relevant CI running, no explicit blocker and a live re-check. Preserve the canonical PR.

## 8. Scheduler transaction and crash recovery

For generation `N -> N+1`:

1. read/validate generation N events and live state;
2. compute complete N+1 plan;
3. create N+1 cycle issue first;
4. update `#250` to point to N+1 and record full assignment/review-lock state;
5. re-fetch and verify `#250` + N+1 issue;
6. only then mark N cycle Done/status:done and close completed.

If multiple cycles are open after a crash, `#250.current_cycle_issue` is the committed cycle. Adopt a newer orphan only after revalidation; otherwise close it as superseded.

## 9. Delivery W1/W2/W3/W4

All Delivery workers are interchangeable.

At every run:

1. read policy/status/review docs, #250, current cycle body/comments, live product issue/PR/head/CI/reviews/threads;
2. reject stale/conflicting assignments;
3. respect review lock before any source write;
4. follow assigned mode:
   - `SOURCE`: implementation/TDD batch;
   - `PEER_CODE_REVIEW`: read-only exact-head review;
   - `REVIEW_FIX`: one bounded reviewer-owned correction batch;
   - `SECOND_PEER_CODE_REVIEW`: independent exact-head review of a review-fix head;
   - CI diagnosis/preflight/docs fallback as assigned;
5. obey exact-head backpressure;
6. max one new qualification head per PR per run;
7. before every repository write re-fetch issue/PR/head/main/review-lock state;
8. never force-push or discard concurrent work;
9. apply the generation-race rule before every worker event.

### 9.1 SOURCE mode

Use batch-first TDD and the full ticket completeness matrix. Before `SOURCE_READY`, perform the implementation worker's complete self-review against `CODE_REVIEW.md` and ensure normal/path qualification is sufficient for peer review.

Self-review is not peer review.

### 9.2 PEER_CODE_REVIEW / SECOND_PEER_CODE_REVIEW

Inspect full exact diff and relevant surrounding code, acceptance/tests/non-goals, current review threads and applicable CI.

Submit a formal GitHub PR review starting with:

```text
<!-- nativeui-peer-code-review:v1 -->
```

and the structured record required by `AUTOMATION_CODE_REVIEW_GATE.md` / `CODE_REVIEW.md`.

A review cannot PASS a head authored/modified by that reviewer.

### 9.3 REVIEW_FIX

Only when explicitly assigned and only for bounded defects allowed by `AUTOMATION_CODE_REVIEW_GATE.md`:

- keep the review lock;
- add/strengthen a deterministic regression test first when behavior changes;
- apply one coherent correction batch;
- re-fetch before every write;
- publish one new head;
- emit `REVIEW_FIX_APPLIED`;
- never certify that new head;
- wait for CI and another worker's second peer review.

This is the only exception to the ordinary rule that a reviewer does not source-fix another worker's PR without a handoff.

## 10. Integration

Integration is productive but does not implement product features.

Ordered responsibilities:

1. service highest-impact closeout first;
2. classify exact-head CI failures as product vs infrastructure;
3. verify mandatory head-valid peer review and current-head authorship independence;
4. perform an additional final Integration verification against `CODE_REVIEW.md`, acceptance/tests/non-goals;
5. if product rework is needed, record durable findings and route back to review/source flow; do not hide feature implementation in Integration;
6. use safe mechanical main synchronization only when it changes no intended behavior; changed head invalidates review and requires a fresh peer review;
7. transition Draft -> Ready only after current-head `REVIEW_PASS` and CI_POLICY preconditions;
8. verify heavyweight final gates and observed base composition;
9. emit `MERGE_READY` only when every merge invariant term is proven;
10. merge only exact expected head;
11. transition product issue Done/closed and synchronize required project-state docs/unlocks;
12. emit verified `MERGED`.

Integration verification does not replace peer review.

## 11. In-cycle reaction

Workers may react without waiting for the next Scheduler only when the action is already authorized by current control state:

- source worker may resume after head-valid `REWORK_REQUIRED` explicitly routes ownership back;
- reviewer may enter `REVIEW_FIX` only when current assignment/event authorizes it;
- a reserved `on_unlock` may be used only after verifying live `MERGED`, dependency/status coherence and lane capacity;
- no in-cycle reaction may violate review lock or the four-source-lane cap.

## 12. CI/backpressure

Remote CI is qualification, not the inner TDD loop.

If exact-head relevant workflows are queued/in-progress:

- do not supersede them with a new small source correction;
- use independent assigned work/review/preflight instead;
- inspect all completed failures before producing a correction batch.

`startup_failure`, zero-job/no-step failures and equivalent non-executed results are infrastructure/unqualified evidence, never product green or product red.

A successful workflow is interpreted with its tested head/base composition. Executable/API/build changes on main may require requalification according to `CI_POLICY.md`.

T042/T052 remain final-candidate gates according to `CI_POLICY.md`.

## 13. Release gates and continuation

T071/T121 are validation-only release gates. Product/API fixes discovered there belong in focused product work followed by a new exact RC.

Completing v1.0/v1.1 does not stop automation. Continue through the dependency-ready planned backlog until no planned work remains.

## 14. Reporting

Reporter is strictly read-only.

Hourly reporting must distinguish:

- product issue status;
- PR phase;
- self-review;
- peer-review / reviewed head;
- review-fix author/current-head author;
- Integration verification;
- exact-head CI/final qualification;
- merge readiness.

It must report W1/W2/W3/W4/Integration last useful actions, source utilization/4, mandatory review waits, review-lock violations, stale events/reviews, SOURCE_READY->review latency, review-fix->second-review latency, CI-finish->action latency, merge->dependent-claim latency, collisions/stalls/handoffs and status inconsistencies.

Control-plane activity never inflates product progress.

## 15. Merge invariant

For every code-changing PR:

```text
MERGE_ALLOWED =
    current_head_is_frozen
 && exact_head_required_ci_green
 && peer_review_exact_head_pass
 && reviewer_did_not_author_current_head
 && blocking_findings == 0
 && important_findings == 0
 && acceptance_complete
 && required_tests_complete
 && final_qualification_complete
 && base_composition_valid
 && mergeable
```

If any term is missing, stale, ambiguous, queued, unexecuted or unsupported by live GitHub evidence, merge is forbidden.

## 16. Server-side safety

Prompt/process safeguards do not replace GitHub enforcement. `main` should use branch/ruleset protection requiring the intended check/merge contract and blocking force-push/deletion when repository administration permits it.

Until server-side protection exists, every automation role treats `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `AUTOMATION_STATUS.md`, `AUTOMATION_CODE_REVIEW_GATE.md`, `AUTOMATION_CAPACITY_OVERRIDE.md`, exact-head safeguards and this contract as mandatory.

No personal information may be introduced in code, tests, examples, generated metadata, tickets, PRs, reviews, commits, documentation or control-plane records.