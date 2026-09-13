# NativeUI scheduled automation contract

This document defines the execution contract for autonomous GitHub-only NativeUI delivery. It supplements `AGENTS.md`, `CI_POLICY.md`, and `CODE_REVIEW.md`; it never weakens product acceptance, review, test, warning, platform, or merge requirements.

GitHub live state is authoritative. `ROADMAP.md`, `CONTEXT.md`, scheduler snapshots, task output, and worker events are recovery/coordination data and must never override live issues, pull requests, exact heads, explicit dependencies, or executed checks.

## 1. Objective

The automation exists to finish the complete planned NativeUI backlog with maximum safe throughput and minimal human intervention.

It must:

- make scheduling decisions autonomously from the complete live project graph;
- keep at most three simultaneous source-changing lanes;
- avoid idle capacity when dependency-ready useful work exists;
- preserve one canonical issue/branch/PR stream per product ticket;
- use coherent batch-first TDD and exact-head executable evidence;
- perform independent integration/review before merge;
- merge autonomously only when the Definition of Done is genuinely satisfied;
- continue after release gates until planned backlog exhaustion;
- maintain durable GitHub-to-GitHub communication between workers;
- provide separate read-only project reporting.

## 2. Environment boundary

Scheduled workers may run without a repository checkout, shell, Docker, compiler, or arbitrary network access. In that case they use the GitHub connector and existing GitHub Actions.

When local execution required by the normal developer workflow is unavailable:

- never claim it passed;
- record `local execution unavailable in connector-only scheduled environment` where relevant;
- keep test-first intent inside the same coherent source batch;
- do not publish intentional RED heads merely to use CI as the inner development loop;
- publish tests + implementation + required refactor as one qualification-worthy head;
- use GitHub Actions as the first executable validation layer available to the scheduled worker.

The environment changes where evidence is obtained, not the Definition of Done.

## 3. Final operating model

Exactly six NativeUI automation roles are active:

1. **Scheduler** — decision engine and control-plane owner. It never changes product source/tests and never merges.
2. **Delivery W1** — interchangeable end-to-end implementation worker.
3. **Delivery W2** — interchangeable end-to-end implementation worker.
4. **Delivery W3** — interchangeable end-to-end implementation worker.
5. **Integration** — independent closeout/integration worker: review, CI diagnosis, safe synchronization, final qualification, merge, status/unlock bookkeeping. It does not implement product features.
6. **Reporter** — read-only project, CI, critical-path, orchestration-health and efficiency reporting.

There is no permanent audit-only worker and no permanent burst-only worker. Delivery workers have only soft affinities used as tie-breakers; no ticket family is permanently owned.

At most three product tickets may be source-changing at once. Review, preflight, CI wait and final qualification do not consume a source-changing lane.

## 4. GitHub blackboard: the shared communication model

GitHub is the shared memory for every automation role.

### 4.1 Product issues

Product issues define:

- scope and non-goals;
- priority;
- explicit `Dependencies:`;
- acceptance criteria and required tests;
- product status/lease.

### 4.2 Product pull requests and Actions

PRs and exact-head Actions define:

- canonical implementation stream;
- current exact head and observed base composition;
- code/review evidence;
- executed CI/platform/sanitizer evidence;
- mergeability.

Any actionable source/review finding must be durable in the product issue or PR, not only in task output.

### 4.3 `#250` scheduler control state

Issue `#250` is the compact machine snapshot written **only by the Scheduler**. Delivery, Integration and Reporter never modify it.

It records the committed generation, current cycle issue, current main/head observations, three Delivery assignments, secondary work, conditional unlock reservations, Integration queue, Ready queue, CI waits, critical path, status inconsistencies, stalls and handoffs.

### 4.4 Current `AUTOMATION CYCLE — GNNN — ...` issue

Each committed generation has exactly one current cycle issue.

Its body is the human-readable plan. Its comments are the **Worker → Scheduler event bus**.

`#250` and all orchestration-cycle issues are control-plane records, not product work items. They are excluded from product backlog/status percentages, dependencies, leases, lane utilization and release readiness.

