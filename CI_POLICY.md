# NativeUI CI execution policy

This document defines **when** validation runs. It does not reduce the test/review coverage required by `AGENTS.md` or `CODE_REVIEW.md`.

## 1. Goals

- keep TDD feedback fast while a pull request is changing frequently;
- avoid starting unrelated platform matrices for every commit;
- cancel obsolete runs for the same pull request;
- preserve one complete final-candidate qualification before merge;
- keep dedicated workflows owned by the subsystem they validate.

## 2. Pull-request phases

### 2.1 Development — Draft PR

Keep implementation pull requests in Draft while code is still changing.

Every source/build/test change must run:

- the normal `CI` workflow;
- any dedicated workflow whose `paths` filter matches the changed subsystem;
- the smallest targeted local tests required by the ticket.

Heavy whole-project qualification workflows must **not** run on every Draft commit.

### 2.2 Final candidate — Ready for review

Before changing a PR from Draft to Ready for review:

1. freeze source, tests, CMake/build logic and workflow files;
2. make the latest normal `CI` run green;
3. make every applicable path-scoped dedicated workflow green;
4. complete the applicable `CODE_REVIEW.md` review;
5. preferably commit completion-state documentation before the transition.

The Draft -> Ready for review transition is the explicit final-candidate trigger for heavyweight qualification. `T042 Lifecycle Stress` and `T052 v0.1 Release Gate` run on that transition and remain available through `workflow_dispatch` for recovery/manual requalification.

### 2.3 Changes after final qualification

A change to any of the following invalidates the final candidate:

- production source or public headers;
- tests, fixtures, goldens or examples used for validation;
- CMake/build/dependency logic;
- GitHub Actions workflow files;
- release/API documentation or snippets that are consumed by tests or define shipped behavior.

If one of those changes is required after the PR is Ready, convert the PR back to Draft **before editing/pushing**, make the change, obtain green normal/relevant CI again, then mark it Ready again to launch a fresh heavyweight qualification.

Pure project-state/completion documentation such as `CONTEXT.md`, `ROADMAP.md`, `AGENTS.md`, `CODE_REVIEW.md`, `VALIDATION.md`, `DESIGN.md` and `THIRD_PARTY.md` does not invalidate an already qualified source/build/test candidate when it changes no executable contract. Do not rerun heavyweight platform/release gates solely because such bookkeeping changed the Git head.

## 3. Workflow authoring rules

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

## 4. Merge evidence

A code-changing PR is mergeable only when the evidence applicable to its final candidate is green:

- latest normal `CI` for the source/build/test candidate;
- every relevant path-scoped dedicated contract workflow;
- heavyweight final-candidate workflows required by the ticket/project (`T042` and `T052` by default for final qualification);
- mandatory `CODE_REVIEW.md` record with no remaining Blocking/Important finding.

The purpose of this policy is to change **cadence**, not quality: validation is concentrated where it provides useful information instead of being repeated on every intermediate commit.
