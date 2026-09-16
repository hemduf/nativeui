# NativeUI automation status

**Updated:** 2026-09-16

This file is a read-only planning snapshot derived from live GitHub state. It is not a Scheduler database and does not assign workers. If this file conflicts with current issues, pull requests, reviews or checks, live GitHub wins.

See [`AUTOMATION.md`](AUTOMATION.md) for the deterministic serialized delivery model.

## Automation state

```text
legacy Scheduler: retired
legacy Reporter state: retired
legacy W1-W4 persistent assignments: retired
implementation/source-changing lanes: 1
review: independent phase on demand for frozen candidate
fallback work: disabled
GitHub: sole durable source of truth
```

## Completed current closeout

**T130 / #291 / PR #408 is Done.**

- frozen executable head: `4c40b881fa13ea0f9839ae415059744f6107c62d`;
- merged to `main` as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`;
- normal/path qualification: CI + Package Contracts + T044/T050/T060/T064/T065/T066/T072 all green;
- final T042 Lifecycle Stress `35140156948`: green on Linux ASan+UBSan, Linux X11, macOS and Windows;
- final T052 v0.1 Release Gate `35140156810`: green, including clean Linux/macOS/Windows bootstraps and exact-head T051 performance/allocation benchmark;
- final review records report 0 Blocking / 0 Important findings;
- issue #291, `CONTEXT.md` and `ROADMAP.md` synchronized in the completion cycle;
- no personal information introduced in source/tests/examples/generated metadata.

The first Windows normal-CI attempt failed only while downloading external Mesa after a successful build. A targeted rerun on the unchanged head passed Mesa and Windows tests; no source change was made for that infrastructure failure.

## Current v1 critical path

Live state now reduces the hard pre-freeze safety frontier to T125.

Planned serialized order:

```text
1. T125 / #286 / PR #382   P0 retained dispatch/reconciliation closeout
2. T174 / #409 / PR #410   finish on final T125 semantics or explicitly retarget post-v1
3. T069 / #81 / PR #269    P0 v1 public API freeze
4. T070 children            reference app / Getting Started
5. T122 / #280              remaining v1 documentation, explicitly scheduled
6. T071 / #83               exact-SHA v1 release gate
```

T068 / #80 remains deferred to 1.2 and is outside the v1 delivery lane.

## Step 1 — T125 / #286 / PR #382

Current live issue state: **Doing / P0**.

T125 is now the only non-Done hard safety prerequisite among T123–T132 for T069. Resume its existing canonical PR rather than creating a replacement.

Execution:

1. re-fetch the live PR head, `main`, checks, review threads and issue acceptance matrix;
2. re-audit the complete remaining focus/hover/reconciliation continuation family once;
3. implement one coherent correction batch with deterministic regressions;
4. run exact-head normal/path qualification;
5. complete the full self-review record;
6. run the independent frozen-head review phase;
7. if review changes executable code, requalify the replacement head and review it again;
8. transition Draft -> Ready only for the frozen final candidate to run T042/T052;
9. merge immediately when the complete gate is green;
10. close #286 and synchronize `CONTEXT.md`, `ROADMAP.md` and this planning snapshot.

No T174 or fallback source change starts automatically while T125 remains executable.

## Step 2 — T174 / #409 / PR #410

Current live issue state: **Blocked by T125 / P1**.

T174 adds a public per-UI fallback for unhandled raw KeyDown shortcuts. Its existing candidate is intentionally composed on T125's dispatch unwind/reconciliation contract.

After T125 merges:

- resume PR #410 rather than creating another branch;
- retarget/reconcile onto final `main` only as composition requires;
- keep T125's canonical dispatch recovery unchanged;
- rerun the T174 callback-throw/recovery, command/text/IME/isolation and feature-example matrix;
- complete exact-head review and final qualification;
- merge when green.

If product planning deliberately moves T174 post-v1 instead, record that decision in #409 and the T069 freeze record before T069 starts. Do not leave a new public-API ticket implicitly straddling the freeze.

## Step 3 — T069 / #81 / PR #269

Current live issue state: **Blocked / P0**.

T130 is now satisfied. T069 may resume only when:

- T125 is Done, completing the T123–T132 hard safety prerequisites;
- T174 is Done or explicitly retargeted outside v1 because it adds public API.

Then reconcile the canonical T069 PR with current `main` and execute the whole public-surface inventory/freeze/validation defined by #81. No new v1 public API should land behind the freeze.

## Step 4 — T070 / reference app and Getting Started

After T069 freezes the public API, execute T070 children in dependency order. Keep the serialized pilot: finish each source-changing child through review/merge before starting the next unless the automation policy is deliberately changed.

## Step 5 — T122 / #280

T122 remains a real v1 documentation ticket, not idle-worker fallback work. Schedule it explicitly against the frozen/final v1 surface.

## Step 6 — T071 / #83

T071 is validation/release-only. Choose one exact release-candidate SHA after all v1 dependencies and open-issue policy are satisfied. Any behavior defect becomes a focused canonical ticket; merge the fix first, then select a new RC SHA.

## Pilot metrics

For the next completed tickets continue recording:

- issue selected -> first coherent PR head;
- first coherent head -> frozen-head review;
- review -> merge;
- total issue -> merge time;
- qualification heads per ticket;
- review-fix cycles;
- stale-head/composition invalidations;
- infrastructure-only reruns;
- merges requiring manual orchestration recovery.

T130 contributes one completed serialized-delivery data point: several pre-retirement inherited qualification heads were required, but the final frozen candidate converged to complete normal/path + T042/T052 green and merged without changing the executable head after Ready.