Task/chat mini-reports are informational only and are never coordination state.

## 5. Worker event protocol

Meaningful worker transitions are comments on the cycle issue currently referenced by `#250.current_cycle_issue`.

Every event starts with:

```text
<!-- nativeui-worker-event:v1 -->
```

and contains a compact structured block with at least:

```yaml
generation: <current committed generation>
assignment_generation: <generation that assigned the work|null>
worker: W1|W2|W3|Integration
kind: <event kind>
ticket: TNNN|null
issue: <number|null>
pr: <number|null>
head: <exact SHA|null>
base: <observed target/base SHA|null>
result: <concise factual result>
next_action: <concrete next action>
```

Allowed kinds:

- `CLAIMED`
- `PROGRESS`
- `PREFLIGHT_READY`
- `CI_WAIT`
- `CI_FAILED_PRODUCT`
- `CI_FAILED_INFRA`
- `SOURCE_READY`
- `REVIEW_PASS`
- `REVIEW_BLOCKED`
- `REWORK_REQUIRED`
- `QUALIFICATION_WAIT`
- `QUALIFICATION_FAILED_PRODUCT`
- `QUALIFICATION_FAILED_INFRA`
- `MERGE_READY`
- `MERGED`
- `BLOCKED`
- `HANDOFF_REQUEST`

Rules:

- write an event only for a meaningful state/head/phase change; never emit hourly heartbeat spam;
- an event referring to a PR/review/qualification must include its exact head SHA;
- `REVIEW_PASS`, `QUALIFICATION_*`, `MERGE_READY` and `MERGED` also record the observed base/main SHA when that composition matters;
- if live head differs, the event is stale and cannot authorize review PASS, qualification or merge;
- if the base/main changed materially since executable qualification, Integration re-evaluates composition according to `CI_POLICY.md`; project-state-only documentation may remain non-invalidating, executable/API/build changes do not;
- detailed code findings belong in the PR; the cycle event summarizes their scheduling consequence;
- a `MERGED` event must be verified against live PR/issue state before any dependent claim;
- workers always read the current cycle comments before deciding their useful action.

### 5.1 Generation-race rule

Immediately before posting any worker event, the worker must re-fetch `#250` and its `current_cycle_issue`.

If the committed generation changed while the worker was running:

1. revalidate the live issue/PR/head and the new generation assignment/reservation;
2. if the result is still relevant, post the event to the **new current cycle**, use the new `generation`, and preserve the original `assignment_generation`;
3. if the new plan invalidates the scheduling consequence, do not post a misleading control-plane event; keep any durable technical finding in the product PR/issue and let the Scheduler rediscover live state;
4. never post a fresh coordination event only to a superseded/closed cycle.

This prevents completed work from being lost when a worker overlaps a Scheduler generation transition.

## 6. Product status and leases

`Doing` is the source/review/validation lease for a product ticket.

Before assignment, claim, handoff or dependency unlock, read together:

1. GitHub issue open/closed state and reason;
2. body `## Status`;
3. all `status:*` labels.

Coherent completed/active product states are:

- Ready = open + body Ready + exactly `status:ready`;
- Doing = open + body Doing + exactly `status:doing`;
- Blocked = open + body Blocked + exactly `status:blocked`;
- Done = closed/completed + body Done + exactly `status:done`.

A closed `not_planned`/duplicate issue is a separate terminal non-delivery state: it is excluded from scheduling and does **not** satisfy a dependency as Done unless the dependency was explicitly removed/replaced. Do not automatically rewrite such tickets to Done merely to fit the four active/completed states.

Other disagreement is `status_incoherent`; newly claiming that ticket is forbidden until Integration repairs an unambiguous inconsistency.

Before a new product claim, a Delivery worker must verify dependencies Done, coherent Ready state and absence of another canonical PR/owner, set Doing/status:doing, re-fetch, then create/continue exactly one canonical PR.

## 7. Scheduler: autonomous decision engine

