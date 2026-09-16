# NativeUI automation flow override — same-cycle closeout service

This document is a narrow control-plane override for the scheduled NativeUI automation. It exists to remove orchestration latency defects discovered on 2026-09-15: a PR could become `SOURCE_READY` after the hourly Scheduler snapshot while later Delivery workers continued stale SOURCE assignments, and a nearly-qualified long-lived branch could drift behind a rapidly advancing `main` until final qualification became wasted work.

This file supplements `AUTOMATION.md`, `AUTOMATION_STATUS.md`, `AUTOMATION_CODE_REVIEW_GATE.md`, `AUTOMATION_CAPACITY_OVERRIDE.md`, and `CI_POLICY.md`. Where this file conflicts only on **handoff timing, assignment authority, closeout priority, or pre-closeout main-composition timing**, this file wins. It does not weaken acceptance, testing, exact-head CI, peer-review independence, warning, platform, privacy, or merge requirements.

## 1. Scheduling objective

Optimize **time to safe merge**, not the number of simultaneously active branches.

Global service priority is:

1. `MERGE_READY` / merge closeout;
2. head-valid `REVIEW_PASS` / `FINAL_QUALIFICATION`;
3. `SOURCE_READY` / `SECOND_PEER_CODE_REVIEW`;
4. source-complete or exact-head-green **closeout work** (acceptance matrix, self review, safe main reconciliation);
5. completed CI product-failure diagnosis;
6. ordinary SOURCE implementation;
7. documentation/fallback work.

A merge-near PR of comparable product priority preempts a new SOURCE batch. A `SOURCE_READY` PR waiting for an eligible reviewer while such a worker starts ordinary SOURCE is an orchestration defect.

### 1.1 Closeout-biased capacity

A ticket is a **closeout candidate** when its intended product scope is substantially implemented and remaining work is dominated by one or more of:

- exact-head CI completion/diagnosis;
- acceptance/test/non-goal reconciliation;
- mandatory self `CODE_REVIEW.md` pass;
- safe main-composition reconciliation;
- peer review / review fix / second review;
- final qualification / merge bookkeeping.

When one or more closeout candidates exist, Scheduler should keep enough implementation capacity to finish them before opening lower-value source batches. In particular:

- never start a new fallback SOURCE batch merely to keep all four lanes busy while a closeout candidate can make progress;
- prefer finishing one 90–95%-complete ticket over advancing several unrelated tickets from 40% to 50%, unless an explicit dependency/critical-path calculation proves otherwise;
- source work already inside one coherent batch may finish that batch, but must not automatically start the next batch when a higher-priority closeout transition is waiting.

This is a scheduling preference, not a waiver of dependency or safety rules.

## 2. Why hourly Scheduler snapshots are insufficient

The Scheduler runs once per hour while Delivery workers run later at different offsets. Therefore a source worker can emit `SOURCE_READY` after the Scheduler has already committed the generation.

Workers MUST NOT treat `#250` as an immutable hourly assignment if newer head-valid worker events prove that a closeout transition became actionable during the same generation.

Live GitHub state and head-valid current-cycle events remain authoritative over a stale scheduler snapshot.

## 3. Conditional reservations emitted by Scheduler

For every SOURCE assignment reasonably capable of reaching `SOURCE_READY` in the current generation, Scheduler should publish an `on_source_ready` reservation naming an eligible Delivery worker different from the current-head author. Prefer a worker whose scheduled run occurs later in the same hour.

For every `REVIEW_FIX`, Scheduler should publish an `on_review_fix_applied` reservation naming a different worker for `SECOND_PEER_CODE_REVIEW` once the replacement head's applicable CI is terminal green.

Reservations consume worker attention, not a source-changing lane. A reservation that becomes true preempts that worker's ordinary SOURCE/fallback work.

### 3.1 Mandatory idle-worker documentation fallback

A conditional reservation is **not active work** while its trigger is false. Scheduler and Delivery workers MUST distinguish `RESERVED_WAIT` from immediately executable work.

When a worker has no immediately executable higher-priority implementation, review, review-fix, qualification/integration-support, or explicit blocker-resolution assignment, that worker MUST advance **T122 — V1 documentation completion** rather than wait idle.

Rules:

