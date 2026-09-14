# NativeUI 1.0 packaging and CMake

This chapter documents the stable NativeUI 1.0 package architecture already implemented in the repository. It explains the roles of the exported core target and the consumer-side CMake helpers without duplicating the final copy-pasteable application tutorial owned by T070.

The final T122 reconciliation must compare this chapter with the frozen T069 public API/package inventory before NativeUI 1.0 is declared complete. Where implementation details can still move without changing the public package contract, this chapter deliberately describes the contract rather than an internal target graph.

## Package model

NativeUI separates the generic C++ UI/runtime library from the platform bridge that must be attached to a concrete final consumer.

The durable consumer model is:

1. locate the NativeUI CMake package;
2. link the consumer to `NativeUI::Core`;
3. attach NativeUI's platform implementation to the concrete final executable/module/shared-library with `nativeui_attach_platform(...)`, or use `nativeui_add_application(...)` for the higher-level standalone application path;
4. keep application code on public NativeUI headers rather than package-owned Pugl, Skia or `nativeui/detail/` implementation headers.

The installed package exports `NativeUI::Core`. Platform implementation sources and pinned third-party support are package machinery used by the helper layer rather than a second public all-in-one library target.

Do **not** document or depend on `NativeUI::NativeUI` as the complete package target. A source-tree implementation alias with that name may exist for NativeUI's own build, but it is not the installed consumer contract and is intentionally not exported as the complete native target.

Repository references:

- [`CMakeLists.txt`](../CMakeLists.txt) defines and exports `NativeUI::Core` and installs the package helper modules;
- [`cmake/NativeUIConfig.cmake.in`](../cmake/NativeUIConfig.cmake.in) is the installed package configuration;
- [`cmake/NativeUIBuildTreeConfig.cmake.in`](../cmake/NativeUIBuildTreeConfig.cmake.in) provides the corresponding external build-tree package surface;
- [`cmake/NativeUIAttachPlatform.cmake`](../cmake/NativeUIAttachPlatform.cmake) owns the low-level public attachment helper;
- [`cmake/NativeUIApplication.cmake`](../cmake/NativeUIApplication.cmake) owns the high-level standalone application helper.

## `NativeUI::Core`

`NativeUI::Core` is the generic C++20 NativeUI library exported by the package. It contains the retained UI/runtime and non-window-specific services needed by consumers. It is intentionally usable independently from the final platform attachment.

That separation matters for two reasons:

- one generic NativeUI core can be reused while each final consumer gets the platform implementation appropriate to its own executable/module identity;
- package consumers do not need to know which private Pugl, Skia, OpenGL, Objective-C, Win32 or Linux implementation units make up the platform bridge.

The exact private dependency graph below `NativeUI::Core` remains package implementation detail. In particular, consumers should not include Skia headers because an implementation archive is present in the package. NativeUI's normal rendering API is the public NativeUI abstraction layer documented by the rendering chapter.

## Low-level final-target attachment

`nativeui_attach_platform(...)` is the low-level package helper for applications or hosts that create their own final CMake target.

Its public responsibilities are conceptual rather than a promise about private generated targets:

- attach the NativeUI/Pugl platform implementation to one existing local final target;
- associate that final target with one stable `CONSUMER_ID`;
- make the required platform languages and native libraries available through package-owned CMake logic;
- keep the platform implementation private to the final consumer instead of exposing backend objects as normal application dependencies.

The helper accepts final `EXECUTABLE`, `MODULE_LIBRARY` or `SHARED_LIBRARY` targets. Alias/imported/non-final targets are rejected. A target can be attached only once.

`CONSUMER_ID` is a stable reverse-DNS-style ASCII identity. It is build/package identity, not mutable runtime application state. A single consumer identity must not be reused for a different final target in the same configure.

The canonical helper implementation and validation rules live in [`cmake/NativeUIAttachPlatform.cmake`](../cmake/NativeUIAttachPlatform.cmake). T070 owns the final copy-pasteable consumer CMake example, so this chapter does not create a second tutorial snippet that could drift from it.

## High-level standalone application helper

`nativeui_add_application(...)` is the higher-level path for a normal standalone NativeUI executable. It creates the appropriate platform application target, links `NativeUI::Core`, and performs the same final-target platform attachment internally.

Its current public metadata model includes:

- a target name;
- `PRODUCT_NAME`;
- `BUNDLE_ID`;
- `VERSION` using the `MAJOR.MINOR.PATCH` form;
- caller-owned `SOURCES`;
- platform-specific optional application icons where supported.

The helper owns native packaging metadata such as the macOS application bundle configuration and the Windows GUI executable/icon resource setup. Application product metadata remains caller input; consumers should not recreate NativeUI's private platform bridge or Objective-C runtime naming themselves.

See [`cmake/NativeUIApplication.cmake`](../cmake/NativeUIApplication.cmake) for the authoritative helper implementation. The final Getting Started journey and reference application remain T070-owned.

## Why macOS platform code is consumer-specific

