> **Historical note:** this document records the original validation campaign
> that used a build-private Pugl staging shim. Current NativeUI pins the corrected
> Pugl source directly and does not rewrite dependency sources during CMake
> configuration. The staging commands and contract-test references below are kept
> only as evidence of the original campaign.

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
Objective-C runtime classes, globals, delegates or host-wide hooks. Its two private
focus selectors derive their names from the existing consumer-prefixed wrapper
class; per-view window observers are removed before detach/destruction. AppKit bridge
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
also used that graphical-session access. Headless/unit tests passed in the
sandbox and do not require graphical-session access.

## Reproduction commands

Run from the NativeUI worktree. The relative dependency locations below describe
the existing cache layout used for these runs; substitute equivalent locations
containing the same pinned sources/archives if necessary. The compilation cache
location affects reuse only. Native test commands require an unlocked graphical
macOS session and access to WindowServer (outside a restrictive process sandbox).

```bash
pugl_source="$(cd ../nativeui/build/_deps/pugl_src-src && pwd)"
skia_root="$(cd ../nativeui/build/_deps/skia_prebuilt-src && pwd)"
export CCACHE_DIR="$PWD/.cache/ccache"
export CMAKE_BUILD_PARALLEL_LEVEL=1

# Reuse the existing CPM bootstrap for an offline configure.
mkdir -p build/cmake
cp ../nativeui/build/cmake/CPM_0.43.1.cmake build/cmake/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DNATIVEUI_BUILD_EXAMPLES=ON -DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON \
  -DNATIVEUI_PUGL_SOURCE="$pugl_source" -DNATIVEUI_SKIA_ROOT="$skia_root"
cmake --build build --target \
  nativeui_state_tests nativeui_edit_session_tests nativeui_widget_edit_tests \
  nativeui_component_state_tests nativeui_focus_tests nativeui_routing_tests \
  nativeui_pointer_capture_tests nativeui_gesture_tests nativeui_lifecycle_tests \
  nativeui_t043_view_geometry_tests nativeui_dynamic_composition_tests \
  nativeui_widget_tests nativeui_button_tests nativeui_checkbox_radio_tests \
  nativeui_t032_slider_value_contract nativeui_t032_slider_completion_tests \
  nativeui_t032_slider_visual_tests nativeui_embedded_visibility_tests \
  nativeui_smoke_standalone nativeui_smoke_embedded \
  nativeui_example_embedded_visibility nativeui_example_value_edit_sessions \
  nativeui_header_edit_compile nativeui_header_state_compile \
  nativeui_header_widgets_compile nativeui_header_window_compile
ctest --test-dir build --output-on-failure -R '^nativeui_(state_tests|edit_session_tests|widget_edit_tests|component_state_tests|focus_tests|routing_tests|pointer_capture_tests|gesture_tests|lifecycle_tests|t043_view_geometry_tests|dynamic_composition_tests|widget_tests|button_tests|checkbox_radio_tests|t032_slider_value_contract|t032_slider_completion_tests|t032_slider_visual_tests|embedded_visibility_tests|smoke_standalone|smoke_embedded|example_embedded_visibility_self_test|example_value_edit_sessions_self_test|objc_runtime_prefix|pugl_visibility_contract)$'
# Result: 24/24 passed; four public header compile targets passed.

mkdir -p build-sanitized/cmake
cp build/cmake/CPM_0.43.1.cmake build-sanitized/cmake/
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DNATIVEUI_BUILD_EXAMPLES=OFF -DNATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS=ON \
  -DNATIVEUI_ENABLE_SANITIZERS=ON \
  -DNATIVEUI_PUGL_SOURCE="$pugl_source" -DNATIVEUI_SKIA_ROOT="$skia_root"
cmake --build build-sanitized --target nativeui_edit_session_tests \
  nativeui_widget_edit_tests nativeui_embedded_visibility_tests nativeui_lifecycle_tests
ctest --test-dir build-sanitized --output-on-failure -R '^nativeui_(edit_session_tests|widget_edit_tests|embedded_visibility_tests|lifecycle_tests)$'
# Final result: 4/4 passed, zero sanitizer diagnostics (2.24 seconds).

mkdir -p build-t066/cmake
cp build/cmake/CPM_0.43.1.cmake build-t066/cmake/
cmake -S tests/t066 -B build-t066 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DNATIVEUI_PUGL_SOURCE="$pugl_source" -DNATIVEUI_SKIA_ROOT="$skia_root"
cmake --build build-t066 --target nativeui_t066_destroy_survivor_tests \
  nativeui_t066_window_controls_platform_tests
build-t066/nativeui_t066_destroy_survivor_tests
build-t066/nativeui_t066_window_controls_platform_tests
# Result: both PASS.
```

