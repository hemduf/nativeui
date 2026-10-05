# Widget review and corrections

This review applies [CODE_REVIEW.md](../CODE_REVIEW.md) to PR #493 on
`codex/nativeui-widgets-and-gallery`. The inspected starting head was
`0f890150735c0b575dff0ca12cf0226c674f10a5`, based on the NativeUI inventory snapshot
`e10077ff39b8cb977669a7d5604562f66d07cb4c`.

**Qualification status:** corrected source is locally qualified: 384/384 Release tests,
20/20 ASan/UBSan tests, strict headless compilation and native gallery lifecycle pass.
Blocking findings: **0**. Important findings: **0**. The PR remains Draft for upstream
integration; this record does not qualify a merge with current main.

The corrected source/test diff from the starting head, produced by
`git diff --no-ext-diff --no-color --binary 0f890150735c0b575dff0ca12cf0226c674f10a5 -- include src tests`, has SHA-256
`ebc1d20a01b8f4267ff406ba89e05842b57b680fa236594b020f1f85a84a080b`.
Documentation-only completion does not change this fingerprint.

## Findings and corrections

Findings were collected before corrective edits. Regression cases were exercised
against the unchanged implementation or an isolated pre-correction implementation.
Existing assertions remain enabled. Severity refers to the confirmed behavior before correction, using CODE_REVIEW.md §1.

| Area | Severity | Confirmed defect | Correction and recovery oracle |
| --- | --- | --- | --- |
| `ForEach` | Blocking | Copying borrowed items could retire the input during a user copy, and excluded move-only item types. | Prepare owned keys and recipes inside State's guarded read frame. Reentrant writes wait until preparation finishes; failed preparation discards its own intent. Tests cover move-only values, replacement during key/recipe preparation, nested reads and later successful reconciliation. |
| `Switch` | Blocking | Copying a borrowed value could retire its input during the copy. | Use the existing guarded `snapshot()` contract; retain owned selection through branch comparison. Throwing-copy tests verify recovery without reading retired storage. |
| `TextArea` | Blocking | Pre-publication callbacks could replace the source, revoke editing or cancel recursively, then an older edit would overwrite that result. A skipped observer or failed invalidation left the draft stale. | Recheck source revision, edit generation and retained input permission at commit. A shared publication frame and checkpoint repair the buffer and line geometry without replaying setters or started callbacks. |
| `TokenField` | Important | Enter/navigation/deletion policy ran before the editor's active IME guard and could commit/delete tokens during preedit. | Consume composition keydowns; Escape cancels preedit only. Tests preserve committed tokens and verify a later ordinary submission. |
| `ColorPicker` | Important | Enter/Escape in the hexadecimal field could commit/cancel the draft during active composition. | Respect the editor's composition snapshot; Escape cancels only preedit. Tests verify later color publication. |
| `SplitView` publication | Important | Invalidation and callback-copy exposure could replace the source or revoke permission before an older write/callback began. | Use conditional publication, revision/generation checks and weak retained permissions across each exposure boundary. Terminal gestures disarm before fallible callback copies. |
| `SplitView` layout | Blocking | A failed staged geometry could later publish when a Collapsed pass skipped preparation. | Consume candidates only for matching committed bounds; discard them on collapse/unmount. Tests verify rollback, collapsed semantics, visible recovery and the next normal keyboard edit. |
| `Breadcrumbs` layout | Blocking | Fit/visibility published before the layout transaction succeeded. | Prepare an owned fit, publish through the no-throw geometry commit hook and discard stale/collapsed candidates. A failing sibling cannot publish a partial fit. |
| `Breadcrumbs` recovery | Blocking | Menu-close invalidation could throw after path commit and lose the unstarted structural/layout/availability suffix. | Journal effects before exposing closure, clear each started effect once and retain the suffix for a safe checkpoint. The close fault occurs before replacement children exist; recovery closes the menu and creates both new keyed children. |
| Lazy retained layout | Blocking | Geometry commit could leave visibility effects pending before direct painting or the first pointer hit test. | Drain retained effects after lazy geometry commit; prepare outer input geometry before acquiring its dispatch token. Direct raster painting and first-pointer tests avoid explicit resize/semantic queries masking the defect. |
| `ComboBox` lifecycle/routing | Important | Popup closure retained an opener latch/handle; blocked controls consumed unrelated Escape. | Release both opener latches on the physical key-up, clear closed-session handles and terminal focus state, and preserve unrelated-key routing. Repeated Escape and Tab/reopen tests cover the sequence. |
| `ComboBox` presentation/gestures | Blocking | Equal resolved states invalidated; changed row metrics missed layout invalidation. A failed pressed-style transition could leave the panel or anchor armed. | Classify resolved style changes, retain failed presentation work and clean up only the failing interaction epoch. Tests cover panel/anchor press/release failures, a newer nested gesture, terminal focus/deactivation faults and the next normal click. |
| `ComboBox` nested opening | Blocking | Provider invocation, callable copying, selection equality or invalidation could recursively open a newer popup, then an older attempt replaced its session or command. | Track an opening epoch through every exposure; prepare the Show command before publication and retain acquisition state through its acknowledgement. Tests verify exactly one newer popup and selection of its value, including provider-copy and equality reentry. |
| `StyleScope` allocation recovery | Blocking | A long inherited Theme copy could terminate at a string helper supplied by the pinned static Skia archive before recovery ran. | Reconstruct owned text/fallbacks from bytes at the internal copy boundary; preserve public signatures and dependency pins. Real allocation faults cover the family, fallback vector and both long fallback strings, followed by checkpoint recovery and another source update. |

