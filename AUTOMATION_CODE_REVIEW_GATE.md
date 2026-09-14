# NativeUI automated code-review gate

This document is the mandatory automation-specific review gate for autonomous NativeUI delivery. It supplements `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, and `AUTOMATION.md`; it never weakens them.

The purpose of this gate is to prevent a code-changing pull request from being merged merely because CI is green or because the implementation worker wrote a self-review summary.

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

`PEER_CODE_REVIEW` is mandatory and cannot be skipped.

A PR with no head-valid peer code-review record is not eligible for Draft -> Ready final qualification, `MERGE_READY`, auto-merge, or direct merge.

Documentation-only/project-state-only changes may use the documentation review rules from `AGENTS.md`; this file is specifically a hard gate for code, tests, examples, build/dependency files, platform code, generated code, or workflow changes that can affect executable behavior or qualification.

## 2. Reviewer independence

The peer reviewer must be a different automation worker role from the worker that produced the source candidate.

For example, if W2 produced the exact candidate, W1, W3, or W4 may perform the mandatory peer review. The Integration worker verifies and enforces the gate but does not satisfy the peer-review requirement by merely repeating the implementation worker's self-review.

The GitHub connector may submit all automation activity through the same repository account. Therefore independence is determined by the recorded automation worker role and assignment, not by GitHub username alone.

The implementation worker's own `CODE_REVIEW.md` self-pass remains mandatory before `SOURCE_READY`, but it is not the peer review.

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
verdict: REVIEW_PASS|REVIEW_BLOCKED
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
- the reviewer is a different worker role from the source worker;
- the full applicable `CODE_REVIEW.md` record is present;
- acceptance criteria, required tests, non-goals and touched failure domains were inspected against the implementation;
- `blocking_findings == 0`;
- `important_findings == 0`;
- no unresolved review thread represents a Blocking or Important defect.

Any Blocking or Important finding requires `REVIEW_BLOCKED` plus a durable PR finding and a worker event that routes the PR back to the implementation worker as `REWORK_REQUIRED`.

Advisory findings may remain only when they do not conceal correctness, lifecycle, performance, API, platform, privacy, or test-completeness risk.

## 6. Review invalidation

The peer review is bound to `reviewed_head`.

Any subsequent source, test, example, build, dependency, generated-code, platform, or workflow commit invalidates the prior `REVIEW_PASS`, including a merge/synchronization commit that changes the PR head. The resulting exact head requires a fresh peer code review.

Pure completion/project-state documentation that does not change executable/API/build/qualification behavior may follow the non-invalidating rules in `CI_POLICY.md`, but Integration must explicitly verify that classification before reusing a peer review.

A stale review event or review body can never authorize qualification or merge.

## 7. Scheduler requirements

After a head-valid `SOURCE_READY`, the Scheduler must explicitly assign `PEER_CODE_REVIEW` to a Delivery worker different from the source worker before placing the PR in a merge/qualification path.

Peer review does not consume a source-changing lane.

The Scheduler must treat a PR awaiting mandatory peer review as `review-closeout`, not `merge-ready`.

The Scheduler must not emit or accept an `on_unlock` assumption that depends on a future merge unless the required peer-review phase is either already complete on the exact head or explicitly present before merge in the Integration queue.

A `REVIEW_PASS` worker event is valid only when it references the exact head and a durable PR review containing `<!-- nativeui-peer-code-review:v1 -->`.

## 8. Delivery peer-review requirements

When W1/W2/W3/W4 is assigned `PEER_CODE_REVIEW`, that review is the worker's primary useful action for the PR.

The reviewer must:

1. inspect the exact diff and relevant surrounding implementation;
2. apply `CODE_REVIEW.md` comprehensively rather than sampling only the obvious code path;
3. inspect acceptance/test completeness and failure recovery;
4. submit the structured PR review;
5. emit `REVIEW_PASS` or `REVIEW_BLOCKED` for the exact head.

A peer-review assignment is read-only with respect to the reviewed PR source unless the Scheduler explicitly assigns a source handoff. The reviewer must not silently fix findings on another worker's branch.

## 9. Integration merge gate

Integration must re-fetch the PR immediately before Draft -> Ready, `MERGE_READY`, and merge.

For every code-changing PR, Integration must verify all of the following:

- a durable `<!-- nativeui-peer-code-review:v1 -->` review exists;
- its `reviewed_head` equals the current exact PR head;
- its reviewer worker differs from the source worker;
- verdict is `REVIEW_PASS`;
- Blocking = 0 and Important = 0;
- no later review/thread introduced a Blocking/Important finding;
- the implementation worker's self-review/completeness evidence exists;
- exact-head CI/final qualification requirements are satisfied;
- acceptance criteria and required tests are complete;
- current base/main composition remains valid under `CI_POLICY.md`.

If any item is missing, Integration must not merge. It must emit `REVIEW_BLOCKED`/`BLOCKED` or route `REWORK_REQUIRED` as appropriate.

Integration's own final verification is an additional defense-in-depth pass. It does not replace the mandatory peer review.

## 10. Merge invariant

For code-changing PRs, the invariant is:

```text
MERGE_ALLOWED =
    exact_head_ci_green
 && peer_review_exact_head_pass
 && blocking_findings == 0
 && important_findings == 0
 && acceptance_complete
 && final_qualification_complete
 && mergeable
```

If the automation cannot prove every term from live GitHub evidence, merge is forbidden.

No personal information may be introduced in code, tests, examples, generated metadata, tickets, reviews, or automation records.