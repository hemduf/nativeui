# NativeUI automation status model

This document is the canonical status/state-machine reference for NativeUI autonomous delivery. It supplements `AGENTS.md`, `AUTOMATION.md`, `AUTOMATION_CODE_REVIEW_GATE.md`, `CI_POLICY.md`, and `CODE_REVIEW.md`.

The main purpose is to prevent product status, PR phase, CI state, review state, worker assignment and merge readiness from being conflated.

## 1. Product issue status — only four delivered-work states

Product issues use only these status states in the issue body and `status:*` labels:

| Product state | GitHub issue state | Required label | Meaning |
|---|---|---|---|
| `Ready` | open | `status:ready` | dependencies satisfied and legal to claim |
| `Doing` | open | `status:doing` | leased by active implementation/review/qualification work |
| `Blocked` | open | `status:blocked` | explicit dependency or genuine external blocker prevents progress |
| `Done` | closed/completed | `status:done` | Definition of Done satisfied and merged/completed |

A closed `not_planned` or duplicate issue is terminal non-delivery. It is not `Done` and does not satisfy a dependency unless that dependency is explicitly removed or replaced.

Do **not** create product labels such as `status:ci-wait`, `status:review`, `status:source-ready`, `status:merge-ready`, or `status:review-fix`. Those are execution phases and belong in the control plane / PR evidence.

## 2. PR delivery phase — control-plane state, not product status

A product issue normally remains `Doing` while its canonical PR moves through these phases:

```text
SOURCE
  -> CI_WAIT
  -> SOURCE_READY
  -> PEER_CODE_REVIEW
      -> REVIEW_PASS
      -> REVIEW_FIX_REQUIRED -> REVIEW_FIX -> CI_WAIT -> SECOND_PEER_CODE_REVIEW
      -> REVIEW_BLOCKED -> REWORK_REQUIRED -> SOURCE -> CI_WAIT -> SOURCE_READY
  -> FINAL_QUALIFICATION
  -> MERGE_READY
  -> MERGED
  -> product issue Done
```

The canonical phase definitions are:

| Phase | Meaning | May change source? | Counts as source lane? | Exit condition |
|---|---|---:|---:|---|
| `SOURCE` | implementation/TDD batch active | yes | yes | coherent head published |
| `CI_WAIT` | exact-head applicable qualification running/queued | no | no | terminal results classified |
| `SOURCE_READY` | implementation scope complete, self-review/completeness complete, normal/path qualification sufficient for peer review | no | no | reviewer assigned |
| `PEER_CODE_REVIEW` | independent diff/code review of exact head | no | no | PASS, FIX_REQUIRED or BLOCKED |
| `REVIEW_FIX_REQUIRED` | reviewer found bounded unambiguous actionable defect suitable for immediate correction | no by itself | no | same reviewer enters REVIEW_FIX |
| `REVIEW_FIX` | reviewer applies one bounded TDD correction batch | yes | **yes** | new exact head published |
| `SECOND_PEER_CODE_REVIEW` | another worker independently reviews the review-fix head | no | no | PASS/FIX/BLOCKED |
| `REVIEW_BLOCKED` | finding is not safely reviewer-fixable | no | no | REWORK_REQUIRED routed to source worker |
| `REWORK_REQUIRED` | source ownership resumes for non-bounded correction | yes once SOURCE resumes | yes once editing begins | corrected head qualified and SOURCE_READY again |
| `REVIEW_PASS` | exact current head has independent complete review, 0 Blocking/0 Important | no | no | Integration final qualification |
| `FINAL_QUALIFICATION` | frozen candidate executes final gates required by CI_POLICY | no | no | all required executed gates green |
| `MERGE_READY` | all merge invariant terms proven on live exact head/base | no | no | Integration merges exact expected head |
| `MERGED` | PR verified merged | no | no | issue/docs/unlocks synchronized |

`SOURCE_READY`, `REVIEW_PASS`, `FINAL_QUALIFICATION`, and `MERGE_READY` are all exact-head scoped. A relevant head change invalidates every downstream phase that depended on the previous head.