The provisional collection hit-index finding was rejected: Tree refuses obsolete
geometry after a structural epoch change. Its regression checks the layout retry
and successful recovery; no collection production change was made for that hypothesis.

The StyleScope fault injector was also reviewed: its skip counter now advances
successful allocations before the selected failure. Four repetitions of the first
allocation are not counted as four independent failure stages.

## Mandatory review record

| Required field | Assessment |
| --- | --- |
| **CODE_REVIEW.md** | Completed for the corrected source; applicable executable qualification passes below. |
| **Instance isolation** | Corrected state, fit plans, recipes, gestures, permissions and retry flags are owned per component/UI compilation. Existing two-instance tests and gallery coverage remain enabled. |
| **Globals/statics** | No new mutable production global, singleton or thread-local state. Allocation counters are intentionally isolated test/benchmark executable seams. |
| **Threading/RT** | State and retained mutation remain UI-thread confined. No audio callback, plug-in adapter or shared cross-thread transport changes. |
| **Lifetime/reentrancy** | Detached publication state retains no raw component pointer; long-lived permissions/invalidators use Tree lifetime/identity guards. Source retirement and subtree removal suppress stale work. New nested gestures survive older-stack cleanup. Top-level UI/window destruction during its own callback remains deferred under the existing contract. |
| **Transactional state** | State read preparation pins storage and queues nested writes. Conditional edit commits verify revision/generation/permission. Breadcrumb fit and splitter geometry publish only after successful layout. Breadcrumb accepted effects have a durable unstarted suffix. |
| **Scheduling/queue failure** | No new Dispatcher queue or synchronous teardown fallback. Existing overlay command, descendant-action and timer rejection/throw recovery suites remain applicable; retained recovery never restarts callbacks that began. |
| **Exception/unwind** | Ordinary C++ exceptions propagate after guard/depth and current-gesture recovery. Geometry commit/availability hooks remain no-throw; cleanup contains release failures. Changed unmount/control cleanup remains no-throw and ordinary destructor-driven teardown starts no application callback. No foreign ABI thunk changes. |
| **Partial construction** | No new native resource acquisition. Existing mount/construction rollback and retained invalidator cleanup contracts apply. Owned vectors/recipes are prepared before publication. |
| **Objective-C runtime** | No Objective-C names/categories/selectors/ARC or native bridge changes. Existing consumer-specific prefixing is preserved. |
| **Platform integration** | Widgets remain independent of Pugl/AppKit/Win32/Xlib and plug-in SDKs. Logical coordinate contracts and pinned Pugl/Skia dependencies remain unchanged. Native gallery verification passes below; Windows/Linux execution has not run locally. |
| **Performance/allocation** | Fixed Release benchmarks plus active routing/lazy layout measurements below. The added successful dirty-layout checkpoint has a measured bounded cost; clean pointer routing adds no allocations. |
| **Privacy** | No personal data, credentials or user files introduced in production code, tests, gallery or generated project metadata. |
| **Tests** | Pass: exact focused, fault, full Release, multi-instance, headless, native window and ASan/UBSan checks below. Other platforms/package consumers remain explicit limits. |
| **Remaining findings** | None in corrected source: Blocking = 0, Important = 0. Upstream conflict integration and qualification of that integrated head remain merge gates. |

