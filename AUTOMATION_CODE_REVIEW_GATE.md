# NativeUI automated code-review gate

This document is the mandatory automation-specific code-review gate for autonomous NativeUI delivery. It supplements `AGENTS.md`, `CODE_REVIEW.md`, `CI_POLICY.md`, `AUTOMATION.md`, and `AUTOMATION_STATUS.md`; it never weakens them.

`AUTOMATION_STATUS.md` is authoritative for phase names, review-lock semantics, event kinds, Draft/Ready semantics and source-lane accounting. This file defines what constitutes a valid independent code review and when reviewer-owned correction is allowed.

## 1. Mandatory review path

Every code-changing PR must reach a head-valid independent peer review before final qualification:

```text
SOURCE_READY
  -> PEER_CODE_REVIEW
      -> REVIEW_PASS
      -> REVIEW_FIX_REQUIRED -> REVIEW_FIX -> CI_WAIT -> SECOND_PEER_CODE_REVIEW
      -> REVIEW_BLOCKED -> REWORK_REQUIRED -> SOURCE -> ... -> SOURCE_READY
  -> FINAL_QUALIFICATION
  -> MERGE_READY
  -> MERGED
```

No code-changing PR may go directly from `SOURCE_READY` or CI green to final qualification/merge.

## 2. Independence rule

The reviewer that issues `REVIEW_PASS` for a head must be a different Delivery worker from the worker that authored/modified that exact head.

- The implementation worker's self-review is mandatory but does not satisfy peer review.
- Integration verification is mandatory but does not satisfy peer review.
- A reviewer that applies `REVIEW_FIX` becomes an author of the resulting new head and cannot certify that new head.
- A second Delivery worker must independently review a review-fix head.
- If that second reviewer applies another fix, a different worker must review the next head.

The GitHub connector may use the same repository account for all automation activity. Independence is therefore recorded by automation worker role/current-head authorship, not GitHub username alone.

## 3. Review lock

`PEER_CODE_REVIEW` establishes the review lock defined by `AUTOMATION_STATUS.md`.

- The ordinary source worker must not push while the review lock is active.
- The reviewer is read-only unless explicitly assigned `REVIEW_FIX`.
- `REVIEW_FIX` keeps the same review lock and is the only permitted source-edit path during that correction batch.
- `REVIEW_FIX` counts as a source-changing lane.
- After a review-fix head is published, the PR remains frozen through CI and `SECOND_PEER_CODE_REVIEW`.
- `REVIEW_BLOCKED` returns source ownership only through an explicit Scheduler `REWORK_REQUIRED` route.
- `REVIEW_PASS` keeps ordinary source edits frozen and hands control to Integration/final qualification.

A concurrent source write during an active review lock invalidates downstream review state and is a P0 orchestration defect.

## 4. Exact-head review procedure

Before issuing a verdict, the reviewer must re-fetch and record:

- PR number;
- exact PR head SHA;
- observed target/base SHA;
- current-head author/source worker when known;
- linked issue, acceptance criteria, required tests and non-goals;
- complete changed-file list and full diff/patch;
- relevant surrounding implementation;
- existing reviews and unresolved threads;
- applicable exact-head CI evidence;
- current review-lock state.

The reviewer must read current repository versions of:

1. `AGENTS.md`;
2. `CODE_REVIEW.md`;
3. `CI_POLICY.md`;
4. `AUTOMATION.md`;
5. `AUTOMATION_STATUS.md`;
6. this document;
7. the product issue/comments;
8. architecture/validation docs required by the ticket.

The following are not sufficient review evidence:

- trusting the PR description/checklist without inspecting implementation;
- restating green CI results;
- checking only metadata/docs;
- copying the source worker's self-review;
- copying an older head review;
- emitting `REVIEW_PASS` without evaluating applicable `CODE_REVIEW.md` failure domains.

## 5. Durable review record

The reviewer submits a GitHub pull-request review for the exact head. If `APPROVE` is unavailable because the connector identity is also the PR author, a formal `COMMENT` review is acceptable; a top-level PR comment is not a substitute.

The review begins with:

```text
<!-- nativeui-peer-code-review:v1 -->
```

and includes at least:

```yaml
reviewer_worker: W1|W2|W3|W4
source_worker: W1|W2|W3|W4|unknown
reviewed_head: <exact PR head SHA>
reviewed_base: <observed base/main SHA>
verdict: REVIEW_PASS|REVIEW_FIX_REQUIRED|REVIEW_BLOCKED
blocking_findings: <integer>
important_findings: <integer>
```

The body then contains the complete applicable record required by `CODE_REVIEW.md`, including:

- instance isolation;
- globals/statics;
- threading/real-time boundaries;
- lifetime/reentrancy and owner destruction;
- transactional prepare/commit/recovery;
- scheduling/queue rejection/capacity/exception-before-enqueue when applicable;
- exception/unwind, `noexcept`, destructor and foreign-ABI behavior;
- partial construction/resource cleanup when applicable;
- Objective-C runtime rules when applicable;
- platform integration;
- performance/allocation impact;
- privacy;
- acceptance/completeness mapping;
- exact tests/fault seams/CI evidence;
- remaining findings classified Blocking/Important/Advisory.