Every cycle the Scheduler must inspect the **whole relevant project**, not only currently assigned tickets.

It reads:

- repository policy docs;
- `#250` and current cycle events;
- all open product issues plus dependencies/priorities/status;
- all open product PRs and exact heads;
- exact-head workflows/checks/reviews/mergeability and observed base composition;
- recently merged work needed to recompute unlocks.

Then it:

1. validates worker events against live state;
2. recomputes the dependency DAG/frontier;
3. classifies existing Doing tickets as source-changing, CI-wait, review/closeout, qualification-wait, blocked or stalled;
4. computes available source capacity out of three;
5. ranks useful work by priority, critical-path/downstream-unblock value, number/value of dependants unlocked, proximity to merge/release, expected effort and conflict risk; worker affinity is only a tie-breaker;
6. assigns each Delivery worker a `primary` and, where useful, one `secondary` review/preflight/fallback action;
7. assigns Integration an ordered closeout/CI/merge queue;
8. publishes `on_unlock` conditional reservations for likely imminent merges so Delivery workers can react safely before the next Scheduler cycle;
9. records objective reasons for standby when no useful legal work exists.

### 7.1 Work-conserving requirement

A Delivery worker must not remain a passive observer while useful safe work exists.

Scheduler secondary work priority:

1. independent peer review of another exact-head PR that has not already received equivalent review;
2. CI-failure analysis when it can shorten rework;
3. substantive preflight for an imminent dependency-blocked ticket: completeness matrix, affected surface, test plan and conflict analysis, without source changes;
4. only then standby.

A run that merely reports unchanged state is not considered productive.

### 7.2 Stall/handoff

Do not infer a stall merely from absence of a commit. Useful events, CI execution, durable review/preflight evidence or an explicit blocker count as progress.

A source handoff is allowed only after at least two committed generations with no useful progress, no relevant CI running, no explicit blocker and a live re-check. Handoff keeps the same canonical PR; never create a competing implementation branch.

## 8. Scheduler transaction and crash recovery

Cycle publication is ordered to avoid the gap observed in the earlier design.

For generation `N -> N+1`:

1. read/validate generation N events and live state;
2. compute the complete N+1 plan;
3. create the N+1 cycle issue first;
4. update `#250` atomically as far as the API permits to point to N+1 and its cycle issue;
5. re-fetch and verify `#250` + N+1 issue;
6. only then transition N cycle to Done/status:done and close completed.

If multiple orchestration cycles are open after a crash:

- `#250.current_cycle_issue` is the committed cycle;
- a newer valid orphan may be adopted only after revalidating its plan against live state;
- otherwise close orphan/superseded cycles as control-plane records;
- never let workers infer current assignment from issue creation time alone.

## 9. Delivery W1/W2/W3 workflow

All three Delivery workers are interchangeable and capable of the complete implementation path up to independent final integration.

At every run:

1. read policy docs, #250, current cycle body/comments, live product issue/PR/head/CI;
2. reject stale/conflicting assignments;
3. follow this work ladder:
   - continue actionable primary source work;
   - if primary exact head is CI-wait/review/final-gate, use assigned source fallback if capacity permits;
   - otherwise perform assigned peer review or CI diagnosis;
   - otherwise perform assigned substantive preflight;
   - otherwise standby because no useful legal work exists;
4. use batch-first TDD: inspect complete bounded surface, tests before correction, implementation + refactor in one coherent qualification head;
5. obey exact-head backpressure: no push over queued/in-progress useful CI;
6. max one new qualification head per PR per run;
7. before every repository write re-fetch issue/PR/branch/main; never force-push or discard concurrent work;
8. perform own completeness/self-review before declaring `SOURCE_READY`;
9. immediately before every event write, apply the generation-race rule from section 5.1.

Delivery workers do not merge their own product PRs. Independent final integration belongs to Integration.

A Delivery worker may review another worker's PR without consuming a source lane. It may source-fix another worker's PR only after Scheduler assignment or an explicit current-generation handoff; it must never silently take over.

## 10. Integration worker

Integration is productive, not a reporting role.