## Executable evidence

All local builds are serial with `CMAKE_BUILD_PARALLEL_LEVEL=1`. Temporary files use
`TMPDIR`, `TMP` and `TEMP` under `/Volumes/T7/tmp/`. Final sources are in the NativeUI
repository; temporary probes and logs are diagnostic artifacts only.

The five previously reported failures were reproduced before this correction batch.
The final qualification runs below use the corrected Core and final regression sources.

### Release and platform

```sh
export TMPDIR=/Volumes/T7/tmp TMP=/Volumes/T7/tmp TEMP=/Volumes/T7/tmp
cmake -S . -B build-widgets -DCMAKE_BUILD_TYPE=Release \
  -DNATIVEUI_BUILD_PLATFORM=ON -DNATIVEUI_BUILD_EXAMPLES=ON \
  -DNATIVEUI_BUILD_FEATURE_EXAMPLES=ON -DNATIVEUI_BUILD_TESTS=ON \
  -DNATIVEUI_BUILD_PACKAGE_TESTS=OFF -DNATIVEUI_ENABLE_INSPECTOR=OFF \
  -DNATIVEUI_ALLOWED_WARNINGS= \
  -DNATIVEUI_PUGL_SOURCE=/Volumes/T7/Code/nativeui/build/_deps/pugl_src-src \
  -DNATIVEUI_SKIA_ROOT=/Volumes/T7/Code/nativeui/build/_deps/skia_prebuilt-src
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build-widgets
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-widgets -R '^(nativeui_t035_combo_popup_tests|nativeui_text_area_tests|nativeui_widget_token_field_tests|nativeui_widget_color_picker_tests|nativeui_widget_split_callback_faults_tests|nativeui_widget_breadcrumbs_tests|nativeui_widget_dynamic_extraction_tests|nativeui_widget_state_owned_transactions_tests|nativeui_widget_style_scope_binding_recovery_tests|nativeui_widget_collection_transactions_tests|nativeui_example_t035_combo_popup_self_test|nativeui_example_t038_closure_invalidation_self_test|nativeui_example_t038_menu_item_invalidation_self_test|nativeui_example_widgets_gallery_self_test)$' --output-on-failure
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-widgets --output-on-failure
build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --window-self-test
c++ -std=c++20 -DNATIVEUI_EXAMPLE_SELF_TEST_ONLY -Iinclude -Iexamples/features \
  -isystem build/_deps/skia_prebuilt-src/include \
  -Wall -Wextra -Wpedantic -Werror -fsyntax-only examples/features/widgets_gallery.cpp
```

**Result:** full serial Release build passes with **zero warning diagnostics**:
Core, macOS platform, all examples/test executables and 83 public-header probes.
**14/14 focused CTest checks pass** (9.35 seconds).
After the Debug raster-accessor correction described below, the all-target build
and complete suite were rerun: **384/384 pass** (58.14 seconds).
No test is disabled or weakened. Installed-package contracts, inspector and
optional platform-smoke configuration remain disabled.