Inline comments should be used for precise findings when useful.

## 6. Verdict rules

### REVIEW_PASS

Legal only when:

- reviewed head equals current PR head;
- reviewer did not author/modify current head;
- complete applicable `CODE_REVIEW.md` record exists;
- acceptance/required tests/non-goals and touched failure domains were inspected;
- Blocking = 0;
- Important = 0;
- no unresolved thread represents a Blocking/Important defect.

### REVIEW_FIX_REQUIRED

Use only when one or more Blocking/Important findings are bounded, unambiguous and safely correctable by the reviewer under section 7.

The old head does not pass. The reviewer moves to explicitly assigned `REVIEW_FIX` while retaining the review lock.

### REVIEW_BLOCKED

Use when a finding cannot safely be corrected in reviewer-owned bounded mode. Record durable findings and route `REWORK_REQUIRED` back to the source worker through the Scheduler.

Advisory findings may remain only when they do not hide correctness, lifecycle, performance, API, platform, privacy or test-completeness risk.

## 7. Reviewer-owned correction (`REVIEW_FIX`)

The reviewer may immediately correct findings to reduce latency only when all of these are true:

- intended behavior is unambiguous from issue/tests/architecture/project policy;
- correction stays within existing ticket/public contract;
- no product/design decision is required;
- no broad architecture redesign/dependency change/ticket decomposition/new feature is required;
- regression test can be added/strengthened first when behavior changes;
- current PR head and review lock are re-fetched before every write;
- no concurrent worker changed the branch;
- correction can be delivered as one coherent review-fix batch.

Typical suitable fixes:

- missing error/recovery handling with documented expected behavior;
- missing lifetime/reentrancy guard;
- missing bounds/null/stale-handle check;
- incorrect state restoration after exception;
- missing regression/fault test for an already-defined invariant;
- small explicit API/implementation mismatch;
- warning/build portability fix that does not alter intended product behavior.

Reviewer-owned correction is forbidden for:

- ambiguous product/API behavior;
- meaningful architectural redesign;
- scope expansion/new feature;
- broad cross-subsystem refactor that cannot be bounded safely;
- branch conflict/concurrent source work;
- any change that weakens tests, acceptance, CI or review independence.

The reviewer-fix worker must:

1. keep the old-head finding/review durable;
2. add/adjust regression tests before production correction when applicable;
3. apply one coherent correction batch;
4. publish one new exact head;
5. emit `REVIEW_FIX_APPLIED` with old head/new head/findings/tests;
6. never emit `REVIEW_PASS` for that new head;
7. leave the PR frozen for CI and `SECOND_PEER_CODE_REVIEW` by another worker.

## 8. Review invalidation

Review verdicts are bound to `reviewed_head`.

Any source/test/example/build/dependency/generated/platform/workflow commit invalidates the previous `REVIEW_PASS`, including a main synchronization commit that changes the PR head.

Pure project-state completion documentation may be non-invalidating only when Integration explicitly verifies the `CI_POLICY.md` classification.

A stale review or stale event can never authorize final qualification or merge.

## 9. Scheduler review service requirements

After head-valid code-changing `SOURCE_READY`:

- assign `PEER_CODE_REVIEW` in the same Scheduler generation to another worker;
- reserve reviewer capacity before opening new source fallback work;
- if all workers would source-edit, schedule 3 source + 1 reviewer;
- if reviewer enters `REVIEW_FIX`, count it as the fourth source lane;
- after `REVIEW_FIX_APPLIED`, assign `SECOND_PEER_CODE_REVIEW` to a different worker before any new source fallback consumes that capacity;
- review starvation is a P0 orchestration defect.

Target service rule: review assignment in the same Scheduler cycle, execution on the assigned worker's next run.

## 10. Integration merge gate

Immediately before Draft -> Ready, `MERGE_READY`, and merge, Integration re-fetches live PR/head/base/reviews/threads/checks and proves:

- durable `<!-- nativeui-peer-code-review:v1 -->` exists;
- `reviewed_head` equals current exact head;
- PASS reviewer did not author/modify current head;
- verdict is `REVIEW_PASS`;
- Blocking = 0 and Important = 0;
- no later Blocking/Important finding exists;
- any prior `REVIEW_FIX_APPLIED` was followed by independent review of its resulting head;
- implementation self-review/completeness evidence exists;
- exact-head CI/final qualification requirements are satisfied;
- acceptance/required tests are complete;
- current base composition remains valid;
- PR is mergeable.

If any term is missing/stale/ambiguous, merge is forbidden.

Integration's own final verification is defense in depth and never replaces the peer review.

## 11. Merge invariant

```text
MERGE_ALLOWED =
    current_head_is_frozen
 && exact_head_required_ci_green
 && peer_review_exact_head_pass
 && reviewer_did_not_author_current_head
 && blocking_findings == 0
 && important_findings == 0
 && acceptance_complete
 && required_tests_complete
 && final_qualification_complete
 && base_composition_valid
 && mergeable
```

No personal information may be introduced in code, tests, examples, generated metadata, tickets, PRs, reviews, commits, documentation or control-plane records.