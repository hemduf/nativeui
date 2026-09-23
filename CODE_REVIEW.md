# NativeUI code review policy — plugin-safe C++ and platform code

This document is a **mandatory review gate** for NativeUI changes. It exists because NativeUI is intended to run inside standalone applications **and inside audio plug-in processes**, where several plug-ins, several versions of the same plug-in, and several instances of one plug-in can coexist in one host process.

The rules below apply even though NativeUI itself does not implement VST3, CLAP, AU or DSP APIs. Any implementation in NativeUI must remain safe when called by an adapter for those formats.

A ticket that changes code is not complete until the applicable checks in this document have been performed and every Blocking/Important finding has been corrected. The issue or PR must record the review result.

The normal-path result is not sufficient evidence. **Every state transition that invokes user/component code, allocates, schedules work, crosses a native boundary or acquires a resource must be reviewed at each failure point between prepare and commit.**

## 1. Review priorities

Review findings are classified as:

- **Blocking** — can cause cross-instance state corruption, host/process crashes, undefined behavior, real-time violations, use-after-free, data races, Objective-C runtime collisions, ABI violations, broken attach/detach lifecycle, destructor-time termination, leaked registered/native resources after partial construction, poisoned guard/state-machine flags after an exception, silent loss/duplication of accepted work, partially published retained state, or an unsafe synchronous fallback when a contract requires deferred execution. Must be fixed before merge.
- **Important** — correctness, performance, ownership or maintainability defect that is likely to become a production issue. Fix before merge unless explicitly split into a tracked follow-up with no current correctness risk.
- **Advisory** — style or non-critical improvement.

An unapproved compiler warning from NativeUI-owned code is a **Blocking** finding. NativeUI-owned targets must pass with the default empty `NATIVEUI_ALLOWED_WARNINGS`; an exception is valid only when the exact diagnostic is documented in the ticket/PR and explicitly enabled through that CMake cache setting. Target/source-local suppression (`-Wno-*`, `/wd*`, diagnostic pragmas, `COMPILE_WARNING_AS_ERROR=OFF`, or equivalent) is not an acceptable substitute for the explicit warning policy.

Plugin-host safety takes precedence over convenience. "It works in the standalone example" and "the CI happy path is green" are not sufficient validation.

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

If any answer is unclear, treat the global as a Blocking finding.

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
- partial construction failure at **every** acquisition step;
- exceptions/errors during platform setup and retained mount/layout;
- module unload/reload behavior where the format/host supports it.

Requirements:

- all ownership is explicit and preferably RAII;
- callbacks into an object are disconnected before that object dies;
- callback captures must not outlive the captured object;
- no raw owning pointer;
- no hidden dependence on static destruction order;
- teardown is idempotent where host call sequences may repeat benignly;
- native handles are treated as borrowed unless ownership is explicitly documented;
- a child view must never destroy the host-owned parent window/view;
- every resource becomes cleanup-capable **before the next operation that can throw**;
- partial construction must be tested after each meaningful resource acquisition, not only before the first and after the last.

### 3.1 Top-level owner destruction during callbacks

Do not assume that a callback may synchronously destroy the object, `UI`, window or view that invoked it.

If synchronous self/owner destruction is intentionally supported, the complete caller chain must prove all of the following:

- an independent lifetime/control token is acquired before user code begins;
- after user code returns, no access to `this`, `impl_`, `Tree`, `ViewCore`, native handles or other destroyed-owner state occurs unless the token proves the owner still exists;
- nested/reentrant callers satisfy the same rule;
- platform/native code that called into the callback is not left with a dangling handle;
- tests destroy the owner from the callback under ASan/UBSan.

If any caller cannot prove this, synchronous top-level destruction is unsupported at that callback boundary and must be deferred to an owner/Dispatcher/platform safe checkpoint. Component **subtree** removal may still use the retained-tree safe reconciliation mechanism.

A comment stating "the callback may destroy the owner" is a Blocking finding if code later touches owner state on the same stack.

### 3.2 Retained callbacks, invalidators and borrowed contexts

A callback documented or used as long-lived must not capture a raw `Tree*`, `Node&`, stack callback context, transient native event pointer, or other borrow that can expire before the callback object.

For retained callbacks:

- prefer a weak owner lifetime token plus stable monotonic identity/handle;
- stale invocation after node removal or whole-owner destruction must be a deterministic safe no-op unless another behavior is explicitly documented;
- retaining the callback must not keep a removed node/tree/window alive solely to avoid a dangling reference;
- stale identity must never resolve to a replacement object through address or ID reuse;
- explicit disconnect on unmount is desirable, but safety must not depend solely on every consumer remembering to disconnect.

