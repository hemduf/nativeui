# NativeUI code review policy — plugin-safe C++ and platform code

This document is a **mandatory review gate** for NativeUI changes. It exists because NativeUI is intended to run inside standalone applications **and inside audio plug-in processes**, where several plug-ins, several versions of the same plug-in, and several instances of one plug-in can coexist in one host process.

The rules below apply even though NativeUI itself does not implement VST3, CLAP, AU or DSP APIs. Any implementation in NativeUI must remain safe when called by an adapter for those formats.

A ticket that changes code is not complete until the applicable checks in this document have been performed and blocking findings have been corrected. The issue or PR must record the review result.

## 1. Review priorities

Review findings are classified as:

- **Blocking** — can cause cross-instance state corruption, host/process crashes, undefined behavior, real-time violations, use-after-free, data races, Objective-C runtime collisions, ABI violations or broken attach/detach lifecycle. Must be fixed before merge.
- **Important** — correctness, performance, ownership or maintainability defect that is likely to become a production issue. Fix before merge unless explicitly split into a tracked follow-up with no current correctness risk.
- **Advisory** — style or non-critical improvement.

Plugin-host safety takes precedence over convenience. "It works in the standalone example" is not sufficient validation.

## 2. Per-instance isolation — mandatory

Every plug-in/UI instance must be independently creatable, usable and destructible without changing another instance's behavior.

### 2.1 Mutable global state is forbidden by default

Do not introduce instance-dependent mutable state through:

- namespace-scope or file-scope non-const variables;
- mutable function-local `static` objects;
- singleton service locators;
- a global "current UI", "current editor", "current host" or callback target;
- a global pointer/reference to a `UI`, component, window, host, parent view or plug-in object;
- `thread_local` as a substitute for instance ownership;
- mutable static data members used as implicit instance state;
- process-wide registries whose entries can be overwritten by one instance and observed by another.

`thread_local` does **not** provide plug-in-instance isolation: multiple instances may execute on the same OS thread, and CLAP may move the logical audio thread between OS threads over time.

### 2.2 Global/process-wide data that may be acceptable

The following can be acceptable only when their semantics are explicit and reviewed:

- `constexpr` values and immutable lookup tables;
- immutable process-wide services supplied by the OS/framework;
- immutable, content-addressed caches that cannot be mutated in an instance-dependent way;
- synchronized process-wide caches/registries whose sharing is part of the public contract, whose keys cannot collide accidentally, and whose entries cannot be silently replaced by another instance;
- reference-counted process-wide initialization where the dependency itself requires process scope.

For every mutable process-wide object, the review must answer:

1. Why can this not be instance-owned?
2. What happens when two instances use it simultaneously?
3. Can one instance replace, clear or invalidate data used by another?
4. Is construction/destruction safe when plug-in modules are loaded/unloaded?
5. Is the sharing behavior part of a documented API contract?

If any answer is unclear, treat the global as a blocking finding.

### 2.3 Caches and resource managers

Prefer caches owned by `UI`, a window/view instance, or an explicitly owned resource context.

A shared cache must not use a short human-readable key if two independent consumers can legitimately provide different data under that key. Use instance ownership, a consumer namespace, or content identity.

Never allow `clear()`, replacement or eviction in one instance to invalidate an object still used by another instance without safe shared ownership.

## 3. Lifetime and host lifecycle

Assume hosts will exercise lifecycle sequences more aggressively than the standalone application.

Review all changed code for:

- repeated create/destroy cycles;
- multiple simultaneous instances;
- editor/view open -> close -> reopen;
- parent attach -> detach -> reattach where supported;
- destruction while focus, pointer capture, text input, timers, drag/drop or callbacks are active;
- host destruction immediately after the last callback;
- partial construction failure;
- exceptions/errors during platform setup;
- module unload/reload behavior where the format/host supports it.

Requirements:

- all ownership is explicit and preferably RAII;
- callbacks into an object are disconnected before that object dies;
- callback captures must not outlive the captured object;
- no raw owning pointer;
- no hidden dependence on static destruction order;
- teardown is idempotent where host call sequences may repeat benignly;
- native handles are treated as borrowed unless ownership is explicitly documented;
- a child view must never destroy the host-owned parent window/view.

VST3 initialization/de-initialization is normally performed on the UI/main thread. CLAP `init`, `activate`, `deactivate` and `destroy` have explicit main-thread/lifecycle preconditions in the CLAP headers. NativeUI adapters must preserve those contracts.