The macOS Pugl backend contains Objective-C runtime classes. Objective-C class names are process-global, which means one framework-wide generic class prefix is unsafe when multiple independently built NativeUI consumers can coexist in one process.

NativeUI therefore keeps generic pieces reusable while compiling the Cocoa/OpenGL bridge and Cocoa IME runtime-bearing implementation for each final consumer identity. The public helper derives a deterministic Objective-C runtime namespace from the exact consumer identity. Consumers provide the stable identity; they do not provide or manage a manual runtime prefix.

This is a build-time namespace/isolation mechanism. It does not introduce a runtime singleton or a process-global current NativeUI application.

The implementation and coexistence rules live in [`cmake/NativeUIConsumerPlatform.cmake`](../cmake/NativeUIConsumerPlatform.cmake). NativeUI's package tests audit independently identified macOS consumers to prevent unprefixed Pugl runtime classes from leaking back into final binaries.

## Windows and Linux attachment

Windows and Linux use the same public final-target attachment concept. The package helper resolves and adds their platform implementation without requiring application code to include backend-specific headers.

On Linux/X11, the package also connects the package-owned D-Bus transport required by NativeUI platform services. That transport remains below the public API: callers use NativeUI service types rather than libdbus types.

Platform implementation details are deliberately not reproduced here because normal application code should not depend on them and T069 owns the final public/private inventory.

## Installed and build-tree package parity

NativeUI supports both an installed package and an external build-tree package. Both expose the same consumer concepts:

- `NativeUI::Core`;
- `nativeui_attach_platform(...)`;
- `nativeui_add_application(...)`;
- the binary-data resource helper.

The installed package carries the pinned package resources it needs so a relocated installation does not silently depend on the original source/build tree. Package validation checks are responsible for catching missing required payloads.

The build-tree package points at the current NativeUI build/source dependency roots, but application-facing helper names and target roles are intended to match the installed package. Consumers should not write separate package logic for the two modes.

## Pinned implementation payload

NativeUI owns a pinned renderer/windowing implementation payload as part of its package construction. That payload is not promoted into the public application API.

In particular:

- pinned Skia libraries/headers may be packaged to satisfy NativeUI's own renderer linkage;
- pinned Pugl sources may be packaged because the final consumer platform bridge is compiled with the consumer target;
- platform implementation source files can be shipped as package machinery for that same attachment step;
- legal/notices payload is installed with the package.

The presence of these files is not permission for normal consumer code to include private Skia/Pugl/platform headers. The public boundary remains NativeUI's headers, `NativeUI::Core`, and the documented CMake helpers.

## Binary resources

The package also exposes `nativeui_add_binary_data(...)` for compile-time embedded resources. It generates a caller-named CMake target plus a generated public resource table header, and links that generated target to `NativeUI::Core`.

Resource identifiers are validated and deterministic; generated payload symbols stay implementation-local. Runtime resource lookup/ownership belongs to NativeUI's public resource APIs, documented in [`v1-services-testing-and-limits.md`](v1-services-testing-and-limits.md).

The helper implementation is [`cmake/NativeUIBinaryData.cmake`](../cmake/NativeUIBinaryData.cmake).

## Consumer boundaries

For 1.0, keep these package boundaries explicit:

- application code links the documented NativeUI targets/helpers, not private implementation targets;
- application code does not include `nativeui/detail/`, Pugl, Skia, Cocoa, Win32 or Linux transport implementation headers as part of the normal NativeUI API;
- platform bridge state belongs to the concrete final consumer; there is no hidden current target/application registry at runtime;
- CMake's configure-time bookkeeping is not runtime mutable global state;
- UI/platform services are not real-time/audio-thread facilities;
- plugin SDK ownership, DSP/audio processing and host parameter models remain outside NativeUI.

## Package validation and release discipline

Packaging changes are not validated by a successful in-tree library build alone. NativeUI's CI policy requires package/relocation and relevant path-scoped checks in addition to normal CI, followed by final-candidate qualification when the candidate is frozen.

For contributors, use the repository policies rather than copying them into application documentation:

- [`CI_POLICY.md`](../CI_POLICY.md) defines exact-head qualification and final-candidate rules;
- [`CODE_REVIEW.md`](../CODE_REVIEW.md) defines package/public-private/lifetime/platform review expectations;
- [`AGENTS.md`](../AGENTS.md) defines the repository implementation/review workflow.

T122's final documentation pass must also scan this chapter for obsolete package claims after T069 freezes the v1 public surface.

## Final 1.0 reconciliation checklist

Before this chapter is considered final:

- confirm the exported package target inventory against T069;
- confirm `nativeui_attach_platform(...)` and `nativeui_add_application(...)` names/arguments against the frozen package contract;
- confirm the final package/relocation checks are green on the release candidate;
- ensure no text presents `NativeUI::NativeUI` as a consumer complete-target;
- ensure no normal-user guidance depends on direct Skia, Pugl or `nativeui/detail/` APIs;
- link to T070's canonical Getting Started/reference application once its final location is frozen;
- remove any stale source-tree/POC packaging guidance from README/navigation during the T122 completion cycle.
