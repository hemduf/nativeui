# NativeUI 1.0 documentation index

This page is the navigation entry point for the NativeUI 1.0 documentation set tracked by T122. It links the current public documentation slices and records the remaining release-closeout boundaries against the current `main` branch.

For broad application code, [`<nativeui/nativeui.hpp>`](../include/nativeui/nativeui.hpp) is the convenience umbrella for the normal public consumer surface. Narrow public headers may be included directly to reduce compile-time coupling; `nativeui/detail/` remains implementation-only.

## Start here

| Goal | Document | Scope |
| --- | --- | --- |
| Understand the toolkit, supported platforms and ownership model | [NativeUI 1.0 overview and application lifetime](v1-overview-and-application-lifetime.md) | Retained architecture, `Application`/`UI`/window ownership, standalone vs embedded lifetime, UI-thread boundary |
| Understand logical geometry and box constraints | [Core geometry and constraints](v1-core-geometry-and-constraints.md) | `Size`, `Point`, `Rect`, affine transforms, colors and normalized `Constraints` |
| Build retained interfaces and use standard controls | [Composition, layout and widgets](v1-composition-layout-and-widgets.md) | Static/dynamic composition, layout, widgets, overlays and virtualization |
| Drive the retained runtime directly | [Low-level retained Tree runtime](v1-low-level-tree-runtime.md) | Advanced Tree construction, lifecycle, layout, invalidation, input/focus and painting |
| Handle input, pointer capture, focus and editing commands | [Input, focus and commands](v1-input-focus-and-commands.md) | Normalized events, bubbling, IME, pointer metadata/capture, focus scopes and semantic command routing |
| Understand observable state and UI bindings | [State and binding](v1-state-and-binding.md) | `State<T>`, `Binding<T>`, notification/lifetime rules, reentrancy and widget binding boundaries |
| Customize drawing, styling and animation | [Rendering, styling and animation](v1-rendering-styling-and-animation.md) | Theme/style resolution, retained invalidation, custom painting and animation ownership |
| Customize standard widget recipes | [Widget style reference](v1-widget-style-reference.md) | Slider/progress/toggle/scrollbar/list/tabs/combo/text-editor typed style fields and resolution |
| Consume NativeUI from CMake | [Packaging and CMake](v1-packaging-and-cmake.md) | Installed/build-tree package concepts, public targets/helpers, platform attachment and binary resources |
| Use services, tests and platform-specific limits | [Services, testing and limits](v1-services-testing-and-limits.md) | Resources, Dispatcher, DesktopServices, inspector/testing paths, supported/deferred platform scope |
| Understand release status and qualification evidence | [Release and validation](v1-release-and-validation.md) | Active validation sources, exact-head evidence, retired release gates and remaining reference-app/Getting Started work |
| Build NativeUI itself on Linux | [Linux build notes](linux-build.md) | Linux/X11 development dependencies and build commands |
| Understand performance gates | [Performance benchmarks](performance-benchmarks.md) | Benchmark workloads, allocation/timing expectations and CI performance evidence |
| Publish semantic/accessibility data | [Semantic and accessibility data model](v1-semantics-and-accessibility.md) | `SemanticInfo`, roles/actions, stable IDs, immutable snapshots and virtualized collection semantics |
| Understand platform accessibility mapping | [Accessibility](accessibility.md) | Platform mapping, focus/availability semantics and native bridge boundaries |

## Public/private boundary

Normal application and component code should stay on documented NativeUI public headers and drawing abstractions. The v1 documentation set does not teach direct Skia, Pugl, platform-backend or `nativeui/detail/` APIs as normal consumer interfaces.

T069 has been retired as a standalone freeze gate. The public/package contracts and exact-head qualification on current `main` are authoritative. Documentation pages should describe that shipped public surface and must not promote `nativeui/detail/`, renderer internals or compatibility-only paths into supported consumer API.

## Release closeout boundaries

The main safety contracts that were previously provisional are now landed:

- T123/T124 and their binding follow-ups closed the `State<T>` / `Binding<T>` ownership, notification, reentrancy and lifetime work;
- T125/T130 closed retained dispatch, reconciliation, lifecycle, layout, paint and teardown exception-safety work;
- T069 is deprecated as a standalone freeze gate; current public/package contracts on `main` are the working source of truth.

T122 still needs final reconciliation around the remaining release sequence:

- the production reference application and copy-pasteable Getting Started journey are not currently delivered; T070 is closed as not planned, while T133–T137 remain the separate follow-up track and should be linked here only when those artifacts exist;
- T071 is closed as not planned; release/readiness wording must follow the current repository policies and exact-head qualification evidence rather than waiting on that retired gate;
- native accessibility bridges remain deferred to 1.2, while the current semantic/custom-component accessibility surface remains documented in [Accessibility](accessibility.md).

Documentation should distinguish those remaining release tasks from contracts that have already landed.
## Contributor and validation references

The repository control documents remain the source of truth for contribution and qualification rules:

- [`AGENTS.md`](../AGENTS.md) — repository workflow and engineering constraints;
- [`CODE_REVIEW.md`](../CODE_REVIEW.md) — mandatory review domains;
- [`ROADMAP.md`](../ROADMAP.md) — delivery order, milestone state and current release frontier;
- [`VALIDATION.md`](../VALIDATION.md) — validation commands and qualification expectations;
- [`DESIGN.md`](../DESIGN.md) — architectural design notes and public/private boundaries.

This index is navigation only. It does not replace repository policy, current public/package contracts, exact-head qualification evidence or ticket-specific acceptance criteria.
