# NativeUI automation status

**Updated:** 2026-09-16

This file is a read-only planning snapshot derived from live GitHub state. It is not a Scheduler database and does not assign workers. If this file conflicts with current issues, pull requests, reviews or checks, live GitHub wins.

See [`AUTOMATION.md`](AUTOMATION.md) for the current deterministic delivery model.

## Automation state

```text
legacy Scheduler: suspended / retired
legacy Reporter state: suspended / retired
legacy W1-W4 worker assignments: retired
implementation/source-changing lanes: 1
review: independent, on demand for frozen candidate
fallback work: disabled
GitHub: sole durable source of truth
```

Open control-plane issues from the retired scheduler are to be closed as not planned. Product tickets remain open and are scheduled through their normal issue/PR state.

## Current v1 critical path

The v1 freeze is still blocked by T125 and T130. Both already have canonical PRs and substantial existing work, so the new model finishes them instead of creating replacement branches.

Planned serialized order:

```text
1. T125 / #286 / PR #382   P0 closeout
2. T130 / #291 / PR #383   P0 closeout
3. T174 / #409 / PR #410   resolve before public API freeze
4. T069 / #81 / PR #269    P0 v1 public API freeze
5. T070 children            v1 reference app / Getting Started
6. T122 / #280              remaining v1 documentation, scheduled explicitly
7. T071 / #83               P0 exact-SHA v1 release gate
```

T068 / #80 remains deferred to 1.2 and is outside the v1 delivery lane.

### Why T174 is before T069

T174 adds a public `UI` API and therefore must not silently cross the v1 public API freeze. After T125 and T130 close, finish T174 on its existing PR or explicitly retarget it post-v1 before starting the final T069 freeze. The default plan is to finish the existing T174 work because it is already active and reviewed; no second branch should be created.

## Step 1 — T125 / #286 / PR #382

Current known state from the retired final scheduler snapshot:

- P0 v1 freeze blocker;
- canonical PR exists;
- exact-head qualification was green on the then-current head;
- one Blocking semantic-continuation family remained around preservation of the unstarted focus/hover ancestor suffix during structural reconciliation;
- the branch required current-main composition reconciliation before final closeout.

New execution rule:

1. re-fetch live PR/head/main and discard stale scheduler assignments;
2. audit the complete focus/hover/reconciliation failure family once;
3. implement one coherent correction batch with deterministic regressions;
4. qualify the replacement exact head;
5. complete self review;
6. independent reviewer inspects the exact head;
7. correct any complete review finding set coherently;
8. merge immediately when the full gate is green;
9. close #286 and synchronize `CONTEXT.md` / `ROADMAP.md`.

No T130, T174 or fallback source changes start while T125 remains executable.

## Step 2 — T130 / #291 / PR #383

After T125 reaches a terminal merged/blocked state, resume the existing T130 PR.

The last retired scheduler snapshot recorded four Blocking families to revalidate against live state:

1. unwind-safe restoration of dynamic reconciliation guards;
2. preservation/commit semantics for dirty owners across fallible reconciliation;
3. inspector access through the correct lifecycle boundary;
4. macOS T044 native-routing readiness before the exactly-once semantic PointerDown.

Execution:

- re-fetch live head/checks/reviews before editing;
- confirm which findings still apply;
- fix the complete surviving family in one bounded batch rather than four remote micro-cycles;
- run exact-head qualification and the complete #291 acceptance matrix;
- perform independent exact-head review;
- merge immediately when green;
- synchronize issue / `CONTEXT.md` / `ROADMAP.md`.

## Step 3 — T174 / #409 / PR #410

T174 is parked while the two P0 freeze blockers are being closed. It is not a fallback assignment.

When its turn arrives:

- resume PR #410 rather than creating a replacement;
- rebase/reconcile only if composition requires it;
- compose against the final T125 dispatch semantics;
- complete required callback-throw/recovery qualification;
- run independent review and merge when green.

If product planning deliberately moves T174 post-v1 instead, record that scope change in #409 and T069 before T069 freezes the API. Do not leave the decision implicit.

## Step 4 — T069 / #81 / PR #269

Start/resume the final API freeze only after:

- T125 is Done;
- T130 is Done;
- T174 is either Done or explicitly retargeted outside v1.

Then reconcile the canonical T069 PR with current main and execute the whole public-surface inventory / freeze / validation defined by #81. No new v1 public API should be allowed to land behind the freeze.

## Step 5 — T070 / reference app and Getting Started

After T069 freezes the public API, execute T070 children in dependency order. Although some children are independent after their shared foundation, automation remains serialized during the pilot. Parallelism is not reintroduced merely because the DAG permits it.

Prefer finishing each child completely through review/merge before opening the next source-changing child.

## Step 6 — T122 / #280

T122 remains a real v1 documentation ticket but is no longer an idle-worker fallback.

Schedule it explicitly when its documentation can be completed against the frozen/final v1 surface. Documentation edits should not run concurrently merely to keep unused worker capacity occupied.

## Step 7 — T071 / #83

Run the release gate only after v1 dependencies and open-issue policy are satisfied.

T071 remains validation-only: choose one exact release-candidate SHA and do not hide implementation fixes inside the release gate. Any behavior defect becomes a focused ticket, is merged first, then a new RC SHA is selected.

## Pilot metrics

For the next 5–10 completed tickets record:

- qualification heads per ticket;
- review-fix cycles;
- stale-head/composition invalidations;
- issue-to-merge time;
- time from frozen head to independent review;
- time from final green to merge;
- manual orchestration recoveries.

Do not increase automated source parallelism until these measurements show that serialized delivery is converging reliably.