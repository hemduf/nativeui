# NativeUI development automation

**Status:** local-first delivery with deferred GitHub integration qualification

This document complements `AGENTS.md`, `CODE_REVIEW.md` and `CI_POLICY.md`. GitHub Issues and PRs are the durable ticket record; local Mac tests and review qualify ordinary tickets for merge. GitHub Actions qualify integrated `main` batches and frozen release candidates afterward.

## 1. Delivery chain

```text
Issue Ready -> local Mac TDD and serial build/tests -> Draft PR
-> applicable code review and local evidence -> Ready PR -> merge
-> Main Smoke / path-scoped post-merge checks -> nightly full CI
-> frozen release candidate -> T042 / T052 and all release checks
```

Do not use Actions as the RED/GREEN inner loop. Publish coherent implementation and review-fix batches. A ticket is Done after its local acceptance, review, merge and completion bookkeeping; the integrated `main` SHA has its own remote qualification state.

## 2. Ticket selection and concurrency

1. Reconstruct live issue, dependency, PR and review state from GitHub before assigning work. Never trust retired Scheduler/Reporter snapshots over current GitHub state.
2. Prefer the highest-priority Ready ticket with the greatest downstream unblock value. Resume a valid existing PR before opening a replacement.
3. At most two independent source-changing lanes may be active. Their files, APIs, runtime state and dependencies must not overlap. Use separate branches/worktrees and do not stack independent tickets.
4. Only one local build runs at a time on the Mac, always with `CMAKE_BUILD_PARALLEL_LEVEL=1`. Review of a locally validated PR may overlap work on another independent Ready ticket.
5. If an optional remote/platform check is pending, document it and continue independent work. A red shared Core/build integration result pauses executable merges until fixed; a subsystem failure pauses merges in that area.

Parallelism is a limit, not a target. One lane is appropriate when changes overlap or review/build capacity is constrained.

## 3. Implementation and review

Before writing, read the recovery documents and the selected live issue, then map acceptance criteria to tests and failure/recovery seams. Use local RED -> GREEN -> REFACTOR. Run targeted and full relevant local CTest, applicable feature/self-test/native/golden coverage, and a warning-free serial build in the graphical session when AppKit tests are involved.

The implementation worker records exact local commands/results in the PR. Review the complete bounded ticket against `CODE_REVIEW.md`, including instance isolation, lifetime, transactions, enqueue failures, exception boundaries, native partial construction and privacy where applicable. Collect Blocking/Important findings in one batch; fix and re-review the changed result before merge. An independent reviewer is preferred for substantial or high-risk changes, and may work while another independent ticket is implemented.

For Pugl, Skia, Objective-C, ABI, packaging or dependency changes, run the relevant focused remote check before merge when Mac evidence cannot establish a required correctness claim. Document the specific reason. Ordinary full-matrix CI is not a ticket PR gate.

## 4. Merge and post-merge response

Merge a Ready PR when acceptance criteria, local tests, review, composition and required completion documentation are satisfied. Do not wait for normal or heavyweight GitHub Actions. Keep issue status and labels, `CONTEXT.md` and `ROADMAP.md` synchronized in the completion cycle.

After each merge, observe `Main Smoke` and matching path-scoped workflows. Full `CI` qualifies the latest `main` batch nightly or on demand. A failure gets a priority regression issue with its first affected SHA and a fix or revert. Pause affected-area merges until green; independent local work may continue. Never claim a deferred check passed when it has not run.

## 5. Release qualification

Choose one frozen candidate SHA. Run full `CI`, every applicable dedicated workflow, `T042 Lifecycle Stress` and `T052 v0.1 Release Gate` with a distinct approved benchmark-baseline SHA. A release cannot proceed until all required runs and the release review are green for that candidate. Executable changes after the freeze create a new candidate and require affected checks again.

## 6. Recovery and reporting

A restarted run reconstructs state from live issues, PRs, reviews and workflow runs. `AUTOMATION_STATUS.md` is a readable snapshot, never an assignment database. Reporting/watchdogs are read-only and may flag a stalled review, red `main`, missing completion documentation or an unqualified release candidate.

For the first 5–10 tickets under this policy, track issue-to-merge time, time spent in review, number of local correction batches, time `main` is red, post-merge regressions and rollback count. Revisit concurrency and cadence using those results. Retired Scheduler generations, worker reservations and Reporter state remain inactive.