The gallery self-test renders all eight pages, compact/HiDPI layouts, overlays,
collection transitions and two independent instances. Its separate real macOS
window test reports first render and deferred closure success. The strict
Core-only syntax check above also passes.

The focused suites execute these specific recovery oracles, in addition to their
existing cases:

- `t035_combo_popup_tests`: `nested_provider_open`, `provider_copy_open`,
  `equality_open`, `invalidation_open`, failed panel/anchor down/up,
  `nested_press`, terminal focus/deactivation and repeated popup reopening.
- `widget_split_callback_faults_tests`: `collapsed_geometry`,
  `source_exposure`, `permission_exposure`, `change_copy_exposure`,
  throwing observers/callback copies, subtree removal and source retirement.
- `widget_breadcrumbs_tests`: `close_exposure`, `collapsed_fit`,
  `lazy_fit` and `lazy_pointer`. The raster case calls public painting directly;
  the pointer case performs no intervening resize, paint or semantic query.
- `widget_dynamic_extraction_tests` and `widget_state_owned_transactions_tests`:
  move-only items, copy/key/recipe retirement, guarded reads, nested failed frames
  and subsequent valid publication.
- `text_area_tests`, `widget_token_field_tests` and `widget_color_picker_tests`:
  source replacement, permission revocation, recursive cancellation, skipped
  notifications and synthetic IME transitions.
- `widget_style_scope_binding_recovery_tests`: earlier-observer exceptions and
  `callback_allocation`, exercising four genuine allocation stages.
- `widget_collection_transactions_tests`: rejected stale geometry followed by
  successful mapping/selection recovery.

### Sanitizers

```sh
cmake -S . -B build-review-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DNATIVEUI_BUILD_PLATFORM=OFF -DNATIVEUI_BUILD_EXAMPLES=OFF \
  -DNATIVEUI_BUILD_TESTS=ON -DNATIVEUI_ENABLE_SANITIZERS=ON \
  -DNATIVEUI_BUILD_PACKAGE_TESTS=OFF -DNATIVEUI_ENABLE_INSPECTOR=OFF \
  -DNATIVEUI_ALLOWED_WARNINGS= \
  -DNATIVEUI_SKIA_ROOT=/Volumes/T7/Code/nativeui/build/_deps/skia_prebuilt-src
CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build build-review-sanitize --target \
  nativeui_t035_combo_popup_tests \
  nativeui_text_area_tests \
  nativeui_widget_token_field_tests \
  nativeui_widget_color_picker_tests \
  nativeui_widget_split_callback_faults_tests \
  nativeui_widget_breadcrumbs_tests \
  nativeui_widget_dynamic_extraction_tests \
  nativeui_widget_state_owned_transactions_tests \
  nativeui_widget_style_scope_binding_recovery_tests \
  nativeui_widget_collection_transactions_tests \
  nativeui_state_tests \
  nativeui_widget_retained_checkpoint_tests \
  nativeui_widget_state_snapshot_copy_retirement_tests \
  nativeui_widget_state_key_retirement_tests \
  nativeui_widget_layout_structural_publication_tests \
  nativeui_widget_virtual_list_isolation_tests \
  nativeui_widget_virtual_list_row_faults_tests \
  nativeui_lifecycle_tests \
  nativeui_t061_overlay_acceptance_tests \
  nativeui_widget_overlay_ancestor_commands_tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-review-sanitize \
  -R '^(nativeui_t035_combo_popup_tests|nativeui_text_area_tests|nativeui_widget_token_field_tests|nativeui_widget_color_picker_tests|nativeui_widget_split_callback_faults_tests|nativeui_widget_breadcrumbs_tests|nativeui_widget_dynamic_extraction_tests|nativeui_widget_state_owned_transactions_tests|nativeui_widget_style_scope_binding_recovery_tests|nativeui_widget_collection_transactions_tests|nativeui_state_tests|nativeui_widget_retained_checkpoint_tests|nativeui_widget_state_snapshot_copy_retirement_tests|nativeui_widget_state_key_retirement_tests|nativeui_widget_layout_structural_publication_tests|nativeui_widget_virtual_list_isolation_tests|nativeui_widget_virtual_list_row_faults_tests|nativeui_lifecycle_tests|nativeui_t061_overlay_acceptance_tests|nativeui_widget_overlay_ancestor_commands_tests)$' --output-on-failure
```

