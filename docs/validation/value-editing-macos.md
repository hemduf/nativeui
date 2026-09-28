# Value editing / embedded visibility validation

Implementation is based on NativeUI `74f70b986152903f470a735c9e416003baba3cea`.
Validation machine: macOS arm64, Release and Debug ASan/UBSan, AppleClang,
graphical AppKit session.
All compilation used one process (`CMAKE_BUILD_PARALLEL_LEVEL=1`); no warning
allowlist or new compiler diagnostic suppression was added.

## Baseline and dependencies

The requested existing `nativeui-main/build` directory was absent. A dedicated
worktree build used the unchanged locally cached Pugl
`94982803985eefcbaeb0a1c8d0136ec862d7cb59` and checksum-pinned Skia `chrome/m153`
archives via the documented `NATIVEUI_PUGL_SOURCE` / `NATIVEUI_SKIA_ROOT` overrides.
Baseline state, component-state, slider-completion, and real embedded smoke tests
passed (4/4) before implementation. The initial new helper test failed compilation
because `<nativeui/edit.hpp>` did not exist, then passed after implementation.

## Completed checks

- New EditSession tests: exact ordering, no-op commands, external State writes,
  invalidated Binding, independent sessions, begin/change/end/cancel exceptions,
  recovery after exceptions, State observer exception preserving its error,
  session destruction from begin/change callbacks, deferred cancel winning over
  end, and rejection of recursive edits during callbacks/State notification.
- New widget tests: Knob/Slider pointer, keyboard and opt-in wheel source/order;
  Escape, capture cancellation, disabled/hidden/deactivated and unmounted edit
  cancellation; limit no-ops; default wheel compatibility; Toggle repeat policy.
- New AppKit integration: actually hidden NSView at construction, preserved host
  first responder, passive show, hide and sibling isolation, idempotence, active
  edit cancellation, hidden State/size update/poll, legacy visible construction,
  terminal close behavior and unchanged host frame/visibility.
- Existing State, component-state, focus, pointer-capture, gesture,
  slider-completion and embedded smoke tests passed with the implementation.
- Both public feature examples compiled and their displayless `--self-test`
  modes passed. The standalone public edit header compile fixture passed.
- The expanded final Release CTest selection passed 24/24: State, EditSession,
  widget edits, component-state, focus, routing, pointer capture, gestures,
  lifecycle, view geometry, dynamic composition, widgets, buttons, checkbox/radio,
  slider value/completion/visual contracts, embedded visibility, both smoke tests,
  both feature self-tests, Objective-C runtime-prefix and Pugl staging contracts.
  The edit/state/widgets/window public headers compiled individually.
- The new callback-fault widget test first failed with capture still held after a
  throwing change callback. The fix clears physical gesture/press state and
  releases capture before rethrowing the original error. Begin/change/end fault
  tests now prove at-most-once terminal notification, immediate balanced capture,
  inert trailing move/up events, and a successful next gesture. Toggle also
  proves keyboard retry is not stranded behind stale repeat suppression.
- AppKit hide tests cover cancel, focus-loss and deactivate failures separately
  and simultaneously. They verify the actual NSView is hidden, all cleanup stages
  run, the first error survives, terminal callbacks are not repeated, native IME
  candidate geometry becomes inactive, subsequent show/edit works, and a sibling
  remains usable. Four create/hide/show/close cycles are exercised.
- Debug ASan + UBSan builds use the project's sanitizer configuration, including
  its documented exclusion of Skia vptr instrumentation. EditSession, widget edit,
  lifecycle, and real AppKit embedded-visibility tests passed (4/4), with zero
  sanitizer diagnostics. The isolated EditSession sanitizer probe also passed.
- The separate existing T066 harness compiled and passed
  `nativeui_t066_destroy_survivor_tests` and
  `nativeui_t066_window_controls_platform_tests`, covering acquisition-stage
  constructor failures, throwing teardown, surviving sibling windows and native
  close-control behavior.
- `pugl_visibility_contract.cmake` verifies the shared original Pugl source hash
  remains unchanged, required transformations are present, and staging is
  deterministic. It passed. `git diff --check` passed.

## Implementation review

Editing notifications are UI-thread-only and synchronous. No process-wide
mutable state, host/parameter/audio semantics, or thread transport was added.
The shared session control block is scoped to one editing owner and exists to
survive reentrant owner removal; begin/update hold it by value across callbacks.
Terminals mark the session inactive before application code, and destructor
cancellation contains exceptions. Equal values and external State writes do not
emit callbacks. State delivers observers before edit change notifications.

New callbacks occur after widget gesture/visual bookkeeping wherever possible;
retained subtree removal keeps the existing UI safe checkpoint. Synchronous
whole UI/native owner destruction remains unsupported and is documented.
Visibility retains native acquisition/cleanup ownership and the existing
construction-unwind path. Hide completes pointer/focus/IME cleanup before
propagating callback failure. Native windows, sibling views, and first responders
remain host-owned. The Pugl source compatibility correction introduces no new
Objective-C runtime classes/selectors, globals, delegates or host-wide hooks;
all existing per-consumer class-prefix handling remains in effect. AppKit bridge
casts in the test are borrowed `__bridge` conversions under ARC.

The full package-consumer suite and non-macOS native visibility backends were not
run locally. No VoiceOver/native accessibility implementation is claimed.
Independent root review of the public API, ViewCore, Pugl shim and widget changes
found them coherent and requested the additional fault tests above. Oreto host-
format validation belongs to its integration change and is not implied here.

The native sanitizer run initially aborted before parent realization because the
sandbox exposed no `NSScreen` (the unchanged Release binary reproduced the same
NaN-frame AppKit exception). Running the same binaries outside the sandbox with
WindowServer access passed; no source workaround was added. T066 native runs
also used that graphical-session access. Headless/unit tests remain sandboxed.
