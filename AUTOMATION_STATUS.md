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

**T125 / #286 / PR #382 is Done.**

- frozen executable head: `da4386626837b8cedf9d7f17bc6e8b150aa99921`;
- merged to `main` as `401d73983c2103fae1e48c1984a982277088e060`;
- normal/path qualification: CI + Package Contracts + T044/T050/T060/T064/T065/T066/T072 all green;
- final T042 Lifecycle Stress `35153051058`: green on Linux ASan+UBSan, Linux X11, macOS and Windows;
- final T052 v0.1 Release Gate `35153050940`: green, including clean Linux/macOS/Windows bootstraps and exact-head T051 performance/allocation benchmark;
- self-review `5228579161` and independent review `5228585872`: 0 Blocking / 0 Important;
- no unresolved review thread remained;
- `CONTEXT.md`, `ROADMAP.md` and this planning snapshot synchronized in the completion cycle;
- no personal information introduced in source/tests/examples/generated metadata.

T125 required several inherited review/qualification iterations before the serialized model converged on the final candidate. The final Ready head remained executable-code frozen through T042/T052 and merged immediately when the full gate became green.

## Previous closeout

**T130 / #291 / PR #408 is Done.**

- frozen executable head: `4c40b881fa13ea0f9839ae415059744f6107c62d`;
- merged to `main` as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`;
- normal/path qualification and final T042/T052 were green;
- final reviews reported 0 Blocking / 0 Important.

## Current v1 critical path

The T123–T132 safety frontier is now fully Done. The next serialized source-changing ticket is T174, whose only blocker was T125.

Planned serialized order:

```text
1. T174 / #409 / PR #410   finish on final T125 semantics or explicitly retarget post-v1
2. T069 / #81 / PR #269    P0 v1 public API freeze
3. T070 children            reference app / Getting Started
4. T122 / #280              remaining v1 documentation, explicitly scheduled
5. T071 / #83               exact-SHA v1 release gate
```

T068 / #80 remains deferred to 1.2 and is outside the v1 delivery lane.

## Step 1 — T174 / #409 / PR #410

Current live planning state after T125 completion: **Ready / P1**.

T174 adds a public per-UI fallback for unhandled raw KeyDown shortcuts. Its existing candidate was intentionally composed on T125's dispatch unwind/reconciliation contract.

Execution:

1. resume PR #410 rather than creating another branch;
2. re-fetch current `main`, PR head, issue matrix, checks, reviews and threads;
3. retarget/reconcile onto final T125/current `main` only as composition requires;
4. verify T125's canonical dispatch recovery remains unchanged;
5. run the fallback callback-throw/recovery, command/text/IME/isolation and feature-example matrix;
6. run exact-head normal/path qualification;
7. complete self-review and independent frozen-head review;
8. transition Draft -> Ready only for the frozen final candidate;
9. run T042/T052 where applicable and merge immediately when the complete gate is green;
10. synchronize #409, `CONTEXT.md`, `ROADMAP.md` and this snapshot.

If product planning deliberately moves T174 post-v1 instead, record that decision in #409 and the T069 freeze record before T069 starts. Do not leave a new public-API ticket implicitly straddling the freeze.

## Step 2 — T069 / #81 / PR #269

Current live planning state: **Blocked / P0** pending the T174 pre-freeze decision.

All T123–T132 hard safety prerequisites are now Done. T069 may resume once T174 is either:

- Done and merged; or
- explicitly retargeted outside v1 because it adds public API.

Then reconcile the canonical T069 PR with current `main` and execute the whole public-surface inventory/freeze/validation defined by #81. No new v1 public API should land behind the freeze.

## Step 3 — T070 / reference app and Getting Started

After T069 freezes the public API, execute T070 children in dependency order. Keep the serialized pilot: finish each source-changing child through review/merge before starting the next unless the automation policy is deliberately changed.

## Step 4 — T122 / #280

T122 remains a real v1 documentation ticket, not idle-worker fallback work. Schedule it explicitly against the frozen/final v1 surface.

## Step 5 — T071 / #83

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

T130 and T125 now contribute completed serialized-delivery data points. T125 demonstrates that a long inherited branch can still converge under the serialized model when source changes stop at a frozen reviewed head, normal/path qualification completes, Ready triggers T042/T052, and merge happens immediately after the full green gate.