## Final independent review record

The root integration reviewer completed the original value-editing review of
source commit `46ee985fb21f1bee60b51a6830b14f3b5977ace6`. The separate native-focus
correction and its subsequent review are recorded below.

- **CODE_REVIEW.md:** completed.
- **Instance isolation:** pass. Per-session control and per-view visibility;
  sibling operation and destruction are exercised.
- **Globals/statics:** pass. None added; Pugl retains its existing final-consumer
  runtime prefixes.
- **Threading/RT:** pass. UI/main-thread only, with no audio semantics or transport.
- **Lifetime/reentrancy:** pass. Retained shared control protects edit-owner
  removal; whole UI/native owner destruction is explicitly deferred; invalidated
  Binding rejects editing.
- **Transactional state:** pass. Active state is published before begin; terminal
  state is inactive before callbacks. Failure cancels editing and restores
  capture. Hide commits hidden state and completes pointer/focus/IME cleanup
  despite callback failures.
- **Scheduling/queue failure:** not applicable. No new queue or deferred fallback;
  the existing ViewCore Dispatcher remains retained.
- **Exception/unwind:** pass. Direct errors propagate after cleanup; terminals
  are never retried; destructor cancellation contains exceptions. The existing
  Pugl foreign-callback boundary remains unchanged.
- **Partial construction:** pass. Existing RAII ownership of world, view and
  rendering context is unchanged and exercised by T066 acquisition failures,
  throwing teardown and surviving siblings.
- **Objective-C runtime:** pass. No new classes or categories. The original
  visibility correction added no selectors; the subsequent focus correction below
  adds two private selectors with final-consumer prefixing.
- **Platform integration:** pass on macOS arm64. Hidden Cocoa child, sibling and
  parent isolation, focus/IME cleanup and repeated lifecycle are tested.
- **Performance/allocation:** pass. Session bookkeeping allocates one small
  control block per owner, with no new allocation per scalar value update.
  No redraw loop was added.
- **Privacy:** pass. No personal information introduced.
- **Tests:** exact commands and results are recorded above, including targeted,
  failure, applicable regression, multi-instance and native sanitizer checks.
- **Remaining findings:** none. Blocking: 0. Important: 0.

This qualifies macOS delivery only. Windows/Linux native behavior and installed
package consumers remain unvalidated locally; the local results are not a
cross-platform qualification.

## macOS embedded input correction

The visible Oreto Standalone exposed a missing native input lifecycle: the image
rendered, but every click and key was ignored. `Tree::dispatch` requires an active
tree, while the pinned Cocoa Pugl implementation emitted focus events only from
its own top-level window delegate. A child in a host-owned window never received
that activation. Earlier visibility tests explicitly activated the UI and thus
did not establish native input delivery.

The staged Pugl correction now connects the embedded wrapper's responder and
key-window transitions to Pugl focus events. Passive construction/show preserve
the host responder; a user click acquires it before input delivery. Two targeted
window notifications belong to each wrapper, are disconnected before moving to
another window, and are removed at destruction. No host delegate, AppKit category,
global registry or OS event injection is introduced. The two new private selectors
derive their names from the existing final-consumer-prefixed wrapper class; the
linked test binary contains the expected prefixed selectors.

The focus flag is committed before callbacks. Native callback errors remain
contained by ViewCore's existing terminal-error boundary. Direct `hide()` retains
its exception-preserving cleanup and suppresses duplicate native focus-out
delivery. Public APIs and dependency pins are unchanged.

The new AppKit test uses a plain host-owned `NSWindow`, asserts it is key, keeps a
host text responder across hidden creation/passive show, and sends real `NSEvent`
objects through `NSWindow::sendEvent`. It covers mouse click, Tab/Return traversal,
host responder transfer, switching between two actual windows, hide/show, switching
between two embedded instances, destruction during a press, and a surviving child.
Separate native transitions inject cancel, blur and deactivate exceptions and
verify complete cleanup, at-most-once cancellation and a working sibling. No test
calls `UI::activate` for these native input checks.

Current targeted validation (macOS arm64, Release):

