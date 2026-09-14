# NativeUI automated code-review gate

This document is the mandatory automation-specific review gate for autonomous NativeUI delivery. It supplements `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, and `AUTOMATION.md`; it never weakens them.

The purpose of this gate is to prevent a code-changing pull request from being merged merely because CI is green or because the implementation worker wrote a self-review summary, while also avoiding unnecessary review -> source-worker -> review latency for bounded corrections.

## 1. Mandatory state transition

Every code-changing product PR must pass through this state machine:

```text
SOURCE_READY
  -> PEER_CODE_REVIEW
  -> REVIEW_PASS
  -> FINAL_QUALIFICATION
  -> MERGE_READY
  -> MERGED
```

When review finds a bounded actionable defect, the optimized path is:

```text
PEER_CODE_REVIEW
  -> REVIEW_FIX
  -> new exact head
  -> applicable CI qualification
  -> SECOND_PEER_CODE_REVIEW
  -> REVIEW_PASS
```

`PEER_CODE_REVIEW` is mandatory and cannot be skipped.

A PR with no head-valid peer code-review record is not eligible for Draft -> Ready final qualification, `MERGE_READY`, auto-merge, or direct merge.

Documentation-only/project-state-only changes may use the documentation review rules from `AGENTS.md`; this file is specifically a hard gate for code, tests, examples, build/dependency files, platform code, generated code, or workflow changes that can affect executable behavior or qualification.

## 2. Reviewer independence

The peer reviewer must be a different automation worker role from the worker that produced the source candidate.

For example, if W2 produced the exact candidate, W1, W3, or W4 may perform the mandatory peer review. The Integration worker verifies and enforces the gate but does not satisfy the peer-review requirement by merely repeating the implementation worker's self-review.

The GitHub connector may submit all automation activity through the same repository account. Therefore independence is determined by the recorded automation worker role and assignment, not by GitHub username alone.

The implementation worker's own `CODE_REVIEW.md` self-pass remains mandatory before `SOURCE_READY`, but it is not the peer review.

A worker that modifies the PR while acting as reviewer becomes a **review-fix author** for the resulting head and cannot provide the final independent `REVIEW_PASS` for that new head. A second Delivery worker, different from both the original source worker when practical and always different from the review-fix author, must independently review the resulting exact head.

## 3. Exact-head review procedure

The peer reviewer must re-fetch and record:

- PR number;
- exact PR head SHA;
- observed target/base SHA;
- source worker role when known;
- linked product issue and acceptance criteria;
- complete changed-file list and full diff/patch for the exact candidate;
- current reviews and unresolved review threads;
- applicable exact-head CI evidence.

Before issuing a verdict, the reviewer must read the current repository versions of:

1. `AGENTS.md`;
2. `CODE_REVIEW.md`;
3. `CI_POLICY.md`;
4. `AUTOMATION.md`;
5. this document;
6. the product issue and its current comments;
7. architecture/validation documents required by the ticket.

The reviewer must inspect the actual diff and affected surrounding code. The following are explicitly insufficient as a code review:

- trusting the PR description or acceptance checklist without inspecting the implementation;
- restating green CI results;
- checking only metadata/status/docs;
- copying the implementation worker's self-review;
- writing `REVIEW_PASS` without evaluating every applicable `CODE_REVIEW.md` failure domain.

## 4. Required review record

The peer reviewer must submit one durable GitHub pull-request review for the exact reviewed head. When GitHub cannot accept `APPROVE` because the connector identity is also the PR author, a formal `COMMENT` review is acceptable, but it must contain the complete structured review record below.

The review body starts with:

```text
<!-- nativeui-peer-code-review:v1 -->
```

and records at least:

```yaml
reviewer_worker: W1|W2|W3|W4
source_worker: W1|W2|W3|W4|unknown
reviewed_head: <exact PR head SHA>
reviewed_base: <observed base/main SHA>
verdict: REVIEW_PASS|REVIEW_BLOCKED|REVIEW_FIX_REQUIRED
blocking_findings: <integer>
important_findings: <integer>
```

It must then contain the complete applicable review record required by `CODE_REVIEW.md`, including:

- instance isolation;
- globals/statics;
- threading/real-time boundaries;
- lifetime/reentrancy and owner destruction;
- transactional prepare/commit/recovery;
- scheduling/queue capacity/rejection/exception-before-enqueue where applicable;
- exception/unwind, `noexcept`, destructor and foreign-ABI behavior;
- partial construction/resource cleanup where applicable;
- Objective-C runtime rules where applicable;
- platform integration;
- performance/allocation impact;
- privacy;
- acceptance/completeness mapping;
- exact tests/fault seams/CI evidence;
- remaining findings classified as Blocking, Important, or Advisory.

Inline review comments should be used for precise findings when useful. Findings must refer to code, behavior, tests, or contract evidence rather than vague style preferences.

## 5. Verdict rules

`REVIEW_PASS` is legal only when:

- the review was performed on the current exact PR head;
- the reviewer is a different worker role from the source/review-fix author of that head;
- the full applicable `CODE_REVIEW.md` record is present;
- acceptance criteria, required tests, non-goals and touched failure domains were inspected against the implementation;
- `blocking_findings == 0`;
- `important_findings == 0`;
- no unresolved review thread represents a Blocking or Important defect.

Any Blocking or Important finding requires either:

- `REVIEW_FIX_REQUIRED` when the reviewer can safely apply a bounded correction under section 8; or
- `REVIEW_BLOCKED` plus a durable PR finding and `REWORK_REQUIRED` when the correction is not suitable for reviewer-owned fixing.

Advisory findings may remain only when they do not conceal correctness, lifecycle, performance, API, platform, privacy, or test-completeness risk.

## 6. Review invalidation

The peer review is bound to `reviewed_head`.

Any subsequent source, test, example, build, dependency, generated-code, platform, or workflow commit invalidates the prior `REVIEW_PASS`, including a merge/synchronization commit that changes the PR head. The resulting exact head requires a fresh peer code review.

Pure completion/project-state documentation that does not change executable/API/build/qualification behavior may follow the non-invalidating rules in `CI_POLICY.md`, but Integration must explicitly verify that classification before reusing a peer review.

A stale review event or review body can never authorize qualification or merge.

## 7. Scheduler requirements

After a head-valid `SOURCE_READY`, the Scheduler must explicitly assign `PEER_CODE_REVIEW` to a Delivery worker different from the source worker before placing the PR in a merge/qualification path.

Peer review does not consume a source-changing lane, but it **does consume worker execution capacity**. The Scheduler must reserve that capacity explicitly rather than assuming a reviewer will eventually become idle.

A ready mandatory peer review has scheduling priority over starting a new source slice, fallback implementation, documentation fallback, or optional preflight.

When a code-changing `SOURCE_READY` candidate has no valid peer review:

1. assign `PEER_CODE_REVIEW` as a Delivery worker's **primary** action in the same scheduling generation;
2. prefer a worker already waiting on CI/review/final qualification;
3. if every Delivery worker is actively source-changing, allow the lowest-priority/lowest-critical-path worker to finish only its current coherent batch, then switch it to peer review before starting another source batch;
4. while the mandatory review remains unassigned or unstarted, do not open an additional source lane or source fallback;
5. when all four Delivery workers would otherwise be source-changing, the effective capacity becomes **three source lanes plus one reviewer** until the review reaches `REVIEW_PASS`, `REVIEW_FIX_REQUIRED`, or `REVIEW_BLOCKED`;
6. lack of reviewer capacity for a ready peer review is an orchestration defect, not a valid reason to delay the review indefinitely.

The target service rule is: `SOURCE_READY -> peer-review assignment` in the same Scheduler cycle, followed by execution on the assigned reviewer's next run.

When `REVIEW_FIX_REQUIRED` is emitted and the finding qualifies for section 8, keep the same reviewer assigned in `REVIEW_FIX` mode so the correction can happen immediately rather than round-tripping to the original source worker.

As soon as the reviewer publishes a corrected new head, schedule `SECOND_PEER_CODE_REVIEW` on a different Delivery worker. This second review has priority over starting a new source slice. The worker that performed the correction cannot certify its own resulting head.

After the verdict, the reviewer may resume its source primary/fallback according to the next Scheduler plan.

The Scheduler must treat a PR awaiting mandatory peer review as `review-closeout`, not `merge-ready`.

The Scheduler must not emit or accept an `on_unlock` assumption that depends on a future merge unless the required peer-review phase is either already complete on the exact head or explicitly scheduled before merge. An `on_unlock` reservation must never consume capacity needed by an already-ready mandatory peer review.

A `REVIEW_PASS` worker event is valid only when it references the exact head and a durable PR review containing `<!-- nativeui-peer-code-review:v1 -->`.

## 8. Reviewer-owned correction mode (`REVIEW_FIX`)

A peer reviewer may immediately correct findings on the reviewed PR to reduce latency, but only under this bounded mode.

`REVIEW_FIX` is allowed when all of the following are true:

- the defect is directly evidenced by the review and its intended behavior is unambiguous from the issue, tests, existing architecture and `CODE_REVIEW.md`;
- the correction stays within the existing ticket scope and public contract;
- no product/design decision is required;
- the correction does not require broad architecture redesign, dependency changes, ticket decomposition, or a new feature;
- the reviewer can add or strengthen a deterministic regression test first when behavior changes;
- the current PR head is re-fetched immediately before every write;
- no conflicting worker has changed the branch;
- the correction is one coherent review-fix batch, not micro-commit churn.

Typical suitable fixes include:

- missing error/recovery handling with an obvious documented contract;
- missing lifetime/reentrancy guard;
- missing bounds/null/stale-handle check;
- wrong state restoration on exception;
- missing regression/fault test for an already-defined invariant;
- small API/implementation mismatch where the issue contract is explicit;
- warning/build portability correction that does not alter intended product behavior.

`REVIEW_FIX` is forbidden when the finding requires:

- a new product/API decision or ambiguous behavior choice;
- meaningful architectural redesign;
- a scope expansion or new ticket family;
- large cross-subsystem refactoring whose correctness cannot be bounded in the review run;
- resolution of a conflict with concurrent source work;
- bypassing or weakening acceptance criteria, tests, CI, or review independence.

When correction is not safe under these rules, use `REVIEW_BLOCKED` / `REWORK_REQUIRED` and return ownership to the source worker.

A reviewer applying a fix must:

1. leave the original finding/review durable on the old head;
2. add/adjust regression tests before the production correction when applicable;
3. apply one coherent correction batch;
4. publish a new exact head;
5. emit `REVIEW_FIX_APPLIED` with old head, new head, findings corrected and tests changed;
6. never emit `REVIEW_PASS` for that new head;
7. hand off immediately to `SECOND_PEER_CODE_REVIEW` by another Delivery worker.

The review-fix author may perform a self-check of its correction, but that self-check is not the independent final peer review.

## 9. Review / correction loop

The optimized loop is:

```text
PEER_CODE_REVIEW
  -> REVIEW_PASS