## 4. Threading and real-time boundaries

### 4.1 NativeUI is UI-thread confined unless an API explicitly says otherwise

The retained component tree, focus/input routing, window/view operations and ordinary `State<T>` mutation are treated as **UI/main-thread-only** facilities unless an API explicitly documents thread safety.

Do not infer thread safety from the absence of crashes in tests.

In particular, `State<T>` is not an audio-thread synchronization primitive. Do not call `State<T>::set`, mutate widgets, trigger layout, paint, touch a native view, or invoke AppKit/Win32/X11 UI operations directly from a real-time audio callback.

### 4.2 Real-time thread rules

Any adapter path callable from VST3 `IAudioProcessor::process`, VST3 `setProcessing`, CLAP `process`, CLAP `start_processing`, CLAP `stop_processing` or CLAP `reset` must be treated according to the format's real-time contract.

On a real-time path, do not perform:

- heap allocation/deallocation;
- contended mutex/condition-variable locking;
- filesystem or network I/O;
- UI calls;
- blocking logging;
- waiting/sleeping;
- process-wide initialization;
- Objective-C/AppKit work;
- STL operations that may allocate unless capacity/lifetime is proven in advance.

Move data between audio and UI domains using an explicitly reviewed mechanism such as atomics for simple scalar state, an SPSC queue, a bounded lock-free mailbox, or immutable/double-buffered snapshots. Ownership and memory ordering must be documented.

### 4.3 CLAP-specific threading consequence

CLAP's audio thread is a **logical** thread. A host may schedule one plug-in instance on different OS threads over time, but must not concurrently execute two audio-thread functions for the same instance. Different plug-in instances may of course process concurrently.

Therefore:

- never key per-instance state by `std::thread::id`;
- never rely on `thread_local` DSP/UI bridge state;
- all process data belongs to the plug-in instance/context, not to the executing OS thread;
- any process-wide service touched by different instances must itself be concurrency-safe.

## 5. Cross-thread callbacks and reentrancy

Assume callbacks can cause immediate re-entry unless the API guarantees otherwise.

Review for:

- user callbacks deleting/replacing the object that invoked them;
- callbacks triggering layout/invalidation while dispatch/layout/paint is already active;
- observer lists modified from inside an observer callback;
- host callbacks that synchronously call back into the plug-in/UI;
- stale callbacks queued after detach/destruction.

Do not hold a lock while calling user code or host code unless the API contract explicitly requires it and deadlock has been ruled out.

## 6. Exceptions and ABI boundaries

C++, C, VST3/COM-style, CLAP C ABI and Objective-C callbacks are different exception domains.

Requirements:

- never let a C++ exception escape through a C callback, CLAP function pointer, Objective-C runtime callback, Pugl callback, Win32 callback or other foreign ABI boundary;
- boundary thunks should be `noexcept` where practical and convert failures to the target API's error/result mechanism;
- destructors used during plug-in teardown must not throw;
- partial construction must leave no registered callback/native resource behind.

## 7. Symbol visibility and process coexistence

An audio host may load hundreds of plug-in modules into one process. NativeUI must not pollute or depend on the host's global symbol environment.

Review for:

- exported C/C++ symbols that do not need to be public;
- generic exported C names;
- accidental dependency on symbols supplied by the host or another plug-in;
- interposition-sensitive symbols;
- conflicting bundled dynamic libraries;
- process-wide environment or signal-handler changes.

Prefer hidden-by-default C/C++ visibility and export only the entry points/API that genuinely need external linkage. Internal free functions should use internal linkage or a detail namespace as appropriate.

Do not change host-wide application state such as application delegates, activation policy, main menu, signal handlers, working directory or locale from a library intended for plug-in embedding.

## 8. Objective-C / Objective-C++ runtime rules — mandatory on macOS

These rules apply whenever NativeUI or generated code contains `.m`, `.mm`, Objective-C runtime calls, categories, protocols, Objective-C-visible Swift classes, or dynamically allocated Objective-C classes.

### 8.1 The Objective-C runtime namespace is process-global

Objective-C class names live in a flat runtime namespace for the process. Audio plug-ins from unrelated vendors are loaded into the same host process. Hiding C/C++ symbols does **not** prevent an Objective-C class from being registered by name.

**Blocking rule:** never generate or add a generic Objective-C class name such as `Window`, `View`, `NativeView`, `PluginView`, `EditorView`, `AppDelegate`, `WindowController` or `NativeUIView`.

