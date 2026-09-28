# NativeUI automated code-review gate

This document defines how automation applies the mandatory review policy from [`CODE_REVIEW.md`](CODE_REVIEW.md). `CODE_REVIEW.md` remains authoritative for review content; this file defines only the orchestration around that review.

## 1. Review is mandatory

Every code-changing ticket requires a complete final review against `CODE_REVIEW.md` before merge. Documentation-only changes still require a scope/consistency review appropriate to the changed documentation.

A green CI result is not a substitute for review.

## 2. Exact-head rule

Review evidence is valid only for the exact PR head inspected by the reviewer.

If source, tests, build files or workflows change after review, the previous approval is stale. Re-run applicable CI and review the replacement exact head.

Pure project-state bookkeeping that does not affect executable behavior follows the exception documented in `AGENTS.md` / `CI_POLICY.md`.

## 3. Independent reviewer

The final approving reviewer must be independent from the worker that produced or modified the exact candidate head.

The reviewer must re-fetch:

- the live issue and acceptance criteria;
- current PR head and base;
- complete diff / affected files;
- exact-head checks;
- prior reviews and unresolved threads;
- current `main` composition when relevant.

Then perform the complete applicable `CODE_REVIEW.md` pass.

## 4. Findings

Collect the complete current set of Blocking and Important findings before beginning corrections. Do not create a remote fix/push cycle for each individual finding when the findings can be corrected coherently together.

Review outcomes:

- **PASS** — zero unresolved Blocking / Important findings on the exact current head.
- **FIX REQUIRED** — bounded concrete defects can be corrected without product-scope redesign.
- **BLOCKED** — a product/API/architecture decision, missing dependency, external validation or broader redesign is required.

## 5. Reviewer-owned corrections

A reviewer may directly apply a bounded, unambiguous correction when that reduces handoff latency.

When the reviewer changes the head:

1. apply one coherent correction batch;
2. run required local validation;
3. publish one replacement qualification head when practical;
4. re-run exact-head CI;
5. the correcting reviewer is no longer eligible to provide the final approval for that head;
6. a different independent reviewer must inspect and approve the replacement head.

Do not use reviewer-owned correction for ambiguous product decisions, broad redesigns or scope expansion.

## 6. Merge gate

Automation may merge only when:

- acceptance criteria are complete;
- required exact-head CI is green;
- the complete `CODE_REVIEW.md` record exists;
- independent final review is PASS on the current exact head;
- no Blocking / Important findings remain;
- no blocking review thread remains unresolved;
- composition with current `main` is valid;
- required `CONTEXT.md` / `ROADMAP.md` completion bookkeeping is ready;
- privacy review is complete and no personal information has been introduced.

When the gate becomes fully green, merge without waiting for any Scheduler or Reporter cycle.

## 7. Serialized automation interaction

The current automation model has one source-changing implementation lane.

While a frozen candidate is under independent review, no automatic fallback source ticket is started. If the review requests changes, the same ticket remains the active implementation assignment until the replacement exact head is reviewed and merged or the ticket becomes explicitly blocked.

This prevents review latency from turning into branch proliferation and stale-head churn.