# NativeUI 1.0 documentation index

This page is the navigation entry point for the broad NativeUI 1.0 documentation set tracked by T122. It links stable documentation slices without trying to freeze public contracts that are still owned by active pre-1.0 blockers.

## Start here

| Goal | Document | Scope |
| --- | --- | --- |
| Understand the toolkit, supported platforms and ownership model | [NativeUI 1.0 overview and application lifetime](v1-overview-and-application-lifetime.md) | Retained architecture, `Application`/`UI`/window ownership, standalone vs embedded lifetime, UI-thread boundary |
| Build retained interfaces and use standard controls | [Composition, layout and widgets](v1-composition-layout-and-widgets.md) | Static/dynamic composition, layout, input, focus, pointer capture, widgets, overlays and virtualization |
| Consume NativeUI from CMake | [Packaging and CMake](v1-packaging-and-cmake.md) | Installed/build-tree package concepts, public targets/helpers, platform attachment and binary resources |
| Use services, tests and platform-specific limits | [Services, testing and limits](v1-services-testing-and-limits.md) | Resources, Dispatcher, DesktopServices, inspector/testing paths, supported/deferred platform scope |
| Build NativeUI itself on Linux | [Linux build notes](linux-build.md) | Linux/X11 development dependencies and build commands |
| Understand performance gates | [Performance benchmarks](performance-benchmarks.md) | Benchmark workloads, allocation/timing expectations and CI performance evidence |
| Understand accessibility scope | [Accessibility](accessibility.md) | Current semantic surface and the boundary between 1.0 semantics and deferred native accessibility bridges |

## Public/private boundary

Normal application and component code should stay on documented NativeUI public headers and drawing abstractions. The v1 documentation set does not teach direct Skia, Pugl, platform-backend or `nativeui/detail/` APIs as normal consumer interfaces.

The final exact 1.0 public API and CMake inventory is owned by T069. Until that freeze closes, documentation pages should describe only already-delivered behavior and must not turn provisional compatibility paths into promised stable API.

## Deliberate freeze boundaries

Several areas require final reconciliation before T122 can be considered complete:

- the final `State<T>` / `Binding<T>` ownership, reentrancy, notification and exception wording follows the T123/T124 safety work and the T069 API freeze;
- retained callback, reconciliation, lifecycle, layout, paint and teardown exception guarantees follow the remaining pre-freeze safety blockers before T069 freezes those contracts;
- the copy-pasteable Getting Started journey and production reference application belong to T070 and its child tickets rather than being duplicated here;
- release-candidate procedure and final release-readiness wording belong to T071;
- native accessibility bridges remain outside the 1.0 blocker set and are tracked separately; the current semantic/custom-component accessibility surface remains documented in [Accessibility](accessibility.md).

Documentation should make these boundaries explicit instead of guessing the final result of an unfinished blocker.

## Contributor and validation references

The repository control documents remain the source of truth for contribution and qualification rules:

- [`AGENTS.md`](../AGENTS.md) — repository workflow and engineering constraints;
- [`CODE_REVIEW.md`](../CODE_REVIEW.md) — mandatory review domains;
- [`CI_POLICY.md`](../CI_POLICY.md) — CI and final-qualification policy;
- [`ROADMAP.md`](../ROADMAP.md) — delivery order and milestone state;
- [`CONTEXT.md`](../CONTEXT.md) — current project context and active frontier.

This index is navigation only. It does not replace those policies, the T069 public API inventory, or ticket-specific acceptance criteria.