1. `RESERVED_WAIT` workers keep their reservation but execute T122 documentation until the reservation becomes actionable.
2. T122 fallback is documentation-only, granular, conflict-aware, and safely interruptible between coherent units.
3. When the reserved or other higher-priority assignment becomes executable, it preempts T122 at the next worker/event boundary.
4. Multiple workers may advance T122 concurrently only after synchronizing current documentation state and claiming distinct documentation units where possible.
5. Documentation fallback does not consume a product source-changing lane and does not justify weakening closeout, review, CI, or merge gates.
6. Convergence/head-budget limits on product SOURCE work do not disable documentation fallback.
7. Scheduler and Reporter MUST expose both a worker's pending reservation and its active T122 fallback assignment. A worker must never be reported as productively assigned merely because it is on standby.
8. While T122 has remaining documentation work, target worker execution utilization is **4/4** even when product source-changing utilization is intentionally lower.

This fallback is an orchestration invariant, not an optional scheduling preference. A worker waiting idle with only a false conditional reservation while T122 has actionable documentation is an orchestration defect.

## 4. Mid-cycle self-service review claim

At the beginning of every Delivery run, before executing the snapshot `primary`, inspect the current cycle events and live PR state for:

- a head-valid `SOURCE_READY` with no current-head `REVIEW_PASS` and no active review claim/lock;
- a head-valid `REVIEW_FIX_APPLIED` whose applicable exact-head CI is terminal green and which has no second-review claim/PASS;
- a conditional reservation targeting the worker whose condition is now true.

If the worker is independent from every worker that produced/modified the current head, mandatory review preempts ordinary SOURCE/fallback work.

Before reviewing, the worker posts a current-cycle event:

```text
<!-- nativeui-worker-event:v1 -->
...
kind: CLAIMED
...
result: "mode=PEER_CODE_REVIEW" # or SECOND_PEER_CODE_REVIEW
```

The event must include the exact PR/head/base and assignment generation. Immediately re-fetch cycle events. The earliest still-head-valid claim wins. If another eligible worker already claimed that exact head, do not duplicate the review.

A winning `CLAIMED` event is a **provisional review lock** until Scheduler normalizes it into `#250` in the next generation. Every source worker must inspect these provisional locks before any repository write.

The provisional lock has the same no-source-write consequence as the normal `review_lock_worker` field.

## 5. Reviewer-owned correction fast path

The normal `AUTOMATION_CODE_REVIEW_GATE.md` safety criteria still apply.

When a peer reviewer finds a bounded, unambiguous defect eligible for reviewer-owned correction, it may avoid an hourly Scheduler round-trip:

1. submit the durable current-head PR review with verdict `REVIEW_FIX_REQUIRED`;
2. post a current-cycle `CLAIMED` event whose result states `mode=REVIEW_FIX`;
3. re-fetch head, cycle events and review locks;
4. if still uncontested, apply **one** coherent TDD correction batch and publish at most one replacement qualification head;
5. emit `REVIEW_FIX_APPLIED` with old/new head, findings and tests;
6. freeze the PR for applicable exact-head CI.

The review-fix author becomes a current-head author and **cannot** issue the final `REVIEW_PASS` for that head. A different worker must perform `SECOND_PEER_CODE_REVIEW`.

Ambiguous findings, product/API decisions, broad redesigns, scope expansion or unsafe concurrent conflicts still use `REVIEW_BLOCKED` / `REWORK_REQUIRED`; no fast-path source correction is allowed.

## 6. Pre-closeout main-composition checkpoint

Rapid merges can make a long-lived source branch stale immediately before review/final qualification. Avoid spending peer-review or heavyweight qualification budget on a candidate that is already composition-invalid.

Before a code-changing PR may emit `SOURCE_READY`, the source worker must perform a **main-composition checkpoint** after the intended ticket scope is complete and before the final self-review is considered authoritative:

1. fetch current `main`, PR base, exact head, mergeability/rebaseability and the commits/files by which `main` advanced;
2. classify advances as:
   - **control-plane/docs-only and non-overlapping** — no branch rewrite is required solely for these changes; record composition compatibility;
   - **product/build/test changes but provably non-overlapping and semantically independent** — record the composition audit and continue only when repository policy permits an unchanged candidate;
   - **overlapping, dependency-changing, ABI/public-contract-changing, merge-conflicting, or otherwise composition-relevant** — reconcile current `main` before `SOURCE_READY`;
