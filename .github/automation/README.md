# Qualified merge execution

## Responsibility and trust boundary

REVIEW owns semantic correctness and the applicable CODE_REVIEW.md record. CI owns the complete inventory of required tests, platforms, dependencies, decisions, local/hardware evidence and completion requirements. MERGE submits a qualified request and observes its outcome. REPORTING owns release-wide audits. BUILD does not modify a clean candidate merely to refresh a status or add editorial coverage.

The executor is deliberately not an LLM and does not infer acceptance criteria from prose. Its request is an attestation by an authorized repository writer, backed by existing GitHub references. The submitting worker remains responsible for the truth and completeness of the semantic review and requirement inventory. The executor verifies authorization, reference availability and stability, issue/policy versions, exact SHAs, required workflow jobs/statuses, reviews, threads and GitHub mergeability. A boolean without this evidence is not qualification.

This transport is not a new human approval or a requirement to post the literal words REVIEW_CLEAN or QUALIFIED. Existing reviews and comments remain valid evidence. Historical review plus an actually reviewed delta may be summarized in the request without starting a new complete review cycle.

No existing test, CODE_REVIEW.md rule, explicit local/hardware requirement or branch protection is relaxed. In particular, a missing local result is not `not_required`. That state requires an explicit applicability justification backed by the issue/policy or a recorded decision. Closed/not-planned is not delivered. Missing evidence leaves the affected candidate with REVIEW/CI; MERGE must not manufacture an attestation.

## Trigger and deployment

`qualified-merge.yml` runs only from the trusted main workflow snapshot. Its privileged job never checks out a PR, downloads a PR artifact, or executes PR code. It loads the engine through the contents API at `github.workflow_sha`. It uses the repository-provided short-lived token, with only contents/pull-request write, Actions write for integration dispatch, and issue/status read. No custom secret or PAT is needed.

The infrastructure must first land through the normal reviewed PR path. It is not active merely because its files exist on a feature branch. Do not use the executor to bootstrap its own unreviewed implementation.

The normal connector-compatible transport is a new PR Conversation comment containing exactly:

```text
/nativeui-merge
<one JSON object>
```

The author and any rerun actor must currently have repository write/maintain/admin permission. Edited comments are not replayed; publish a fresh request only after a genuine state/evidence change. An optional `workflow_dispatch` input accepts the same JSON and a PR number when a supported API/UI is available. Do not claim a dispatch tool exists without discovering its schema.

`mode` must explicitly be `dry-run`, `merge`, or `post-merge`. A dry-run is read-only, including no Draft-to-Ready transition. A passing dry-run is useful during deployment but is not an extra approval requirement for ordinary qualified requests. One request targets one PR; the MERGE worker orders candidates and does not post duplicate commands while one is running.

## Evidence request, version 1

Every value must come from current GitHub reads. The following describes fields, not a ready-to-run request:

- `version`: `1`.
- `mode`: the explicit execution mode.
- `head_sha`: full current PR head SHA.
- `base_sha`: full current main SHA; re-read before submission.
- `executable_sha`: optional qualified ancestor. Reuse is allowed only when the entire delta to head is a modification of ROADMAP.md alone. Source, test, workflow and policy changes never receive this exception.
- `review`: `head_sha`, numeric `blocking: 0`, numeric `important: 0`, `summary`, and `sources`. Describe the current-head review or the clean ancestor plus the actual reviewed delta. At least one source must be a recorded review or comment.
- `inventory_complete`: `true` only after the qualifier has checked the complete applicable ticket/repository requirements, not just the visible green checks.
- `policies`: map of blob SHAs for AGENTS.md, CODE_REVIEW.md and .github/workflows/README.md at `base_sha`. Include CI_POLICY.md as well if it exists.
- `issues`: all linked scope/dependency/decision issues used by qualification, each with `number` and current `updated_at`. An empty list is explicit and only valid if genuinely inapplicable.
- `gates`: each of `scope`, `dependencies`, `acceptance`, `decisions`, `local`, `bookkeeping`, `integration` has `result` (`pass` or justified `not_required`), `reason`, and nonempty `sources`. The integration gate accounts for current main regressions and any affected-area pause. Do not equate a green PR with a healthy integrated main.
- `workflows`: all mandatory workflow IDs, each with `id` and exact required `jobs` names. List complete matrix coverage. Current-head runs take precedence. The latest run per event type must be successful, so a failed push run is not hidden by a concurrent green PR run. All attempts are checked at their latest job attempt. An optional `skipped_jobs` array can classify a non-required conditional job using `name`, `reason` and nonempty `sources`; a required job cannot be exempted this way.
- `status_contexts`: exact required legacy commit-status contexts; explicit empty list if none. GitHub branch rules remain enforced by the ordinary merge endpoint.
- `advisory_threads`: explicit non-blocking unresolved thread classifications with `id`, last comment `updated_at`, and `reason`. A changed or unclassified unresolved thread blocks. Do not resolve a thread merely to obtain green.