`PaintContext`, `InputContext`, `FocusContext`, `LifecycleContext` and equivalent callback contexts are borrowed for callback duration unless explicitly documented otherwise. Review copy/move support if it makes accidental retention possible.

### 3.3 Destructor callback policy

Destructors and destructor-driven teardown are no-throw.

- Ordinary direct destruction should not call application callbacks unless the public contract explicitly requires destructor-time notification.
- If a contract deliberately requires a destructor to invoke application code, internal ownership/state must be terminal before the callback starts, the callback must be at-most-once, exceptions from it must be contained, and cleanup must continue.
- One throwing child/component must not stop sibling/parent/native resource teardown.
- Never depend on a scope-guard destructor that calls arbitrary user/component code while another exception is already unwinding.

VST3 initialization/de-initialization is normally performed on the UI/main thread. CLAP `init`, `activate`, `deactivate` and `destroy` have explicit main-thread/lifecycle preconditions in the CLAP headers. NativeUI adapters must preserve those contracts.

## 4. Threading and real-time boundaries

### 4.1 NativeUI is UI-thread confined unless an API explicitly says otherwise

The retained component tree, focus/input routing, window/view operations and ordinary `State<T>`/`ScrollState` mutation are treated as **UI/main-thread-only** facilities unless an API explicitly documents thread safety.

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
- recursive state writes from an observer;
- callbacks that throw after mutating subscriptions/state;
- host callbacks that synchronously call back into the plug-in/UI;
- stale callbacks queued after detach/destruction;
- callbacks that open/close overlays, windows, dialogs or other lifecycle objects reentrantly.

Do not hold a lock while calling user code or host code unless the API contract explicitly requires it and deadlock has been ruled out.

### 5.1 Callback iteration and observer rules

Any observer/callback dispatcher must define and test:

- whether a callback added during a pass can run in that same pass;
- whether removing a not-yet-run callback suppresses it immediately;
- the stable value/snapshot visible to one pass;
- recursive mutation/coalescing semantics;
- what happens to not-yet-started callbacks when an earlier callback throws;
- what happens to recursive/pending writes when a callback throws;
- whether a callback that already started can ever be retried.

Default rule unless an API explicitly says otherwise: **a callback whose invocation started is never automatically retried solely because it threw**.

Observer/dispatcher state must remain usable after the caller catches an exception. A `notifying`, dispatch-depth, iterator-generation or equivalent busy flag left active after unwind is a Blocking finding.

## 6. Exceptions, transactions and ABI boundaries

C++, C, VST3/COM-style, CLAP C ABI and Objective-C callbacks are different exception domains.

Requirements:

- never let a C++ exception escape through a C callback, CLAP function pointer, Objective-C runtime callback, Pugl callback, Win32 callback or other foreign ABI boundary;
- boundary thunks should be `noexcept` where practical and convert failures to the target API's error/result mechanism;
- ordinary direct C++ APIs may propagate user/component exceptions only **after framework invariants are restored**;
- no function declared `noexcept` may reach potentially throwing user/virtual/application code without an explicit local containment rule;
- destructors used during plug-in/UI/native teardown must not throw;
- partial construction must leave no registered callback/native resource behind.

### 6.1 Guard and bookkeeping restoration

Every flag/counter used as a temporary guard must be reviewed as an unwind-sensitive resource, including patterns such as:

- dispatch/reentrancy depth;
- `syncing_*`, `reconciling_*`, `cancelling_*`, `notifying_*`;
- pointer interaction/capture state;
- `*_posted`, `*_pending`, `*_active`, completion flags;
- transient borrowed native pointers;
- temporary callback/offer/delegate state.

Rules:

- restore the **exact previous state**, not merely a hard-coded default, so nested/reentrant usage remains correct;
- restoration runs on every return/throw path;
- a cleanup guard may restore framework-owned bookkeeping, but must not invoke arbitrary user/component callbacks during exception unwind;
- if semantic cleanup requires user callbacks, leave durable pending/dirty work for the next normal safe checkpoint;
- after catching an exception, the next valid operation must not be silently ignored because a stale busy/posted/cancelling flag remains set.

Any boolean guard set before a user/virtual callback and reset only on the normal tail is presumed unsafe until proven otherwise.

### 6.2 Transactional state machines and publication order

For every multi-step state transition — retained reconciliation, overlay show/close, dialog completion, window close, focus transfer, lifecycle phase, state notification, layout publication, resource registration — identify:

