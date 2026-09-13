# NativeUI CI execution policy

This document defines **when** validation runs. It does not reduce the test/review coverage required by `AGENTS.md` or `CODE_REVIEW.md`.

## 1. Goals

- keep TDD feedback fast by keeping RED/GREEN iteration local whenever possible;
- use GitHub Actions as remote qualification, not as the inner development loop;
- batch related corrections into validation-worthy heads instead of triggering a matrix for every micro-change;
- avoid starting unrelated platform matrices for every pushed batch;
- avoid superseding a useful in-progress exact-head run without a concrete reason;
- cancel obsolete runs for genuinely superseded heads;
- preserve one complete final-candidate qualification before merge;
- keep dedicated workflows owned by the subsystem they validate.

## 2. Pull-request phases

### 2.1 Development — Draft PR

Keep implementation pull requests in Draft while code is still changing.

Local development may contain many fine-grained TDD steps. Those steps do **not** require one Git commit or remote CI run each.

A **qualification batch** is a coherent set of tests + implementation + refactor that closes one bounded acceptance slice, one related family of review findings, or another independently reviewable unit. Before publishing such a batch:

1. reproduce the relevant RED locally when the environment permits it;
2. complete the GREEN/refactor locally;
3. build the affected production surface locally;
4. run the targeted tests for the whole batch;
5. run the relevant broader local suite when practical for the touched surface;
6. fold/squash temporary local RED/GREEN/fixup commits when they do not represent independently useful rollback units.

Every **pushed qualification batch** must then run:

- the normal `CI` workflow;
- any dedicated workflow whose `paths` filter matches the changed subsystem.

Do **not** use remote CI as the default way to discover the next small local failure. Do **not** publish a known RED merely to create evidence when it can be reproduced locally.

An intentional RED may be pushed only when the required failure depends on a remote-only platform/environment that cannot be reproduced locally, or when the RED itself is an independently useful diagnostic checkpoint. Record that reason in the PR/ticket.

While an exact-head remote run is queued or running, do not push another small correction to the same PR merely because another potential gap was noticed. Collect and validate additional compatible local findings, then publish the next coherent batch only after the current run has produced useful evidence or is already known to be obsolete.

Heavy whole-project qualification workflows must **not** run on every Draft batch.

### 2.2 Closeout batching

When a ticket enters closeout/finalization:

1. audit the complete ticket against all acceptance criteria and required tests in one pass;
2. collect all current actionable Blocking/Important review findings before editing;
3. group compatible corrections into one bounded closeout batch;
4. run targeted/local validation for the whole batch;
5. publish one qualification head for that closeout batch;
6. repeat only if the resulting evidence reveals a genuinely new defect.

Do not intentionally run a remote sequence of “one widget/finding -> one push -> one matrix” when those findings could have been discovered and fixed in the same complete audit.

### 2.3 Final candidate — Ready for review

Before changing a PR from Draft to Ready for review:

1. freeze source, tests, CMake/build logic and workflow files;
2. make the latest normal `CI` run green;
3. make every applicable path-scoped dedicated workflow green;
4. complete the applicable `CODE_REVIEW.md` review;
5. preferably commit completion-state documentation before the transition.

The Draft -> Ready for review transition is the explicit final-candidate trigger for heavyweight qualification. `T042 Lifecycle Stress` and `T052 v0.1 Release Gate` run on that transition and remain available through `workflow_dispatch` for recovery/manual requalification.

### 2.4 Changes after final qualification

A change to any of the following invalidates the final candidate:

- production source or public headers;
- tests, fixtures, goldens or examples used for validation;
- CMake/build/dependency logic;
- GitHub Actions workflow files;
- release/API documentation or snippets that are consumed by tests or define shipped behavior.

If one of those changes is required after the PR is Ready, convert the PR back to Draft **before editing/pushing**, make the change, obtain green normal/relevant CI again, then mark it Ready again to launch a fresh heavyweight qualification.

Pure project-state/completion documentation such as `CONTEXT.md`, `ROADMAP.md`, `AGENTS.md`, `CODE_REVIEW.md`, `VALIDATION.md`, `DESIGN.md` and `THIRD_PARTY.md` does not invalidate an already qualified source/build/test candidate when it changes no executable contract. Do not rerun heavyweight platform/release gates solely because such bookkeeping changed the Git head.

## 3. Commit/push cadence

The remote branch should describe meaningful development checkpoints, not every local edit.

- Prefer one coherent implementation commit for a bounded acceptance slice.
- A ticket may use an additional consolidated review-fix commit when the final review finds actionable issues.
- Completion/docs bookkeeping may be separate when it does not alter executable behavior.
- More commits are acceptable when they are independently meaningful rollback, architecture, platform or diagnostic units.
- Avoid published `RED -> GREEN -> next RED -> next GREEN` micro-history for one logical slice; keep that detail local and squash/fold it before push.
- Never create a no-op or metadata-only source commit just to retrigger CI; use supported rerun/manual qualification mechanisms when the candidate itself is unchanged.

The policy controls remote cadence, not local TDD granularity.

## 4. Workflow authoring rules

For every dedicated workflow:

- use `paths` filters that describe the subsystem the workflow actually owns;
- do not add umbrella headers such as `include/nativeui/nativeui.hpp` merely to detect exports when normal CI already compiles/tests the root project;
- do not add root `CMakeLists.txt` merely to detect generic integration when normal CI already owns root-build validation;
- use one PR-scoped concurrency group and cancel obsolete runs:

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.event.pull_request.number || github.ref }}
  cancel-in-progress: true
```

- keep expensive whole-project/lifecycle/release gates on the final-candidate transition rather than every `pull_request synchronize` event;
- keep `workflow_dispatch` on heavyweight gates so a candidate can be requalified explicitly without manufacturing a source commit;
- prefer adding a test to the normal CI/CTest graph over creating another always-on per-ticket matrix;
- a new dedicated workflow requires a clear ownership boundary that normal CI cannot cover efficiently.

## 5. Merge evidence

A code-changing PR is mergeable only when the evidence applicable to its final candidate is green:

- latest normal `CI` for the source/build/test candidate;
- every relevant path-scoped dedicated contract workflow;
- heavyweight final-candidate workflows required by the ticket/project (`T042` and `T052` by default for final qualification);
- mandatory `CODE_REVIEW.md` record with no remaining Blocking/Important finding.

The purpose of this policy is to change **cadence**, not quality: validation is concentrated on coherent qualification batches and the final candidate instead of being repeated for every micro-step.