```bash
export CCACHE_DIR="$PWD/.cache/ccache"
export CMAKE_BUILD_PARALLEL_LEVEL=1
cmake --build build --target nativeui_embedded_visibility_tests
ctest --test-dir build --output-on-failure -R '^nativeui_(embedded_visibility_tests|pugl_visibility_contract)$'
# Result: 2/2 passed, no compiler warnings.
```

The final test fixture was also linked against the preceding staged Pugl source
from `7fd70f8710088cb1c5082e595df6e8ef99cb519e` as a negative control. Its key-window
precondition passed, then the first mouse click failed to invoke the button.
Restoring the corrected staged source passed the same test. Only the generated
build-tree `mac.m` was replaced for this control; tracked sources and the shared
Pugl dependency were unchanged.

Final regression and sanitizer commands used the existing configurations above:

```bash
export CCACHE_DIR="$PWD/.cache/ccache"
export CMAKE_BUILD_PARALLEL_LEVEL=1
cmake --build build --target nativeui_embedded_visibility_tests \
  nativeui_smoke_standalone nativeui_smoke_embedded
ctest --test-dir build --output-on-failure -R '^nativeui_(state_tests|edit_session_tests|widget_edit_tests|component_state_tests|focus_tests|routing_tests|pointer_capture_tests|gesture_tests|lifecycle_tests|t043_view_geometry_tests|dynamic_composition_tests|widget_tests|button_tests|checkbox_radio_tests|t032_slider_value_contract|t032_slider_completion_tests|t032_slider_visual_tests|embedded_visibility_tests|smoke_standalone|smoke_embedded|example_embedded_visibility_self_test|example_value_edit_sessions_self_test|objc_runtime_prefix|pugl_visibility_contract)$'
# Result: 24/24 passed (5.18 seconds).
cmake --build build-sanitized --target nativeui_embedded_visibility_tests
ctest --test-dir build-sanitized --output-on-failure -R '^nativeui_(edit_session_tests|widget_edit_tests|embedded_visibility_tests|lifecycle_tests)$'
# Result outside the restrictive process sandbox: 4/4 passed (1.32 seconds),
# zero ASan/UBSan diagnostics. No sanitizer options were suppressed or changed.
git diff --check
```

The initial sanitizer attempt inside the sandbox failed its key-window
precondition with AppKit XPC/WindowServer `Connection invalid` diagnostics.
Running the same binaries and command with graphical-session access passed.
This failure preceded input delivery and was not treated as a product-test pass.

The root reviewer independently inspected the final focus source and expanded
tests, with no Blocking or Important findings. Review record for this correction:

- **CODE_REVIEW.md:** completed.
- **Instance isolation / globals:** pass. Focus flags and observer lifetimes are
  per wrapper; no mutable global state was added. Native A-to-B transfer and
  survivor operation after A closes or throws are tested.
- **Threading/RT:** pass. Cocoa/UI-main-thread only; no audio or transport changes.
- **Lifetime/reentrancy:** pass. The wrapper borrows its host window, removes
  scoped observers before detach, and unregisters at destruction. No host
  delegate is replaced. Synchronous top-level owner destruction from native
  callbacks remains unsupported and must be deferred.
- **Transactional state / exception unwind:** pass. Focus state is published
  before callbacks. Native errors are contained at the existing Pugl thunk and
  leave that view terminal; cancel/blur/deactivate cleanup and surviving-instance
  operation are tested. Direct hide preserves its original error after cleanup.
- **Scheduling/queue failure:** not applicable; no new queues or deferred fallback.
- **Partial construction:** pass by ownership review. Registrations belong to the
  wrapper from acquisition, with cleanup in window detachment and deallocation;
  existing ViewCore RAII releases the wrapper on construction failure. The T066
  baseline above was not rerun for this follow-up.
- **Objective-C runtime:** pass. Existing consumer-prefixed class; two new private
  selectors derive the same prefix, confirmed in the linked binary. No categories,
  swizzling, global notification handler, or application/host delegate mutation.
- **Platform integration:** pass on macOS arm64, including real Cocoa input,
  focus loss, window switching, repeated child lifecycle and active teardown.
- **Performance/allocation:** pass. Two scoped observer registrations per attached
  wrapper, one focus flag; no per-event allocation or continuous redraw added.
- **Privacy:** pass. No personal data introduced.
- **Tests:** negative control, Release 24/24 and ASan/UBSan 4/4 as above.
- **Remaining findings:** none. Blocking: 0. Important: 0.

This follow-up remains macOS-only validation. Windows/Linux and installed-package
consumer qualification are unchanged and not implied by these results.