1. **Prepare:** allocation/validation/callback construction that may fail before publication.
2. **Commit point:** the exact moment the new externally observable logical state becomes authoritative.
3. **Recovery/rollback:** how every failure before/after commit leaves one coherent state.

Blocking patterns include:

- clearing/moving the only handle, generation, callback or owner reference before later fallible work is durably recoverable;
- mutating a public/logical registry, then running a throwing invalidator with no rollback or pending reconciliation;
- erasing logical state while retained/native state still requires work, without a durable dirty/pending marker;
- publishing a lifecycle phase or generation and then allowing allocation failure to lose the work needed to finish it;
- using address reuse or recycled IDs to "recover" a stale retained callback.

A failed operation must leave the object either coherently in the old state, coherently in the new state with required follow-up durably queued/marked, or in an explicitly terminal failure state. Ambiguous half-commit is Blocking.

### 6.3 Accepted work, queues and scheduling failure

Review both **callback execution failure** and **enqueue failure**.

For accepted queued work:

- a task whose callback began is not automatically retried unless the API explicitly documents retry;
- work accepted earlier but not yet started must not be silently destroyed merely because a neighboring callback threw, except under an explicit owner-shutdown contract;
- original ordering/fairness must be preserved when unstarted work is restored;
- timers/one-shots/repeating work must not duplicate or resurrect because recovery requeues tasks.

For enqueue/post/schedule operations:

- review `false`/rejection, capacity/full, owner-closing and exception-before-enqueue separately;
- setting a `posted` flag before a fallible enqueue requires rollback or another durable execution path;
- entering a lifecycle phase such as `Requesting` before a fallible enqueue requires recovery if enqueue fails;
- **if the contract requires execution at a later safe checkpoint, enqueue failure must never fall back to synchronous execution on the currently active callback stack**;
- lifecycle/control-plane correctness must not depend on ordinary user queue capacity; use a bounded owner-local pending flag/reserved control slot/existing pump checkpoint when needed;
- do not introduce unbounded retry queues or busy loops as recovery.

Tests must be able to deterministically force queue full/rejection and exception-before-enqueue; relying on natural OOM or rare saturation is insufficient.

### 6.4 Partial construction and native resource acquisition

From the first acquired resource onward, every subsequent throw point must be covered.

Audit constructors/factories that acquire:

- native windows/views/worlds/contexts;
- callback registrations/handles/delegates;
- Objective-C runtime attachment/associated state;
- IME bridges;
- timers/Dispatcher registrations;
- renderer/context resources;
- retained invalidation callbacks or owner registrations.

Rules:

- each resource becomes RAII/scoped-cleanup-owned before the next throwing call;
- cleanup distinguishes borrowed/shared resources from owned resources;
- a failed child must not free a shared Application/host resource;
- cleanup unregisters callbacks/handles before backing storage disappears;
- constructor failure after user-controlled layout/measure/mount is treated exactly like platform failure;
- fault tests should fail after each meaningful acquisition stage and verify zero leak/stale registration.

A manual `cleanup_and_throw()` helper is not sufficient evidence if a later arbitrary exception can bypass it.

### 6.5 No-throw teardown

Destructor-driven teardown must continue best-effort after individual callback failures.

Review all destructor call chains, not only the destructor body. If a destructor calls a helper that invokes `PointerCancel`, `focus_changed`, `deactivate`, `unmount`, completion code, platform cleanup or user callbacks, that entire path is part of the no-throw requirement.

Required order is generally:

1. make future external callbacks impossible or harmless;
2. mark state terminal enough to prevent duplicate entry;
3. contain failures from user/component teardown hooks;
4. continue releasing remaining retained/native resources;
5. unregister from owner/application last as required by ownership.

### 6.6 Layout and paint failure publication

For retained layout/paint:

- failed layout must not be published as a complete geometry frame used by input/paint;
- layout dirtiness must remain/re-become set after failure so a later pass can recompute;
- clearing dirty regions before fallible layout requires rollback/recovery;
- framework-owned Painter/SkCanvas save/clip/transform scopes must use RAII/equivalent balancing;
- failed paint must not clear dirty state as if a frame succeeded;
- a later successful paint must start from balanced renderer/canvas state.

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
3. The consumer-specific prefix should be derived from a unique bundle identifier (for example `com.vendor.product` -> `ComVendorProduct...`) or an equivalently collision-resistant build-time identifier.
4. Code generation must require that prefix as input/configuration. It must not silently fall back to a generic class name.

If NativeUI later introduces generated Objective-C classes, the build/API must expose an explicit consumer prefix mechanism before that code is accepted.

### 8.3 Categories and method swizzling