**Result:** **20/20 pass** (3.46 seconds), with zero build warnings and no ASan/UBSan
diagnostic. The first run passed nineteen suites and exposed an incorrect `addr8()`
use in the new RGBA raster fixture. It now reads bytes through `SkPixmap::addr()`;
the complete pixel comparison remains unchanged. The whole twenty-suite selection
was rerun after rebuilding that test.

ASan and UBSan cover NativeUI-owned Core/tests. The existing configuration excludes
only the vptr check because pinned Skia binaries lack the required RTTI; prebuilt
third-party code is not instrumented. Leak detection is disabled on this macOS
executor, so no LSan qualification is claimed. No TSan claim is made: this batch
introduces no shared cross-thread state.

### Performance

The existing T051 fixed Release protocol uses five warmup and thirty measured samples.
An isolated comparison changes only the two Tree checkpoint headers over the same
baseline Core archive. All ten fixed workloads and the idle-invalidation contract
complete with unchanged allocation counts. Its inactive pointer fixtures do not
qualify active routing; separate active fixtures therefore assert that each event
is handled.

| Active workload | Baseline median | Corrected median | Allocations per operation, before → after |
| --- | ---: | ---: | ---: |
| Clean pointer, 101 nodes | 1.446 µs | 1.503 µs | 0 → 0 |
| Dirty lazy pointer layout, one StyleScope | 1.559 µs | 1.606 µs | 17 → 18 |
| Dirty lazy pointer layout, fifty StyleScopes | 30.679 µs | 37.555 µs | 360 → 361 |

The additional allocation snapshots opted-in retained participant IDs after a
successful dirty layout. Fifty scopes add approximately 6.9 µs in this small synthetic
case; this is a correctness checkpoint, not a claimed performance improvement.
These single local comparisons establish the exercised cost/allocation boundary,
not a cross-machine performance guarantee or release-wide benchmark qualification.

A separate Breadcrumbs comparison uses the same corrected public headers and backend
archives for both executables, and swaps the pre-correction/corrected Core archive.
Each operation invalidates layout and resizes between 300 and 320 logical pixels;
intrinsic label metrics are warmed. Fifty operations per sample exercise the same
five-warmup/thirty-sample protocol.

| Breadcrumbs resize workload | Baseline median | Corrected median | Allocations per operation, before → after |
| --- | ---: | ---: | ---: |
| Ten path items | 1.786 µs | 1.853 µs | 12 → 13 |
| One hundred path items | 23.435 µs | 23.469 µs | 12 → 13 |

Owned fit staging adds one allocation per exercised layout. Timing differences in
this local comparison are small and are not a performance-improvement claim.
Splitter candidates add fixed-size geometry storage, while ForEach retains owned
keys/recipes and avoids copying item values. No release-wide or other-platform
performance claim follows from these probes.

## Remaining integration and scope limits

The original branch snapshot remains behind `main`; GitHub reports merge conflicts.
Upstream edit-session, retained-tree and CMake changes require deliberate integration
and validation before merge. This corrective batch does not discard those changes or
claim the integrated `main` head is qualified.

Native IME/accessibility bridges, platform-specific acquisition/teardown and the
explicit future extensions in [implementation tracking](widgets_implementation.md)
retain their documented limits. Internal StyleScope recovery does not qualify every
allocation failure inside the prebuilt third-party archives. Full Windows/Linux,
package-consumer and native accessibility qualification is not inferred from Mac
Core/headless tests. The PR is not merged by this work.
