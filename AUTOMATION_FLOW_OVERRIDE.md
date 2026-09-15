# NativeUI automation flow override — same-cycle closeout service

This document is a narrow control-plane override for the scheduled NativeUI automation. It exists to remove an orchestration latency defect discovered on 2026-09-15: a PR could become `SOURCE_READY` after the hourly Scheduler snapshot, while later Delivery workers continued stale SOURCE assignments until the next Scheduler generation.

This file supplements `AUTOMATION.md`, `AUTOMATION_STATUS.md`, `AUTOMATION_CODE_REVIEW_GATE.md`, and `CI_POLICY.md`. Where this file conflicts only on **handoff timing or assignment authority**, this file wins. It does not weaken acceptance, testing, exact-head CI, peer-review independence, warning, platform, privacy, or merge requirements.

## 1. Scheduling objective

Optimize **time to safe merge**, not the number of simultaneously active branches.

Global service priority is:

1. `MERGE_READY` / merge closeout;
2. head-valid `REVIEW_PASS` / `FINAL_QUALIFICATION`;
3. `SOURCE_READY` / `SECOND_PEER_CODE_REVIEW`;
4. completed CI product-failure diagnosis;
5. ordinary SOURCE implementation;
6. documentation/fallback work.

A merge-near PR of comparable product priority may preempt a new SOURCE batch. A `SOURCE_READY` PR waiting for an eligible reviewer while such a worker starts ordinary SOURCE is an orchestration defect.

## 2. Why hourly Scheduler snapshots are insufficient

The Scheduler runs once per hour while Delivery workers run later at different offsets. Therefore a source worker can emit `SOURCE_READY` after the Scheduler has already committed the generation.

Workers MUST NOT treat `#250` as an immutable hourly assignment if newer head-valid worker events prove that a closeout transition became actionable during the same generation.

Live GitHub state and head-valid current-cycle events remain authoritative over a stale scheduler snapshot.

## 3. Conditional reservations emitted by Scheduler

For every SOURCE assignment reasonably capable of reaching `SOURCE_READY` in the current generation, Scheduler should publish an `on_source_ready` reservation naming an eligible Delivery worker different from the current-head author. Prefer a worker whose scheduled run occurs later in the same hour.

For every `REVIEW_FIX`, Scheduler should publish an `on_review_fix_applied` reservation naming a different worker for `SECOND_PEER_CODE_REVIEW` once the replacement head's applicable CI is terminal green.

Reservations consume worker attention, not a source-changing lane. A reservation that becomes true preempts that worker's ordinary SOURCE/fallback work.

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

## 6. Integration live queue

Integration MUST rebuild its queue from live GitHub state on every run. `#250`'s Integration queue is a snapshot, not an eligibility gate.

A durable independent exact-head `REVIEW_PASS` produced after the Scheduler snapshot is immediately eligible for Integration when all other preconditions are satisfied.

If a candidate reaches `QUALIFICATION_WAIT` because heavyweight workflows are queued/running, Integration continues with another independent head-valid `REVIEW_PASS` or `MERGE_READY` candidate instead of idling. Keep at most **two** final-qualification candidates active simultaneously to bound CI fan-out.

## 7. Backpressure remains per PR

Exact-head CI backpressure forbids replacing a PR head while its relevant qualification is active. It does **not** forbid review, qualification or merge work on another independent PR.

## 8. Scheduler normalization

At every new generation Scheduler must consume all head-valid mid-cycle `CLAIMED` events and normalize them into `#250`:

- phase;
- `review_lock_worker`;
- `peer_reviewer`;
- `review_fix_worker` when applicable;
- reviewed/current head and observed base;
- conditional reservations still pending.

Stale claims whose head changed are discarded.

## 9. Anti-starvation measurements

Scheduler and Reporter should track at least:

- `SOURCE_READY -> review claim`;
- `REVIEW_FIX_APPLIED + green -> second-review claim`;
- `REVIEW_PASS -> Integration action`;
- `final qualification green -> merge`.

Target invariant: **no closeout handoff waits a full Scheduler generation when an eligible later worker exists in the same cycle.**
