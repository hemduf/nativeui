# NativeUI 1.0 release and validation guidance

This chapter records the current NativeUI 1.0 documentation boundary for validation and release-readiness work. It follows the repository state on `main` rather than historical gates that are no longer active.

## Current source of truth

For current contributor and qualification rules, use:

- [`ROADMAP.md`](../ROADMAP.md) for the active execution model, release frontier and integrated project state;
- [`AGENTS.md`](../AGENTS.md) for implementation, validation, review and completion workflow;
- [`CODE_REVIEW.md`](../CODE_REVIEW.md) for mandatory architecture, lifetime, threading, platform and failure-path review;
- [`VALIDATION.md`](../VALIDATION.md) for recorded validation notes and platform evidence.

There is no `CI_POLICY.md` file on current `main`. New v1 documentation must not depend on that retired path.

## Active validation model

The current roadmap distinguishes implementation progress from release qualification:

- normal implementation work uses targeted validation appropriate to the changed surface;
- implementation PRs remain Draft while source/build/tests are actively changing;
- ordinary remote CI is integration evidence, not a substitute for explicitly required local/platform validation;
- a frozen release candidate requires full relevant validation against one exact candidate state and an approved benchmark baseline;
- post-merge failures create regressions and pause affected-area merges until corrected;
- NativeUI-owned targets must compile with no unapproved warnings.

Documentation-only work must not invent executable evidence. When a check cannot be run by the current worker, record that limitation and keep the missing evidence as a merge/Done gate only when the applicable ticket requires it.

## Reference application and Getting Started status

T070 is closed as **not planned**. Its intended production reference application and Getting Started deliverable have not landed.

That work is currently represented by the separate T133–T137 track:

- T133 — external reference-app package skeleton and embedded resource;
- T134 — reference-app UI composition and multi-window lifecycle;
- T135 — deterministic reference-app self-test and service handoff demos;
- T136 — compile-checked NativeUI 1.0 Getting Started guide;
- T137 — cross-platform qualification of the reference app and guide.

Those tickets are currently open and blocked. Until they produce maintained artifacts, the T122 documentation set should link to existing focused examples and public headers instead of inventing a canonical tutorial application.

## NativeUI 1.0 release-gate status

T071 is closed as **not planned**. Historical T071 text can still explain the intent of an exact-SHA release gate, but it is not an active completion dependency for T122.

Current release/readiness wording should therefore point to the repository's active policies and exact-head evidence rather than saying that T071 will eventually supply the final procedure.

If a replacement NativeUI 1.0 release-qualification ticket is introduced, this chapter should be updated to link that concrete artifact.

## Documentation validation checklist

Before the T122 documentation PR is considered complete:

1. compare the branch with current `main` and resolve documentation drift;
2. verify every linked public header, CMake helper and repository policy file exists;
3. remove references to retired or not-planned tickets as future blockers;
4. keep T133–T137 described as undelivered follow-up work until their artifacts actually exist;
5. check relative Markdown links and navigation between the v1 chapters;
6. scan examples and prose for private/backend surfaces such as `nativeui/detail/`, direct Skia/Pugl/platform headers or undocumented package targets;
7. verify supported/deferred platform wording against the current roadmap and public package surface;
8. run any documentation/snippet checks that exist in the repository;
9. do not claim tests, platform runs or release qualification that were not actually executed;
10. keep the PR Draft until the documentation set and its validation are complete.

## Public/private boundary

Release-facing documentation must use the same consumer boundary as the rest of the v1 set:

- public NativeUI headers and documented CMake helpers are supported consumer surfaces;
- `nativeui/detail/`, renderer internals and platform backends are implementation details unless explicitly documented otherwise;
- exact behavior, ownership, threading and lifetime claims must match the current public implementation and its tests;
- documentation must not promote historical compatibility or POC paths as current v1 guidance.

This chapter is intentionally procedural. Product API details remain in the other v1 chapters, and executable release evidence belongs in repository validation records rather than being copied into prose.