Avoid categories on Apple/framework classes in plug-in code. A category method can collide with the original class or another loaded category and the runtime behavior is undefined.

If a category is unavoidable:

- justify it in the ticket;
- prefix every added method selector with the consumer-specific prefix;
- never use the category to override an existing framework method;
- add a collision review/test.

**Method swizzling of host/AppKit/framework classes is prohibited** unless an explicit architecture ticket demonstrates there is no viable alternative and the process-wide effect is acceptable. For normal NativeUI work, treat swizzling as a Blocking finding.

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
- explicit lifetime for every retained callback/invalidator/reference accepted by public or semi-public API;
- stable monotonic identity/generation when stale handles must not resolve to replacements;
- move operations leaving valid destructible objects;
- stable addresses where native APIs store a callback handle;
- no virtual call into a partially constructed/destructed object unless explicitly contained and safe;
- no ABI dependence on compiler-specific layout across a C/plugin boundary.

For public APIs taking references or returning callback handles, the review must state whether ownership is borrowed, shared, transferred or lifetime-token-backed and how stale use behaves.

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
- clipboard/drag/drop pointers are not retained past their documented callback lifetime and are cleared on exceptional unwind;
- native-view constructors protect every acquired world/view/context/IME/callback registration before calling retained/user-controlled layout or mount code;
- close/request lifecycle remains deferred when the contract requires a safe checkpoint even if ordinary Dispatcher enqueue is rejected or throws;
- direct C++ destruction remains callback-silent where the window/view contract says so;
- native callback thunks contain C++ exceptions **and** leave the retained/platform state recoverable rather than merely preventing ABI unwind.

## 11. Mandatory validation matrix

Every code ticket must run the smallest applicable subset plus the full relevant project suite **before merge/final qualification**. This is a coverage requirement, not an instruction to launch every remote workflow on every intermediate commit. Current remote checks are defined in `.github/workflows/`.

### 11.1 Always

- [ ] targeted unit/integration tests for the change;
- [ ] full relevant CTest suite;
- [ ] NativeUI-owned targets emit zero compiler warnings with the default empty `NATIVEUI_ALLOWED_WARNINGS`; any approved diagnostic is documented and explicitly opted in through CMake;
- [ ] ownership/lifetime review;
- [ ] global/static mutable-state review;
- [ ] callback/reentrancy review;
- [ ] exception/unwind and transaction-commit review;
- [ ] scheduling/queue rejection/throw review where work is deferred;
- [ ] thread-domain review;
- [ ] multi-instance impact explicitly assessed;
- [ ] no personal information introduced in tests/examples/code/generated metadata.

### 11.2 If callbacks, observers, retained state or lifecycle transitions change

Use deterministic fault seams; do not depend on rare OOM/queue saturation occurring naturally.

- [ ] throw from the first, middle and last relevant user/component callback where ordering matters;
- [ ] callback removes/adds another observer/subscriber and then throws;
- [ ] callback recursively mutates the same state/structure and then throws;
- [ ] after the exception is caught, perform the next normal operation and prove all guard/depth/busy flags recovered;
- [ ] callback destroys/removes another retained target before its turn;
- [ ] stale retained invalidator/handle is invoked after target node removal and after whole-owner destruction;
- [ ] prepare/commit failure is injected between logical mutation and invalidation/reconciliation publication;
- [ ] accepted but not-yet-started work survives a neighboring callback throw without duplication unless owner shutdown explicitly permits discard;
- [ ] queue full/rejection and exception-before-enqueue are both tested for deferred work;
- [ ] when the contract requires a later safe checkpoint, failed enqueue proves there is **no synchronous callback-stack fallback**;
- [ ] destructor-driven path contains throwing user/component code and still completes remaining cleanup;
- [ ] failure in instance A leaves independent instance B operational.

### 11.3 If view/platform/lifecycle code changes

- [ ] at least two simultaneous independent instances;
- [ ] destroy instance A and confirm instance B remains functional;
- [ ] repeated create/destroy loop;
- [ ] repeated attach/detach/open/close as applicable;
- [ ] active focus/capture/text-input teardown;
- [ ] partial-construction fault after each meaningful acquired native resource/callback registration;
- [ ] retained layout/measure throw during native-view construction leaves no stale native resource/registration;
- [ ] teardown callback throw still releases renderer/IME/view/world/Dispatcher/Application registration as applicable;
- [ ] ASan/UBSan where available;
- [ ] LSan where supported for acquisition/teardown changes;
- [ ] TSan when the changed code introduces or modifies shared cross-thread state and a supported configuration is available.

### 11.4 If layout or paint changes