```

or, for bounded findings:

```text
PEER_CODE_REVIEW
  -> REVIEW_FIX_REQUIRED
  -> REVIEW_FIX
  -> corrected new exact head
  -> applicable CI qualification
  -> SECOND_PEER_CODE_REVIEW
  -> REVIEW_PASS | REVIEW_FIX_REQUIRED | REVIEW_BLOCKED
```

If a second reviewer finds another bounded issue, it may itself become the next `REVIEW_FIX` author; the resulting head must then be reviewed by a different worker again. No worker may certify a head it modified.

For non-bounded findings:

```text
PEER_CODE_REVIEW
  -> REVIEW_BLOCKED
  -> REWORK_REQUIRED to source worker
  -> correction in TDD
  -> new exact head
  -> applicable CI qualification
  -> SOURCE_READY
  -> NEW PEER_CODE_REVIEW
```

The previous review never carries forward across a changed executable head. Repeat until the current exact head has `REVIEW_PASS` with zero Blocking and zero Important findings.

## 10. Integration merge gate

Integration must re-fetch the PR immediately before Draft -> Ready, `MERGE_READY`, and merge.

For every code-changing PR, Integration must verify all of the following:

- a durable `<!-- nativeui-peer-code-review:v1 -->` review exists;
- its `reviewed_head` equals the current exact PR head;
- its reviewer worker differs from the worker that authored the current head's last review-fix/source batch;
- verdict is `REVIEW_PASS`;
- Blocking = 0 and Important = 0;
- no later review/thread introduced a Blocking/Important finding;
- any preceding `REVIEW_FIX_APPLIED` was followed by an independent review of the resulting head;
- the implementation worker's self-review/completeness evidence exists;
- exact-head CI/final qualification requirements are satisfied;
- acceptance criteria and required tests are complete;
- current base/main composition remains valid under `CI_POLICY.md`.

If any item is missing, Integration must not merge. It must emit `REVIEW_BLOCKED`/`BLOCKED` or route `REWORK_REQUIRED` as appropriate.

Integration's own final verification is an additional defense-in-depth pass. It does not replace the mandatory peer review.

## 11. Merge invariant

For code-changing PRs, the invariant is:

```text
MERGE_ALLOWED =
    exact_head_ci_green
 && peer_review_exact_head_pass
 && reviewer_did_not_author_current_head
 && blocking_findings == 0
 && important_findings == 0
 && acceptance_complete
 && final_qualification_complete
 && mergeable
```

If the automation cannot prove every term from live GitHub evidence, merge is forbidden.

No personal information may be introduced in code, tests, examples, generated metadata, tickets, reviews, or automation records.