## 3. GitHub PR Draft / Ready semantics

PR draft state is distinct from product status and control-plane phase.

- Keep the PR **Draft** during SOURCE, CI_WAIT, SOURCE_READY, PEER_CODE_REVIEW, REVIEW_FIX, SECOND_PEER_CODE_REVIEW and REVIEW_BLOCKED/REWORK_REQUIRED.
- Transition Draft -> **Ready for review** only after a head-valid `REVIEW_PASS` and the normal/path preconditions from `CI_POLICY.md` are satisfied.
- The Draft -> Ready transition starts the heavyweight final-candidate qualification where configured.
- If executable/source/test/build/workflow changes occur after Ready/final qualification, return the PR to Draft before editing and restart the required review/qualification chain.
- Pure project-state completion documentation follows the non-invalidating classification in `CI_POLICY.md` only after Integration explicitly verifies it.

## 4. Review ownership and review lock

A PR in `PEER_CODE_REVIEW`, `REVIEW_FIX`, `SECOND_PEER_CODE_REVIEW`, `REVIEW_PASS` awaiting qualification, or `FINAL_QUALIFICATION` is **frozen against ordinary source edits**.

The control plane records:

```yaml
source_worker: W1|W2|W3|W4
review_lock_worker: W1|W2|W3|W4|null
review_fix_worker: W1|W2|W3|W4|null
peer_reviewer: W1|W2|W3|W4|null
reviewed_head: <sha|null>
phase: <canonical phase>
```

Rules:

1. Entering `PEER_CODE_REVIEW` assigns `review_lock_worker` to the reviewer. The source worker must not push to that PR while the lock is active.
2. If the reviewer chooses `REVIEW_FIX`, it keeps the lock and is the only worker allowed to source-edit that PR for one coherent correction batch.
3. `REVIEW_FIX` counts as a source-changing lane. With three other active source lanes, total remains four.
4. Publishing the review-fix head records `review_fix_worker`, clears the first review verdict for merge purposes, and moves to CI_WAIT then SECOND_PEER_CODE_REVIEW.
5. SECOND_PEER_CODE_REVIEW must be performed by a worker different from the worker that authored the current head.
6. No worker may emit the final `REVIEW_PASS` for a head it modified.
7. If the second reviewer performs another REVIEW_FIX, that worker becomes the current-head author and a different worker must review again.
8. `REVIEW_BLOCKED` releases the review lock only when the Scheduler explicitly routes `REWORK_REQUIRED` back to the source worker.
9. `REVIEW_PASS` transfers control to Integration; ordinary source editing remains frozen.

Any source write that races an active review lock is an orchestration defect. The write must not be silently accepted as preserving review validity.

## 5. Canonical worker event kinds

The worker event bus may use these kinds:

### Source / implementation
- `CLAIMED`
- `PROGRESS`
- `PREFLIGHT_READY`
- `CI_WAIT`
- `CI_FAILED_PRODUCT`
- `CI_FAILED_INFRA`
- `SOURCE_READY`

### Review
- `REVIEW_PASS`
- `REVIEW_FIX_REQUIRED`
- `REVIEW_FIX_APPLIED`
- `REVIEW_BLOCKED`
- `REWORK_REQUIRED`

### Final qualification / merge
- `QUALIFICATION_WAIT`
- `QUALIFICATION_FAILED_PRODUCT`
- `QUALIFICATION_FAILED_INFRA`
- `MERGE_READY`
- `MERGED`

### Coordination / blockers
- `BLOCKED`
- `HANDOFF_REQUEST`

Assignment modes such as `PEER_CODE_REVIEW`, `REVIEW_FIX`, and `SECOND_PEER_CODE_REVIEW` are Scheduler modes, not event kinds by themselves. Their meaningful results are emitted through the event kinds above.

Every event referring to a PR/review/qualification includes the exact `head`. Review/qualification/merge events also include observed `base` when composition matters.

## 6. Canonical review records

The durable review record is a GitHub pull-request review containing:

```text
<!-- nativeui-peer-code-review:v1 -->
```

and at least:

```yaml
reviewer_worker: W1|W2|W3|W4
source_worker: W1|W2|W3|W4|unknown
reviewed_head: <sha>
reviewed_base: <sha>
verdict: REVIEW_PASS|REVIEW_BLOCKED|REVIEW_FIX_REQUIRED
blocking_findings: <integer>
important_findings: <integer>
```

The complete applicable `CODE_REVIEW.md` record must follow. Green CI, a PR checklist, an Integration summary, or the source worker's self-review is not a substitute.

## 7. Allowed transitions

The Scheduler/Integration must reject impossible shortcuts.

Allowed normal transitions:

```text
Ready(issue)
 -> Doing(issue) + SOURCE(PR Draft)
 -> CI_WAIT
 -> SOURCE
 -> ... repeated coherent batches ...
 -> CI_WAIT
 -> SOURCE_READY
 -> PEER_CODE_REVIEW
 -> REVIEW_PASS
 -> PR Ready
 -> FINAL_QUALIFICATION
 -> MERGE_READY
 -> MERGED
 -> Done(issue)
```

Allowed reviewer-fix transition:

```text
PEER_CODE_REVIEW
 -> REVIEW_FIX_REQUIRED
 -> REVIEW_FIX
 -> CI_WAIT
 -> SECOND_PEER_CODE_REVIEW
 -> REVIEW_PASS
```

Allowed non-bounded rework transition:

```text
PEER_CODE_REVIEW
 -> REVIEW_BLOCKED
 -> REWORK_REQUIRED
 -> SOURCE
 -> CI_WAIT
 -> SOURCE_READY
 -> PEER_CODE_REVIEW
```

Forbidden shortcuts include:

- `SOURCE_READY -> FINAL_QUALIFICATION` without peer review;
- `CI green -> MERGE_READY` without peer review/completeness;
- `REVIEW_FIX -> REVIEW_PASS` by the same worker on the head it changed;
- stale `REVIEW_PASS -> MERGE_READY` after a relevant head change;
- `FINAL_QUALIFICATION -> MERGED` when exact-head/base composition no longer matches;
- product issue `Done` before verified merge/completion bookkeeping;
- starting new source work on a PR while a review lock is active.

## 8. Merge invariant

For a code-changing PR:

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

If any term is missing, stale, ambiguous, queued, unexecuted, or unsupported by live GitHub evidence, merge is forbidden.

## 9. Capacity rule

Maximum product source-changing capacity is four.

- `PEER_CODE_REVIEW` itself does not consume a source lane but consumes worker execution capacity.
- A ready review has priority over opening a new source lane.
- If all four workers would otherwise source-edit, reserve one as reviewer: effective schedule becomes `3 source + 1 review`.
- If that reviewer enters `REVIEW_FIX`, it becomes `3 source + 1 review-fix source = 4 source-changing lanes`.
- While a mandatory review/second review waits, no new fallback source lane may consume the reviewer capacity.

## 10. Validation checklist for the orchestration itself

The workflow is internally coherent only if all of these are true:

- product issue labels remain only Ready/Doing/Blocked/Done semantics;
- canonical PR phases are represented in #250/current cycle, not as product labels;
- event vocabulary contains every result used by workers;
- Scheduler understands SOURCE_READY, REVIEW_FIX_REQUIRED, REVIEW_FIX_APPLIED and REVIEW_PASS;
- worker prompts allow REVIEW_FIX only when explicitly assigned and forbid self-certification of the changed head;
- Integration rejects merges lacking a head-valid independent review;
- Reporter distinguishes self-review, peer review, review-fix authorship and Integration verification;
- review lock prevents concurrent source writes during review/fix/final qualification;
- REVIEW_FIX counts toward the four-source-lane cap;
- any relevant changed head invalidates previous review/final qualification;
- current #250/cycle records source/reviewer/review-fix ownership for candidates entering review.

No personal information may be introduced in tickets, reviews, code, tests, examples, generated metadata or control-plane records.