- [ ] layout throws after at least one earlier node was visited; no partial geometry frame is treated as committed;
- [ ] failed layout followed by successful recovery;
- [ ] dirty layout/paint state survives failure;
- [ ] descendant paint throws inside framework clipping/save scope; renderer/canvas depth is balanced;
- [ ] failed paint followed by successful repaint from clean canvas state;
- [ ] normal successful hot path is benchmarked if rollback/staging adds material cost.

### 11.5 If macOS Objective-C/Objective-C++ code changes

- [ ] audit every runtime-visible Objective-C name;
- [ ] verify consumer-specific prefixing for generated/static-library runtime classes;
- [ ] audit categories and selectors;
- [ ] audit ARC/bridging/blocks;
- [ ] verify AppKit main-thread confinement;
- [ ] verify no `+load`, swizzling or host-wide application mutation was introduced without an explicit approved exception;
- [ ] inspect linked symbols/runtime classes where practical (`nm`, `otool`, runtime probe or an equivalent CI check).

### 11.6 If code can touch a plug-in audio callback through an adapter

- [ ] prove the path performs no allocation/deallocation;
- [ ] prove it does not take contended/blocking locks;
- [ ] prove it performs no I/O/UI/runtime initialization;
- [ ] verify cross-thread state handoff is bounded and race-free.

### 11.7 CI qualification cadence

- During active TDD, keep the PR Draft and use normal CI plus only path-scoped dedicated workflows relevant to the changed subsystem.
- Do not interpret this validation matrix as a requirement to run unrelated historical ticket matrices on every commit.
- Once source/tests/build/workflows are frozen and normal/relevant CI is green, mark the PR Ready for review.
- Source/test/build/workflow changes after qualification invalidate the candidate and require a new Draft -> Ready transition.
- Pure project-state/completion documentation does not invalidate executable qualification unless it contains release/API material that is itself tested or shipped as part of the contract.

## 12. Required review record in each issue/PR

Before a ticket is marked Done, add a review record containing at least:

- **CODE_REVIEW.md:** completed;
- **Instance isolation:** pass / findings corrected / not applicable with reason;
- **Globals/statics:** pass / list of intentionally shared objects and justification;
- **Threading/RT:** pass / not applicable with reason;
- **Lifetime/reentrancy:** pass, including owner destruction and retained callback lifetime;
- **Transactional state:** pass / identify prepare, commit and rollback/recovery points for changed state machines;
- **Scheduling/queue failure:** pass / not applicable; cover capacity/rejection, owner closing and exception-before-enqueue where relevant;
- **Exception/unwind:** pass; ordinary C++ propagation, `noexcept` paths, destructor behavior and foreign ABI containment explicitly assessed;
- **Partial construction:** pass / not applicable; list acquired native/registered resources and cleanup on each failure stage when relevant;
- **Objective-C runtime:** pass / not applicable; if applicable list prefix strategy;
- **Platform integration:** pass / not applicable;
- **Performance/allocation:** pass / measured or reasoned hot-path impact;
- **Privacy:** pass; no personal information in tests/examples/code/generated metadata;
- **Tests:** exact targeted/fault/full/multi-instance/platform checks run;
- **Remaining findings:** none, or links to explicitly non-blocking follow-ups.

A bare "reviewed" is not sufficient. Omitting an applicable failure-domain field is itself an incomplete review.

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
- `State<T>` and `ScrollState` are UI-thread confined unless replaced/extended by an explicitly thread-safe abstraction;
- State/ScrollState observer dispatch remains usable after a throwing observer; started callbacks are not implicitly retried;
- retained invalidators/callbacks intentionally allowed to outlive a component are lifetime-safe after node removal and whole-Tree/UI destruction;
- top-level `UI`/window/view destruction from an active callback is deferred unless the complete caller chain explicitly proves self-destruction safety;
- ordinary C++ user/component exceptions may propagate only after NativeUI invariants are restored;
- C/Pugl/Objective-C/Win32/X11/other foreign callback boundaries contain all C++ exceptions;
- destructor-driven retained/native teardown is no-throw and best-effort complete;
- accepted queued work is not silently lost because a previously running callback throws, except under explicit owner shutdown semantics;
- deferred lifecycle/control operations never fall back to unsafe synchronous teardown merely because ordinary queue enqueue was rejected or threw;
- retained/lifecycle/overlay/window state machines have explicit prepare/commit/recovery boundaries;
- native partial construction leaves no stale callback/IME/view/world/renderer resource behind;
- failed layout does not publish partial geometry and failed paint does not publish a clean frame or leak framework-owned canvas state.

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