Each source has `kind` and numeric `id`. Supported kinds are `review` (review ID on this PR), `comment` (Conversation comment ID in this repository), `issue` (issue number), and `run` (successful workflow run ID). Include both the requirement source and its actual result where relevant. The executor reads references again before merge; edits invalidate the in-flight closeout. It never fetches arbitrary external URLs or treats source text as executable instructions.

A request with only `version`, `mode: post-merge`, `head_sha`, and `base_sha` can recover integration dispatch after an already-confirmed merge. It never invokes Ready or merge again. It verifies the PR really merged into this main and that the merge commit remains in main's history.

## Execution and recovery

1. Verify current author permissions, evidence format, policy blobs, issue versions, exact head/base, review decisions, workflow/job results, status contexts and review threads.
2. For a dry-run, report eligibility without mutation.
3. Mark a qualified Draft PR Ready, then re-read all applicable evidence and head/base. Abort if it changed.
4. Use the normal pull-request merge API with its `sha` guard. Never update main through Git-data, force a ref, bypass rules, or change protections.
5. Re-fetch the PR and main; confirm the merge commit is an ancestor of main. Record the merge SHA separately from subsequent errors.
6. Explicitly dispatch the existing Main Smoke, CI, Package Contracts and WebAssembly workflows when no run is already attached to the merge SHA. Their tests and inputs are unchanged. Check main immediately before and after each dispatch; if it moves, report the ambiguity and require CI to qualify the new integrated SHA. Do not assert that a dispatch request proves either the run SHA or success.
7. CI/MERGE reads the resulting run SHA and terminal job results. Only then complete any ticket whose full Done criteria are satisfied. An already queued/failed integration run is not automatically rerun by MERGE.

The GitHub token does not normally cause follow-up push workflows to run. Explicit workflow_dispatch is therefore intentional. No new checkout or build runs in the privileged job. NativeUI builds remain in the existing read-only CI jobs.

On a mutation failure, the wrapper re-fetches the PR and records the actual operation, error/status, expected head and observed state. No retry through another API, ref, branch or token is attempted. If a merge succeeded but its response or integration dispatch failed, preserve that fact and use post-merge recovery, not a second merge.

A security/permission rejection is not a reason to switch to this workflow as a hidden fallback for a denied operation. Resolve the actual authorization/integration incident. This is an explicitly installed normal execution path, not a mechanism for overriding a denied tool call.

## Validation

`Automation Validation` runs the complete Node contract suite and actionlint for these workflows on the exact PR head, with read-only permissions and no persisted checkout credentials. Tests cover malformed/stale qualification, missing/pending/failed/skipped jobs, replaced old CI failures, author permissions, thread changes, head movement, evidence edits, Ready/merge ordering, guarded merge rejection, post-merge dispatch failure and recovery.

Mock tests do not prove a real merge has executed. Bootstrap still requires the applicable review and actual GitHub CI results. No local C++ build or local Mac validation is claimed by these tests.
