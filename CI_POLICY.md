# NativeUI CI execution policy

This document defines **when** validation runs. `AGENTS.md` and `CODE_REVIEW.md` define the tests and review coverage required for each change. Local Mac validation and review qualify an ordinary ticket for merge; GitHub Actions qualify the integrated `main` branch and frozen release candidates afterward.

## 1. Ticket development and merge

1. Develop against current `main` on the Mac with local RED -> GREEN -> REFACTOR. Keep the PR Draft while its source, tests, examples or build logic are still changing.
2. Before requesting review, build serially with the default empty `NATIVEUI_ALLOWED_WARNINGS`, run the targeted tests and the full relevant local CTest suite, and exercise any applicable feature self-test, golden, native smoke and deterministic failure/recovery seams. Run AppKit window tests in a graphical session.
3. Complete the applicable `CODE_REVIEW.md` record and correct every Blocking/Important finding. Include exact local commands/results and any Mac-only coverage limit in the PR.
4. Mark the PR Ready for review after this local gate. Merge the reviewed, composition-compatible ticket without waiting for GitHub Actions. A Draft PR cannot merge. Keep one PR per ticket or independently reviewable sub-unit.
5. For a change to Pugl, Skia, Objective-C runtime/bridge code, public ABI, packaging, dependency pins, or another contract the Mac cannot establish, run the relevant remote workflow before merge when its result is necessary to judge correctness. Record this focused exception in the ticket/PR. It does not make the full CI matrix a default PR gate.

A ticket becomes `Done` after its acceptance criteria, local validation, review, merge and completion bookkeeping are satisfied. `Done` does **not** claim that every remote platform has qualified that ticket's individual SHA. Explicit ticket dependencies use this `Done` meaning.

For existing open issues written under the former per-PR CI cadence, keep their acceptance criteria and test coverage but apply this policy to the timing of remote runs. Update stale CI/checklist wording when that issue is next edited; it does not restore a per-PR merge gate.

## 2. Post-merge integration

- `Main Smoke` runs on each executable change pushed to `main`. It checks workflow contracts and builds/tests representative Core behavior on Linux. A newer merge may cancel an obsolete run.
- Dedicated subsystem workflows run on matching `main` pushes through their `paths` filters. They are non-blocking for the PR that was just merged. Retain `workflow_dispatch` where available for targeted diagnosis or early risk qualification.
- Full `CI` runs nightly on the current `main` SHA and through `workflow_dispatch` when a batch or release candidate needs earlier qualification. It retains Linux X11, Linux ARM64, macOS, Windows and sanitizer coverage. The nightly run qualifies an integrated batch, not each intermediate ticket SHA.
- `T042 Lifecycle Stress` runs weekly on `main` and on demand after lifecycle/platform changes or for a release candidate.
- `T052 v0.1 Release Gate` runs only on demand for a frozen release candidate. Supply the full 40-character SHA of an approved benchmark baseline; the selected workflow ref provides the exact candidate SHA. Never compare the candidate with itself.

Scheduled Actions use the latest default-branch commit and may start late. Launch `workflow_dispatch` when an integration checkpoint or release decision cannot wait for the schedule. Record the qualified SHA and run links; do not infer qualification for later commits.

## 3. Failure and release response

A red post-merge run requires prompt triage. Identify the first affected SHA and subsystem, open a priority regression issue, and fix or revert the responsible change. Pause further merges into the affected area until its integration check is green; independent work may continue. If the failure affects the shared Core/build contract, pause all executable merges until it is resolved. Never mark a release candidate qualified while a required integration or release check is red or missing.

Before a release, freeze one candidate SHA and run full `CI`, every applicable dedicated contract, `T042` and `T052` against that candidate. Record the exact SHA and approved T052 baseline. Any source, test, fixture, example, build, dependency, workflow or shipped API/release-documentation change creates a new candidate and requires its affected checks again. Project-state bookkeeping alone does not change executable qualification.

## 4. Workflow authoring

- Ordinary ticket PRs have no required remote status checks. Do not add `pull_request` triggers to project workflows without an explicit policy change.
- Keep broad integration in `CI`; dedicated workflows use `paths` filters for the subsystem they own on `main` pushes. Do not add umbrella headers or root `CMakeLists.txt` as generic proxies when full CI already owns generic integration.
- Keep `workflow_dispatch` on full, lifecycle and release gates. Preserve their platform, sanitizer, clean-bootstrap and benchmark coverage.
- A workflow that runs on `main` must cancel genuinely superseded runs of the same workflow/ref when safe. Do not cancel a frozen release-candidate run because a different ref advanced.
- Treat workflow and policy-contract changes as code: validate their syntax, trigger contracts and relevant local tests before merging.

Remote CI remains a visible integration signal and a mandatory release gate. Its failure is never silently ignored because it was non-blocking for an earlier PR.