Every Objective-C runtime-visible name must have a collision-resistant prefix. This includes:

- classes;
- protocols;
- category names and, for categories on foreign/framework classes, category method selectors;
- names passed to `objc_allocateClassPair`;
- string-based class references (`NSClassFromString`, `objc_getClass`, nib/storyboard principal class names, etc.).

### 8.2 Static-library trap: a NativeUI-only prefix is not sufficient

NativeUI can be statically linked into more than one plug-in bundle loaded by the same host. If each plug-in contains an Objective-C class named `NativeUIFoo`, those copies still collide at runtime even though the source came from the same library.

Therefore reusable/static NativeUI platform code must follow one of these strategies:

1. **Preferred:** avoid defining Objective-C runtime classes entirely; use Objective-C++ functions/RAII wrappers around existing Cocoa objects when possible.
2. If a runtime class is genuinely required, its real runtime name must include a **consumer/plugin-specific unique prefix**, not merely a NativeUI library prefix.
3. The consumer-specific prefix should be derived from a unique bundle identifier (for example `com.hemduf.product` -> `ComHemdufProduct...`) or an equivalently collision-resistant build-time identifier.
4. Code generation must require that prefix as input/configuration. It must not silently fall back to a generic class name.

If NativeUI later introduces generated Objective-C classes, the build/API must expose an explicit consumer prefix mechanism before that code is accepted.

### 8.3 Categories and method swizzling

Avoid categories on Apple/framework classes in plug-in code. A category method can collide with the original class or another loaded category and the runtime behavior is undefined.

If a category is unavoidable:

- justify it in the ticket;
- prefix every added method selector with the consumer-specific prefix;
- never use the category to override an existing framework method;
- add a collision review/test.

**Method swizzling of host/AppKit/framework classes is prohibited** unless an explicit architecture ticket demonstrates there is no viable alternative and the process-wide effect is acceptable. For normal NativeUI work, treat swizzling as a blocking finding.

### 8.4 `+load`, `+initialize`, constructors and process-global initialization

Avoid `+load`, C/C++ constructor attributes and non-trivial global/static initialization in plug-in platform code. They run as the image is loaded, outside an instance lifecycle, and cannot safely represent per-instance state.

If `+initialize` is used, keep it class-local and minimal. It is serialized by the runtime and complex locking/dependency work can deadlock.

Prefer explicit initialization owned by the NativeUI/view instance.

### 8.5 AppKit and ARC ownership

- AppKit/native view creation and mutation belongs on the host's UI/main thread.
- Do not call AppKit from a real-time audio thread.
- Prefer ARC for Objective-C++ platform implementation unless a dependency requires otherwise.
- Audit `__bridge`, `__bridge_retained` and `__bridge_transfer` ownership explicitly.
- Avoid retain cycles in blocks/callbacks; use weak capture where ownership would otherwise cycle.
- Do not store a plug-in instance in process-global Objective-C state.
- Do not install yourself as `NSApplication` delegate or otherwise take ownership of the host application object.
- Do not assume NativeUI owns the host event loop.

### 8.6 Generated Objective-C naming checklist

Any code generator that emits Objective-C/Objective-C++ must prove all of the following:

- [ ] a consumer/plugin-specific prefix is mandatory input;
- [ ] generated class names contain that prefix;
- [ ] generated protocol/category/runtime names contain that prefix where applicable;
- [ ] no generic fallback name exists;
- [ ] string references use the final prefixed runtime name;
- [ ] two independently configured fixture plug-ins can coexist in one process without duplicate runtime names.

## 9. C++ ownership and API review

Check every change for:

- RAII ownership and deterministic teardown;
- `unique_ptr` as the default exclusive owner;
- justified `shared_ptr` ownership rather than convenience sharing;
- weak references for non-owning callbacks where necessary;
- absence of dangling `string_view`, `span`, raw pointer or callback captures;
- move operations leaving valid destructible objects;
- stable addresses where native APIs store a callback handle;
- no virtual call into a partially constructed/destructed object;
- no ABI dependence on compiler-specific layout across a C/plugin boundary.

Public headers must not leak Pugl, Skia, AppKit, Win32, Xlib or plug-in SDK types except through an explicitly approved low-level boundary.

## 10. Rendering/platform review

For rendering or windowing changes verify:

- each view owns its renderer/context state unless sharing is explicitly proven safe;
- no OpenGL/Skia context is accidentally reused between unrelated native views;
- attach/detach releases graphics resources at a graphics-safe point;
- logical/physical coordinate conversion is instance-local and scale-aware;
- embedded polling stays non-blocking;
- a window/view does not start its own nested event loop inside a plug-in host;
- timers are stopped before view destruction;
- clipboard/drag/drop pointers are not retained past their documented callback lifetime.

## 11. Mandatory validation matrix

Every code ticket must run the smallest applicable subset plus the full relevant project suite.

### 11.1 Always

- [ ] targeted unit/integration tests for the change;
- [ ] full relevant CTest suite;
- [ ] compiler warnings reviewed;
- [ ] ownership/lifetime review;
- [ ] global/static mutable-state review;
- [ ] callback/reentrancy review;
- [ ] thread-domain review;
- [ ] multi-instance impact explicitly assessed.

### 11.2 If view/platform/lifecycle code changes

- [ ] at least two simultaneous independent instances;
- [ ] destroy instance A and confirm instance B remains functional;
- [ ] repeated create/destroy loop;
- [ ] repeated attach/detach/open/close as applicable;
- [ ] active focus/capture/text-input teardown;
- [ ] ASan/UBSan where available;
- [ ] TSan when the changed code introduces or modifies shared cross-thread state and a supported configuration is available.

### 11.3 If macOS Objective-C/Objective-C++ code changes

- [ ] audit every runtime-visible Objective-C name;
- [ ] verify consumer-specific prefixing for generated/static-library runtime classes;
- [ ] audit categories and selectors;
- [ ] audit ARC/bridging/blocks;
- [ ] verify AppKit main-thread confinement;
- [ ] verify no `+load`, swizzling or host-wide application mutation was introduced without an explicit approved exception;
- [ ] inspect linked symbols/runtime classes where practical (`nm`, `otool`, runtime probe or an equivalent CI check).

### 11.4 If code can touch a plug-in audio callback through an adapter

- [ ] prove the path performs no allocation/deallocation;
- [ ] prove it does not take contended/blocking locks;
- [ ] prove it performs no I/O/UI/runtime initialization;
- [ ] verify cross-thread state handoff is bounded and race-free.

## 12. Required review record in each issue/PR

Before a ticket is marked Done, add a review record containing at least:

- **CODE_REVIEW.md:** completed;
- **Instance isolation:** pass / findings corrected / not applicable with reason;
- **Globals/statics:** pass / list of intentionally shared objects and justification;
- **Threading/RT:** pass / not applicable with reason;
- **Lifetime/reentrancy:** pass;
- **Objective-C runtime:** pass / not applicable; if applicable list prefix strategy;
- **Platform integration:** pass / not applicable;
- **Tests:** exact targeted/full/multi-instance/platform checks run;
- **Remaining findings:** none, or links to explicitly non-blocking follow-ups.

A bare "reviewed" is not sufficient.

## 13. Current NativeUI-specific invariants

During review, preserve these project-specific rules in addition to the checks above:

- NativeUI does not own VST3/CLAP/AU parameter or DSP semantics.
- widgets/layout remain independent from plug-in SDKs and platform headers;
- Pugl remains the normal native window/embedding layer;
- Skia remains the renderer;
- `PUGL_MODULE` polling never blocks;
- public geometry is logical coordinates; framebuffer geometry is physical pixels;
- component/view/focus/capture state is per instance;
- process-shared mutable registries are exceptional and require explicit justification;
- `State<T>` is UI-thread confined unless replaced/extended by an explicitly thread-safe abstraction.

## 14. External normative references

Reviewers should consult the current upstream specifications when behavior is format/runtime-specific:

- VST3 threading model and workflow: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html
- VST3 audio processor call sequence: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Workflow%2BDiagrams/Audio%2BProcessor%2BCall%2BSequence.html
- CLAP core plug-in lifecycle/thread annotations: https://github.com/free-audio/clap/blob/main/include/clap/plugin.h
- CLAP thread model: https://github.com/free-audio/clap/blob/main/include/clap/ext/thread-check.h
- Apple Objective-C plug-in name conflicts: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/LoadingCode/Tasks/NameConflicts.html
- Apple Objective-C class naming: https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/Conventions/Conventions.html
- Apple category collision guidance: https://developer.apple.com/library/archive/qa/qa1908/_index.html

When upstream specifications conflict with assumptions in this document, stop and update this document through a dedicated reviewed change rather than silently coding around the discrepancy.