Ordered responsibilities:

1. service the highest-impact closeout/merge candidate first; critical path and release unlocks outrank historical metadata cleanup;
2. inspect exact-head CI failures and classify product vs infrastructure;
3. perform/verify independent final `CODE_REVIEW.md` review and completeness evidence;
4. if product rework is needed, record the detailed finding in the PR and emit `REWORK_REQUIRED`/`QUALIFICATION_FAILED_PRODUCT`; do not hide feature/source fixes inside integration;
5. for infrastructure-only failures, use supported rerun/recovery mechanisms when available without manufacturing a new source head;
6. perform safe mechanical main synchronization/merge-conflict resolution only when it changes no intended product behavior; any changed head is requalified;
7. transition Draft -> Ready only for a frozen candidate satisfying `CI_POLICY.md` preconditions;
8. verify heavyweight final gates and the candidate's observed base/main composition;
9. merge only exact expected head with zero Blocking/Important finding and complete acceptance/test evidence;
10. transition product issue Done/closed, synchronize project-state docs as required, recompute immediately affected dependants and make newly available product tickets Ready;
11. emit `MERGED` including verified unlocks and observed base/main SHA, after applying the generation-race rule.

Status repair is background work and must never delay a critical-path qualification or merge.

## 11. In-cycle reaction without waiting for the next Scheduler

The Scheduler remains the authority for new arbitrary assignments, but workers may react to verified events to remove avoidable latency:

- a Delivery worker may continue fixing its already-leased primary after Integration emits head-matching `REWORK_REQUIRED`;
- a Delivery worker may use an `on_unlock` reservation from #250/cycle only after verifying the referenced `MERGED` event and live dependency/status state;
- a Delivery worker may use a pre-assigned fallback when its primary enters exact-head CI wait;
- no in-cycle reaction may exceed three source-changing lanes or create a competing PR.

## 12. CI/backpressure

Remote CI is qualification, not the inner TDD loop.

If exact-head relevant workflows are queued/in-progress:

- do not push a new small correction;
- use the free source capacity on an assigned independent ticket when possible;
- otherwise perform review/preflight/evidence work;
- inspect all completed failing jobs before producing a correction batch.

`startup_failure`, zero-job/no-step failures and equivalent non-executed results are infrastructure/unqualified evidence, never product green or product red.

A successful PR workflow must be interpreted with its tested PR/base composition. If `main` advances with executable/API/build changes, Integration must re-evaluate/requalify the candidate as required by `CI_POLICY.md`; pure project-state documentation does not by itself force executable requalification.

T042/T052 remain final-candidate gates according to `CI_POLICY.md`.

## 13. Release gates and full-project continuation

T071/T121 are validation-only release gates. Product/API fixes discovered there belong in focused product work followed by a new exact RC.

Completing v1.0, v1.1 or another release does not stop automation. Scheduler continues through the complete dependency-ready planned backlog and future explicit product tickets until no planned work remains.

## 14. Reporting

Reporter is strictly read-only and outside delivery capacity.

Hourly reporting must show:

- weighted global/release progress and DoD progress;
- complete active/critical ticket and PR state;
- exact-head CI details and product-vs-infrastructure failures;
- dependency graph/critical path/unlocks;
- next optimal plan and estimates;
- committed scheduler generation/current cycle;
- W1/W2/W3 and Integration last observable actions/events;
- useful source lanes active/3;
- worker events and whether any are stale/unconsumed;
- time from CI completion to useful action and merge to dependent claim;
- productive vs passive runs;
- collisions, stalls, handoffs and status inconsistencies;
- branch protection/ruleset state.

Control-plane activity never inflates product progress.

## 15. Server-side safety

Prompt/process safeguards are not a substitute for GitHub enforcement. `main` should have branch/ruleset protection requiring the intended merge/check contract and blocking force-push/deletion when repository administration permits it.

Until server-side protection exists, every automation role treats `AGENTS.md`, `CI_POLICY.md`, `CODE_REVIEW.md`, exact-head safeguards and this contract as mandatory.