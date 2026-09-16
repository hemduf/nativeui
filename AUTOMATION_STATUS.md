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

**T174 / #409 / PR #410 is Done.**

- frozen exact head: `e6d747961a2fd39760f5703748c10440f8fb0efa`;
- merged to `main` as `1ef326494ce00d215c1211ad0cde2437e3ffadbb`;
- executable diff bounded to five T174 files;
- normal/path qualification: CI `35154523714`, T050 `35154524036`, T066 `35154523694` all green;
- final T042 Lifecycle Stress `35156481983`: green on Linux ASan+UBSan, Linux X11, macOS and Windows;
- final T052 v0.1 Release Gate `35156481846`: green, including clean Linux/macOS/Windows bootstraps and exact-head T051 performance/allocation benchmark;
- self review `5228886765` and independent frozen-head review `5228890731`: 0 Blocking / 0 Important;
- historical inline Blocking thread resolved/outdated;
- issue #409, `CONTEXT.md`, `ROADMAP.md` and this planning snapshot synchronized in the completion cycle;
- no personal information introduced in source/tests/examples/generated metadata.

T174 resolves the last planned public input addition before the v1 API freeze. The final candidate remained executable-code frozen through Ready -> T042/T052 -> merge.

## Previous closeouts

- **T125 / #286 / PR #382:** Done; merged as `401d73983c2103fae1e48c1984a982277088e060`; final normal/path + T042/T052 green; reviews 0/0.
- **T130 / #291 / PR #408:** Done; merged as `e65e317d6584ae440f1f041b8187a63d18b2aa6a`; final normal/path + T042/T052 green; reviews 0/0.

## Current v1 critical path

All T123–T132 safety blockers are Done and T174 is resolved. The serialized source lane moves to T069.

Planned order:

```text
1. T069 / #81 / PR #269    P0 v1 public API audit/freeze
2. T070 children            reference app / Getting Started
3. T122 / #280              remaining v1 documentation, explicitly scheduled
4. T071 / #83               exact-SHA v1 release gate
```

T068 / #80 remains deferred to 1.2 and is outside the v1 delivery lane.

## Step 1 — T069 / #81 / PR #269

Current planning state: **Ready / P0**.

All explicit T123–T132 safety prerequisites are Done. T174's public `set_key_down_handler` addition is also merged before freeze, so no known pre-freeze public feature remains intentionally straddling T069.

Execution:

1. resume the existing canonical PR #269 rather than creating a replacement;
2. re-fetch current `main`, PR head/base, #81 acceptance matrix, checks, reviews and threads;
3. reconcile the PR with current `main` only as composition requires;
4. inventory every documented/stable public C++ and CMake surface required by #81;
5. perform the final breaking cleanup only within T069 scope;
6. validate header/backend isolation, lifetime/reentrancy/exception contracts and package/CMake surface;
7. run exact-head normal/path qualification;
8. complete full `CODE_REVIEW.md` self review and independent frozen-head review;
9. Draft -> Ready for final T042/T052 qualification;
10. merge immediately when the unchanged exact head is fully green and synchronize completion docs.

No new v1 public API should land behind the freeze.

## Step 2 — T070 / reference app and Getting Started

After T069 freezes the public API, execute T070 children in dependency order against that frozen surface. Keep the serialized pilot unless `AUTOMATION.md` is deliberately changed.

## Step 3 — T122 / #280

T122 remains a real v1 documentation ticket. Schedule it explicitly against the frozen/final v1 surface; it is not fallback work.

## Step 4 — T071 / #83

T071 is validation/release-only. Choose one exact release-candidate SHA after all v1 dependencies and open-issue policy are satisfied. Any behavior defect becomes a focused canonical ticket; merge the fix first, then select a new RC SHA.

## Pilot metrics

Continue recording:

- issue selected -> first coherent PR head;
- first coherent head -> frozen-head review;
- review -> merge;
- total issue -> merge time;
- qualification heads per ticket;
- review-fix cycles;
- stale-head/composition invalidations;
- infrastructure-only reruns;
- merges requiring manual orchestration recovery.

T130, T125 and T174 now provide completed serialized-delivery data points. T174 converged with one bounded final composition, exact-head CI, frozen-head review and Ready-triggered heavyweight gates before immediate merge.