3. if GitHub reports `mergeable_state=dirty`, `mergeable=false`, `rebaseable=false`, or the branch misses a required dependency/product change, treat it as SOURCE work immediately; never wait for nonexistent CI;
4. perform only a safe mechanical main synchronization when intended product behavior does not change. If reconciliation changes executable code/tests/build behavior or resolves a semantic conflict, the resulting head is a new qualification candidate and all head-scoped evidence must target that new head;
5. after reconciliation, run the required exact-head qualification and then perform the final acceptance/self-review on the resulting head.

Do **not** repeatedly merge/rebase `main` into every active branch on every upstream commit. The checkpoint is closeout-biased and composition-driven. This avoids both branch drift and pointless CI churn.

### 6.1 Re-check before final qualification and merge

Integration still re-fetches current `main` before Draft->Ready and before merge. If `main` advanced after peer review:

- control-plane/docs-only, non-overlapping advances may be accepted by an explicit composition audit when policy allows;
- composition-relevant advances require synchronization and therefore a new head, fresh peer review, and fresh applicable qualification.

No stale exact-head review is carried across a head change.

## 7. Integration live queue

Integration MUST rebuild its queue from live GitHub state on every run. `#250`'s Integration queue is a snapshot, not an eligibility gate.

A durable independent exact-head `REVIEW_PASS` produced after the Scheduler snapshot is immediately eligible for Integration when all other preconditions are satisfied.

If a candidate reaches `QUALIFICATION_WAIT` because heavyweight workflows are queued/running, Integration continues with another independent head-valid `REVIEW_PASS` or `MERGE_READY` candidate instead of idling. Keep at most **two** final-qualification candidates active simultaneously to bound CI fan-out.

## 8. Backpressure remains per PR

Exact-head CI backpressure forbids replacing a PR head while its relevant qualification is active. It does **not** forbid review, qualification or merge work on another independent PR.

## 9. Scheduler normalization

At every new generation Scheduler must consume all head-valid mid-cycle `CLAIMED` events and normalize them into `#250`:

- phase;
- `review_lock_worker`;
- `peer_reviewer`;
- `review_fix_worker` when applicable;
- reviewed/current head and observed base;
- conditional reservations still pending;
- active T122 documentation fallback assignments for workers whose reservations remain non-executable;
- main-composition checkpoint status for closeout candidates.

Stale claims whose head changed are discarded.

## 10. Closeout service-level objectives

Scheduler and Reporter track these latencies from durable event/live timestamps:

- `SOURCE_READY -> review claim`: **target = next eligible Delivery run in the same cycle; alert if > 30 min when an eligible worker existed**;
- `REVIEW_FIX_APPLIED + terminal-green CI -> second-review claim`: **target = next eligible Delivery run; alert if > 30 min**;
- `REVIEW_PASS -> Integration action`: **target = same-cycle Integration run when one remains, otherwise the next Integration run; alert if > 60 min**;
- `final qualification terminal green -> merge or durable blocker`: **target = next Integration run; alert if > 60 min**;
- `closeout-ready exact-head green -> SOURCE_READY or durable finding`: **target = one source-worker run; alert if the worker instead begins an unrelated new source batch**.

An SLO alert is an orchestration finding, not permission to waive any review or test gate.

Target invariant: **no closeout handoff waits a full Scheduler generation when an eligible later worker exists in the same cycle.**

## 11. Trend / scheduler health

Reporter should distinguish throughput caused by draining a backlog of nearly-finished PRs from sustainable implementation throughput. Track both:

- merge count and median merge interval;
- number of closeout candidates at snapshot;
- average/maximum branch age behind current `main` at the composition checkpoint;
- count of qualification runs invalidated by subsequent composition-relevant main changes;
- count of review/Integration starvation SLO breaches;
- source-lane utilization separately from closeout worker utilization;
- worker execution utilization, including active T122 fallback work separately from product SOURCE/review work;
- count of idle-worker invariant breaches (`RESERVED_WAIT` with no active T122 fallback while documentation remained actionable).

A healthy scheduler may intentionally show fewer simultaneous source-changing lanes while merge throughput and critical-path completion improve. It should not leave executable worker capacity idle when T122 documentation remains actionable.
