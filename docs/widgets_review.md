# Widget review and corrections

This review applies [CODE_REVIEW.md](../CODE_REVIEW.md) to PR #493 on
`codex/nativeui-widgets-and-gallery`. The inspected starting head was
`0f890150735c0b575dff0ca12cf0226c674f10a5`, based on the NativeUI inventory snapshot
`e10077ff39b8cb977669a7d5604562f66d07cb4c`.

## Historical main integration on October 5, 2026

Merge `0666bc591b14f8f0b73154104d1edfd3e9bb3554` integrates main
`6c850ab044ba99b7ccbea7076765cd4efe722adb` into the widget branch. All thirteen
conflicted paths are resolved. The integration source was **`2c36d801d417c78824dc11961f258b3fe3808ab9`**;
later source corrections and qualification are recorded above this historical section.
The extracted file pairs
and RichText crash correction coexist with main's rendering resources/raster
caches, ScalarSource, focus geometry/teardown guards and edit sessions. Pugl uses
main's exact pin `a4bdafe38f48cf906560e40bd1e9e87986369b06` with the Cocoa
embedded-focus hooks; dependency versions are not substituted.

Knob, Slider and Toggle preserve upstream `on_edit`; Knob/Slider wheel input stays
opt-in. TextInput preserves `on_key_down` after the IME guard. Historical
constructors, includes, State/Binding overloads and template adapters remain.
Owned sessions and originating-contact release actions protect subtree retirement,
exception recovery and newer reentrant contacts.

### Integration findings and recovery

- **Blocking — EditSession guarded reads:** strict standalone `set`, `begin` and
  `update` selectors reproduce an edit queued after a rejected State read.
  Reject before any callback/write. Rejection stays silent; the active edit
  survives, and the next ordinary edit produces `BCE`.
- **Blocking — contact cleanup:** Slider/Toggle keyboard cancellation/failure
  originally leaves touch 7 captured. Knob's existing focus-fault regression also
  exposes capture left active when invalidation throws; a new Knob selector
  reproduces the same keyboard/touch failure. Move/release the originating action
  on every failure path while preserving an independent mouse capture. Each
  corrected case proves a subsequent normal edit.
- **Blocking — Knob newer contact:** the unchanged lifetime selector reproduces
  value `0.6` instead of `0.7` after a newer PointerDown inside a throwing State
  observer. Its physical contact survives, but starting the logical edit during
  notification is correctly rejected. Start it on the next valid move and
  recheck owned generation, Binding, gesture and permission before publication.
- **Blocking — Toggle borrowed invalidation:** a layout callback removes the
  subtree before a second borrowed paint invalidation. Stop after layout
  invalidation; the original probe fails before correction and remount/edit
  succeeds afterward.
- **Blocking — build compatibility:** strict GCC rejects a partial ToggleStyle
  fixture initializer; initialize the same style explicitly. Full macOS build
  exposes duplicate SVG linkage in an isolated ScalarSource target and the two
  Toast headless fixture calls missing the new private resource-hook argument.
  Rely on Core's public SVG dependency and pass `nullptr` for the headless hook.
  Public APIs, dependency pins, warning allowance and assertions are preserved.
- **Important — overlay fault location:** the strengthened T063 fixture's
  unconditional close fault now hits extracted panels' pre-action paint
  invalidation, before any close command is accepted. Use the public Rect
  invalidation overload to fault the full-viewport structural notification;
  guard owned popup-bound snapshots before inspection. Show failure remains
  unconditional, and every original close/retry/commit-count assertion stays.

The first integrated full run at `abcab1d1` reports **406/408**, exposing the Knob
focus-capture failure and T063 fault-location mismatch. Both are corrected; the
additional unchanged/new Knob probes also execute RED before correction. The
three focused Knob/edit-retirement/dialog checks then pass **3/3** (1.90 CTest
seconds), without warnings. Independent final source/fixture re-review reports
**0 Blocking / 0 Important**. The earlier eight focused integration suites and
exact pre-merge `99933d32` baseline (zero warnings, **384/384**) keep their original
source identities; they are not substituted for final qualification.

### Final local qualification

At `2c36d801d417c78824dc11961f258b3fe3808ab9`, the complete serial Release rebuild passes with **zero warnings**
(91.63 seconds), including Core, platform, all examples/tests and
**120 standalone public-header probes**. The complete suite passes **408/408**
(219.38 wall seconds), including 221 unit suites and both embedded
keyboard fixtures, with **no skips**. The final sanitizer build passes with zero
warnings (32.69 seconds); **24/24 ASan/UBSan suites pass**
(32.88 wall seconds), without diagnostics or skips. Seven additional
native checks pass: embedded visibility, per-view resource recreation/isolation,
ScalarSource and FractalNoise GPU references, T095/T096 scene recovery, and the
real gallery's first render/deferred close.

Linux Core CI is **still running** on this same source revision and remains a merge gate. The PR stays Draft while that qualification is active. The exact run is
[Linux Core smoke #37332592202](https://github.com/hemduf/nativeui/actions/runs/37332592202).
Earlier failed/cancelled runs are not final evidence. Linux's first strict-GCC
failure prompted the style initializer correction; every executable correction
is followed by dispatch on its new source SHA.

Release configuration enables platform, feature examples and tests, while
`NATIVEUI_BUILD_PACKAGE_TESTS`, `NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS`, inspector
and sanitizers are OFF. The two keyboard fixtures remain registered; the seven
additional native checks above are run explicitly. Debug sanitizer configuration
has platform/examples/package tests/inspector OFF and sanitizers/tests ON. Both
use the pinned prebuilt Skia at
`/Volumes/T7/Code/nativeui/build/_deps/skia_prebuilt-src`; Release leaves
`NATIVEUI_PUGL_SOURCE` empty so CPM acquires the exact upstream pin. Both keep the
default empty `NATIVEUI_ALLOWED_WARNINGS`.

No Windows/native Linux, complete IME, OS accessibility, installed-package,
inspector, LSan or TSan qualification is inferred. The existing Skia vptr
exclusion and uninstrumented prebuilt archives remain limits; macOS leak
checking is disabled. Live keyboard fixtures do not establish a real DAW test.
The 83 file pairs do not claim completion of every future specification contract.

Documentation verification covers **95 Markdown pages**, **862 valid local file links**
and all **83 eleven-section specifications with matching header/source pairs**.
No missing link, file pair or section is found.

### October 5 integration review record

| Required field | Assessment |
| --- | --- |
| **CODE_REVIEW.md** | Applicable integration review completed; independent tree/widget and final Knob/fixture re-reviews report 0 Blocking / 0 Important. Historical component review retains its source identity. |
| **Instance isolation** | Pass. Sessions, generations, retained identities and contact release actions belong to each component/UI. Independent mouse/touch capture, two-model gallery and two-view GPU recovery checks pass. |
| **Globals/statics** | Pass. No new mutable production global, singleton or thread-local state; existing resource ownership boundaries are preserved. |
| **Threading/RT** | Pass. State, retained trees, input and rendering remain UI-thread confined. No audio callback, plug-in-format API or cross-thread transport changes. |
| **Lifetime/reentrancy** | Pass. Owned guards suppress stale writes/callbacks after subtree removal and preserve newer contacts. Knob resumes a newer physical drag only after its earlier State transaction finishes. Borrowed contexts are not reused after exposed callbacks. Whole-owner destruction follows the existing deferred contract, except explicitly qualified safe public-entry paths. |
| **Transactional state** | Pass. Guarded reads/notifications reject edits before callbacks or queued writes. Layout rollback, retained checkpoints and raster/resource cache invalidation coexist. Failed overlay preparation leaves accepted work available for one later retry. |
| **Scheduling/queue failure** | Pass. Existing accepted overlay/descendant/timer recovery remains enabled. No synchronous teardown fallback, replay of started callbacks or new Dispatcher queue. |
| **Exception/unwind** | Pass. Originating contacts release even when invalidation throws; secondary release/cancel failures are contained and the first error propagates. Matching generations protect newer interactions. Unmount/destructor cancellation is contained; foreign ABI boundaries are unchanged. |
| **Partial construction** | Pass for the integration scope. Existing native acquisition rollback and registration ownership remain; edit/session ownership is prepared before exposure. No new native acquisition step. |
| **Objective-C runtime** | Pass for preserved integration. Consumer-specific Objective-C bridge prefixing and upstream Cocoa focus hooks remain; no new runtime class/category or shared bridge. |
| **Platform integration** | Pass locally. Final macOS embedded keyboard/visibility, per-view resource recreation, GPU references, scene recovery and gallery startup/closure checks pass. Focused Linux Core CI is pending at the exact executable source SHA; it is a merge gate. |
| **Performance/allocation** | Reasoned impact: cancellation moves existing release actions; pointer publication adds an active-edit check and starts a deferred logical edit only when needed. No new steady-state allocation or layout staging is introduced by this correction. Historical benchmark numbers are not relabeled as measurements of the integrated source. |
| **Privacy** | Pass. No personal data, credentials or user-file content introduced in production sources, tests, examples or generated project metadata. |
| **Tests** | Final serial Release build: zero warnings; 408/408 CTest checks, no skips. Final targeted ASan/UBSan build: zero warnings; 24/24 suites, no diagnostics or skips. Seven additional native checks pass. Exact source, commands and limits are recorded below. |
| **Remaining findings** | No remaining Blocking/Important finding in the bounded integration review. Linux Core qualification remains a merge gate; future contracts and platform limits stay explicit. |

### Final executable commands

All local builds and tests are serial. Temporary diagnostics use `/Volumes/T7/tmp`;
final implementation files remain in this repository.

```bash
export TMPDIR=/Volumes/T7/tmp TMP=/Volumes/T7/tmp TEMP=/Volumes/T7/tmp
export CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1
cmake --build build-widgets
ctest --test-dir build-widgets --output-on-failure
sanitizer_targets=(
  nativeui_edit_session_tests
  nativeui_edit_read_transaction_tests
  nativeui_widget_edit_tests
  nativeui_widget_edit_retirement_tests
  nativeui_text_input_tests
  nativeui_widget_text_input_key_retirement_tests
  nativeui_widget_rich_text_tests
  nativeui_widget_rich_text_layout_tests
  nativeui_widget_rich_text_recovery_tests
  nativeui_widget_rich_text_paint_tests
  nativeui_widget_rich_text_contact_generation_tests
  nativeui_component_state_tests
  nativeui_routing_tests
  nativeui_focus_tests
  nativeui_paint_culling_tests
  nativeui_layout_constraints_tests
  nativeui_lifecycle_tests
  nativeui_t067_retained_tests
  nativeui_widget_retained_checkpoint_tests
  nativeui_widget_focus_reason_tests
  nativeui_raster_cache_epoch_tests
  nativeui_raster_cache_tree_tests
  nativeui_widget_toast_quiet_paint_tests
  nativeui_widget_knob_lifetime_tests
)
cmake --build build-review-sanitize --target "${sanitizer_targets[@]}"
printf -v sanitizer_pattern '%s|' "${sanitizer_targets[@]}"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-review-sanitize --output-on-failure \
  -R "^(${sanitizer_pattern%|})$"
for target in nativeui_embedded_visibility_tests nativeui_render_resource_gpu_tests \
  nativeui_scalar_source_gpu_reference_tests nativeui_fractal_noise_gpu_reference_tests \
  nativeui_t095_scene_gpu_tests nativeui_t096_scene_gpu_tests; do
  "build-widgets/$target"
done
build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --window-self-test
```

The source/test/example diff from `0f890150`, produced by
`git diff --no-ext-diff --no-color --binary 0f890150735c0b575dff0ca12cf0226c674f10a5 -- include src tests examples`,
has SHA-256 **`4043a3cd89ed7770064b2ad3eea5b99c40950eabb6c2c8973f176f076b947617`**. Historical evidence below retains its
original source identity.

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
| GCC guard formatting | Blocking | Seven guards in the virtual-list retained header triggered `-Werror=misleading-indentation`; the same pattern occurred in Dialog action rows. | Separate each guard from its unconditional successor. Independent review confirms the non-whitespace source sequence is unchanged; no warning policy changes. |
| Calendar format capacity | Blocking | GCC could not infer the valid-date guard when diagnosing `snprintf` into eleven bytes. | Use sixteen bytes: the full inferred chrono fields require at most fifteen including NUL. The valid-date guard, format, public output and allocation behavior are unchanged. |
| Sidebar style initialization | Blocking | GCC reported optional hover-style payload fields as possibly uninitialized in the optimized Release comparison. | Use fully initialized owned style values with the same old/new row guards. Resolver calls, hover commit, short-circuit comparisons and invalidation ordering are preserved; absent values are never compared. |
| Partial aggregate defaults | Blocking | GCC rejected omitted optional/string/callback fields in designated style/spec initializers under `-Werror=missing-field-initializers`. | Give 27 members explicit empty defaults across CollapsibleStyle, AccordionStyle, PopoverStyle, RichTextSpan and ToastSpec. Member types/order and aggregate APIs remain unchanged; all existing partial initializer fixtures remain intact. An independent review and baseline/corrected ABI-traits probe confirm equivalent defaults, sizes, alignments and C++20 aggregate support. |
| Converting range-loop values | Blocking | GCC rejected a const string reference bound to a temporary converted from each character-pointer element in the NumberInput fixture. The same pattern appeared in the Toolbar example. | Use an explicitly owned const string value for both loops. The same string is constructed once per element, and all input values, assertions, callbacks and ownership remain unchanged. |
| `RichText` borrowed input lifetime | Blocking | Copying an action callback can remove its node; activation then invokes the borrowed InputContext invalidator referencing that freed node. Linux recovery segfaults and macOS ASan confirms heap-use-after-free. | Recheck owned generations, mounted state and permissions before borrowed invalidation after callback copy/release, and after pointer capture/focus. Preserve contact-specific cleanup for newer nested gestures. The two original paths reproduce ASan UAF before correction; expanded keyboard/pointer/semantic retirement tests verify suppressed stale actions, later successful activation and balanced capture. Five RichText sanitizer suites pass. |
| English example policy | Important | 33 component examples still exposed French labels; a legacy window-control title and ten regression fixture files also contained French prose. | Translate 34 example files and ten regression fixture files, including matching literal-based and semantic-name expectations. Keep stable IDs, callbacks, code paths and intentional multilingual test data. Static comparison confirms all non-literal source tokens are unchanged. |

The provisional collection hit-index finding was rejected: Tree refuses obsolete
geometry after a structural epoch change. Its regression checks the layout retry
and successful recovery; no collection production change was made for that hypothesis.

The StyleScope fault injector was also reviewed: its skip counter now advances
successful allocations before the selected failure. Four repetitions of the first
allocation are not counted as four independent failure stages.

## Historical mandatory review record (`615a8b83`)

| Required field | Assessment |
| --- | --- |
| **CODE_REVIEW.md** | Applicable source review is recorded below; the latest bounded RichText crash correction passes independent review and local qualification. Complete qualification of an integrated main head remains pending. |
| **Instance isolation** | Corrected state, fit plans, recipes, gestures, permissions and retry flags are owned per component/UI compilation. Existing two-instance tests and gallery coverage remain enabled. |
| **Globals/statics** | No new mutable production global, singleton or thread-local state. Allocation counters are intentionally isolated test/benchmark executable seams. |
| **Threading/RT** | State and retained mutation remain UI-thread confined. No audio callback, plug-in adapter or shared cross-thread transport changes. |
| **Lifetime/reentrancy** | Detached publication state retains no raw component pointer; long-lived permissions/invalidators use Tree lifetime/identity guards. Source retirement and subtree removal suppress stale work. RichText also checks lifetime before reusing borrowed event invalidators after callback-copy, focus/capture and release exposure. New nested gestures survive older-stack cleanup. Top-level UI/window destruction during its own callback remains deferred under the existing contract. |
| **Transactional state** | State read preparation pins storage and queues nested writes. Conditional edit commits verify revision/generation/permission. Breadcrumb fit and splitter geometry publish only after successful layout. Breadcrumb accepted effects have a durable unstarted suffix. |
| **Scheduling/queue failure** | No new Dispatcher queue or synchronous teardown fallback. Existing overlay command, descendant-action and timer rejection/throw recovery suites remain applicable; retained recovery never restarts callbacks that began. |
| **Exception/unwind** | Ordinary C++ exceptions propagate after guard/depth and current-gesture recovery. Geometry commit/availability hooks remain no-throw; cleanup contains release failures. Changed unmount/control cleanup remains no-throw and ordinary destructor-driven teardown starts no application callback. No foreign ABI thunk changes. |
| **Partial construction** | No new native resource acquisition. Existing mount/construction rollback and retained invalidator cleanup contracts apply. Owned vectors/recipes are prepared before publication. |
| **Objective-C runtime** | No Objective-C names/categories/selectors/ARC or native bridge changes. Existing consumer-specific prefixing is preserved. |
| **Platform integration** | Widgets remain independent of Pugl/AppKit/Win32/Xlib and plug-in SDKs. Logical coordinate contracts and pinned Pugl/Skia dependencies remain unchanged. Native gallery verification passes below; Windows/Linux execution has not run locally. |
| **Performance/allocation** | Fixed Release benchmarks plus active routing/lazy layout measurements below. The added successful dirty-layout checkpoint has a measured bounded cost; clean pointer routing adds no allocations. |
| **Privacy** | No personal data, credentials or user files introduced in production code, tests, gallery or generated project metadata. |
| **Tests** | Latest RichText ASan/UBSan 5/5, Release 6/6 and relinked headless/native gallery pass. Earlier broad Mac 384/384, 66/66 and 27/27 describe cb601757; its Linux build passes but RichText recovery fails (202/203 units). Latest complete Linux and integrated-head qualification are not inferred from these targeted reruns. |
| **Remaining findings** | All confirmed findings have source corrections; bounded RichText independent review reports 0 Blocking / 0 Important. Complete Linux and broader integrated-head qualification remain merge gates. The non-blocking coverage follow-up below is explicit. |

## Executable evidence

### RichText crash correction (`615a8b83`)

The existing callback-copy retirement case reproduced ASan heap-use-after-free
at the borrowed paint invalidator, called from `activate_action`. The added
`focus_removal` selector separately reproduced the same error after the retained
focus request removed the paragraph during PointerDown. Both RED runs used
unchanged production code; the test executable returned SIGABRT with an ASan
freed-node backtrace. No assertion or test was disabled.

The correction checks lifetime before the next borrowed context operation.
Generation and originating-contact checks after capture/focus preserve a newer
nested gesture; post-invalidation checks still suppress activation if invalidation
itself removes the action. Related release/blur paths use the same lifetime rule.
The regression covers keyboard, pointer and direct component semantic activation,
then remounts and successfully activates the replacement. Direct semantic delivery
does not qualify an OS accessibility bridge. Platform fixtures outlive their UIs.

Using the existing build configurations described below:

```bash
export TMPDIR=/Volumes/T7/tmp TMP=/Volumes/T7/tmp TEMP=/Volumes/T7/tmp
export CMAKE_BUILD_PARALLEL_LEVEL=1 CTEST_PARALLEL_LEVEL=1
cmake --build build-review-sanitize --target \
  nativeui_widget_rich_text_tests nativeui_widget_rich_text_layout_tests \
  nativeui_widget_rich_text_recovery_tests nativeui_widget_rich_text_paint_tests \
  nativeui_widget_rich_text_contact_generation_tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-review-sanitize --output-on-failure \
  -R '^nativeui_widget_rich_text(_(layout|recovery|paint|contact_generation))?_tests$'
cmake --build build-widgets --target \
  nativeui_widget_rich_text_tests nativeui_widget_rich_text_layout_tests \
  nativeui_widget_rich_text_recovery_tests nativeui_widget_rich_text_paint_tests \
  nativeui_widget_rich_text_contact_generation_tests nativeui_example_rich_text
ctest --test-dir build-widgets --output-on-failure \
  -R '^(nativeui_widget_rich_text(_(layout|recovery|paint|contact_generation))?_tests|nativeui_example_rich_text_self_test)$'
cmake --build build-widgets --target nativeui_example_widgets_gallery
ctest --test-dir build-widgets --output-on-failure \
  -R '^nativeui_example_widgets_gallery_self_test$'
build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --window-self-test
```

**Result:** sanitizer build passes with zero warnings (25.82 seconds), then
**5/5 suites pass** (7.29 CTest seconds), without sanitizer diagnostics. Release
build passes with zero warnings (12.93 seconds), then **6/6 checks pass**
(3.99 CTest seconds). Gallery relink passes (1.54 seconds), its eight-screen
self-test passes (3.21 CTest seconds) and its native window renders and closes
(0.56 seconds). The original contact-generation success/failure cases remain
enabled. Independent read-only review reports no Blocking or Important finding
in this bounded correction. The existing Skia vptr exclusion, uninstrumented
third-party archives and disabled macOS leak detection remain explicit limits.

All local builds are serial with `CMAKE_BUILD_PARALLEL_LEVEL=1`. Temporary files use
`TMPDIR`, `TMP` and `TEMP` under `/Volumes/T7/tmp/`. Final sources are in the NativeUI
repository; temporary probes and logs are diagnostic artifacts only.

The five previously reported failures were reproduced before this correction batch.
The earlier broad qualification runs below use Core and regression sources at
`cb601757`, before the bounded RichText crash correction above.

### Earlier Release and platform (`cb601757`)

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
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-widgets -R '^(nativeui_t035_combo_popup_tests|nativeui_text_area_tests|nativeui_widget_token_field_tests|nativeui_widget_color_picker_tests|nativeui_widget_split_callback_faults_tests|nativeui_widget_breadcrumbs_tests|nativeui_widget_dynamic_extraction_tests|nativeui_widget_state_owned_transactions_tests|nativeui_widget_style_scope_binding_recovery_tests|nativeui_widget_collection_transactions_tests|nativeui_example_t035_combo_popup_self_test|nativeui_example_t038_closure_invalidation_self_test|nativeui_example_t038_menu_item_invalidation_self_test|nativeui_example_widgets_gallery_self_test|nativeui_widget_calendar_tests|nativeui_widget_date_input_tests|nativeui_widget_sidebar_tests|nativeui_widget_sidebar_publication_tests|nativeui_widget_dialog_extensions_tests|nativeui_example_autocomplete_self_test|nativeui_example_breadcrumbs_self_test|nativeui_example_calendar_self_test|nativeui_example_checkbox_group_self_test|nativeui_example_color_picker_self_test|nativeui_example_color_well_self_test|nativeui_example_combo_box_self_test|nativeui_example_context_menu_self_test|nativeui_example_date_input_self_test|nativeui_example_dialog_self_test|nativeui_example_editable_combo_box_self_test|nativeui_example_editable_text_self_test|nativeui_example_field_self_test|nativeui_example_fieldset_self_test|nativeui_example_form_self_test|nativeui_example_history_button_self_test|nativeui_example_list_view_self_test|nativeui_example_outline_table_view_self_test|nativeui_example_outline_view_self_test|nativeui_example_popover_self_test|nativeui_example_popup_menu_self_test|nativeui_example_rating_self_test|nativeui_example_rich_text_self_test|nativeui_example_search_field_self_test|nativeui_example_segmented_control_self_test|nativeui_example_t066_window_controls_self_test|nativeui_example_table_view_self_test|nativeui_example_time_input_self_test|nativeui_example_toast_self_test|nativeui_example_toggle_group_self_test|nativeui_example_token_field_self_test|nativeui_example_toolbar_self_test|nativeui_example_tooltip_self_test|nativeui_example_tree_view_self_test|nativeui_widget_collapsible_tests|nativeui_widget_disclosure_faults_tests|nativeui_widget_popover_tests|nativeui_widget_rich_text_tests|nativeui_widget_toast_tests|nativeui_widget_number_input_tests|nativeui_widget_input_transactions_tests|nativeui_widget_editable_text_tests|nativeui_widget_header_linkage_tests|nativeui_widget_search_field_tests|nativeui_widget_rating_tests|nativeui_widget_value_publication_tests|nativeui_widget_reactivation_tests)$' --output-on-failure
CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-widgets --output-on-failure
build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --window-self-test
c++ -std=c++20 -DNATIVEUI_EXAMPLE_SELF_TEST_ONLY -Iinclude -Iexamples/features \
  -isystem build/_deps/skia_prebuilt-src/include \
  -Wall -Wextra -Wpedantic -Werror -fsyntax-only examples/features/widgets_gallery.cpp
```

**Result at `cb601757`:** the complete serial Release build passes with
**zero warning diagnostics**: Core, macOS platform, all examples/test executables
and 83 component public-header probes. The header-dependent rebuild is followed
by a full incremental build at the frozen final source; that final step passes in
50.38 seconds. **66/66 focused CTest checks pass** (34.76 seconds): 38 public examples
and 28 unit suites. The complete suite then passes **384/384** (188.79 seconds),
including 204 unit checks. No test is disabled or weakened. Installed-package
contracts, inspector and optional platform-smoke configuration remain disabled.

The gallery self-test renders all eight pages, compact/HiDPI layouts, overlays,
collection transitions and two independent instances. Its separate real macOS
window test reports first render and deferred closure success (0.52 seconds). The
strict Core-only syntax check above also passes (2.83 seconds).

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

### Earlier sanitizer selection (`cb601757`)

```sh
export TMPDIR=/Volumes/T7/tmp TMP=/Volumes/T7/tmp TEMP=/Volumes/T7/tmp
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
  nativeui_widget_overlay_ancestor_commands_tests \
  nativeui_widget_calendar_tests \
  nativeui_widget_date_input_tests \
  nativeui_widget_sidebar_tests \
  nativeui_widget_sidebar_publication_tests \
  nativeui_widget_dialog_extensions_tests \
  nativeui_widget_collapsible_tests \
  nativeui_widget_disclosure_faults_tests
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  CTEST_PARALLEL_LEVEL=1 ctest --test-dir build-review-sanitize \
  -R '^(nativeui_t035_combo_popup_tests|nativeui_text_area_tests|nativeui_widget_token_field_tests|nativeui_widget_color_picker_tests|nativeui_widget_split_callback_faults_tests|nativeui_widget_breadcrumbs_tests|nativeui_widget_dynamic_extraction_tests|nativeui_widget_state_owned_transactions_tests|nativeui_widget_style_scope_binding_recovery_tests|nativeui_widget_collection_transactions_tests|nativeui_state_tests|nativeui_widget_retained_checkpoint_tests|nativeui_widget_state_snapshot_copy_retirement_tests|nativeui_widget_state_key_retirement_tests|nativeui_widget_layout_structural_publication_tests|nativeui_widget_virtual_list_isolation_tests|nativeui_widget_virtual_list_row_faults_tests|nativeui_lifecycle_tests|nativeui_t061_overlay_acceptance_tests|nativeui_widget_overlay_ancestor_commands_tests|nativeui_widget_calendar_tests|nativeui_widget_date_input_tests|nativeui_widget_sidebar_tests|nativeui_widget_sidebar_publication_tests|nativeui_widget_dialog_extensions_tests|nativeui_widget_collapsible_tests|nativeui_widget_disclosure_faults_tests)$' --output-on-failure
```

**Result at `cb601757`:** the serial Debug Core/27-target build passes with zero
warnings (140.16 seconds). **27/27 suites pass** (32.97 seconds), with no ASan/UBSan
diagnostic. This covers the original twenty fault/retained/lifecycle suites,
Calendar/DateInput/Sidebar publication and Dialog extension checks, plus the
Collapsible/disclosure fault suites affected by the aggregate defaults.

The original correction run exposed an incorrect `addr8()` use in the new RGBA
raster fixture. It was corrected to read bytes through `SkPixmap::addr()` while
preserving the complete pixel comparison. The final Release and sanitizer runs
above include the corrected fixture.

ASan and UBSan cover NativeUI-owned Core/tests. The existing configuration excludes
only the vptr check because pinned Skia binaries lack the required RTTI; prebuilt
third-party code is not instrumented. Leak detection is disabled on this macOS
executor, so no LSan qualification is claimed. No TSan claim is made: this batch
introduces no shared cross-thread state.

### GCC portability and initial complete Linux run

The manually dispatched [Linux Core run on `1da893dc`](https://github.com/hemduf/nativeui/actions/runs/37297986851)
failed on the virtual-list guard formatting before unit tests could start.
The [run on `a81c841f`](https://github.com/hemduf/nativeui/actions/runs/37299079804)
compiled those guards and Dialog successfully, then reported Calendar's
`-Werror=format-truncation`. The [run on `b1f6e127`](https://github.com/hemduf/nativeui/actions/runs/37299677064)
compiled Calendar successfully, then reported Sidebar style snapshot
`-Werror=maybe-uninitialized` diagnostics. These are confirmed Blocking build findings under
CODE_REVIEW.md §1, rather than warnings to suppress.

The first correction changes whitespace only in two files. Calendar's buffer then
increases from eleven to sixteen bytes; GCC's inferred year/month/day ranges need
at most fifteen bytes including the terminator. Its valid-domain guard and output
format remain unchanged. Independent read-only review checked the guards, bounds
and compatibility and scanned 267 other changed source/header fragments for the
same indentation issue. Sidebar snapshots now use fully initialized plain style values, guarded by the same
old/new optional row identities. Independent review found no allocation, lifetime,
callback-order or exception-contract changes. No NativeUI warning allowance was added.

The [Linux Core run on `b6fd7af7`](https://github.com/hemduf/nativeui/actions/runs/37300773132)
compiled and archived all production Core sources, then failed on partial
CollapsibleStyle initializers in the disclosure tests. Public empty defaults also
cover the analogous Accordion/Popover fields and 93 RichTextSpan/ToastSpec partial
initializers found across fixtures and examples. The baseline/corrected strict
C++20 probe confirms all five types remain aggregates with identical sizes,
alignments, standard-layout traits and noexcept default/move construction.
No fixture assertion or initializer coverage is removed.

The [run on `90ea56ba`](https://github.com/hemduf/nativeui/actions/runs/37305359051)
compiled past those aggregate fixtures, then rejected the converting string
reference in NumberInput's draft loop under `-Werror=range-loop-construct`.
NumberInput and the analogous Toolbar example now construct their const string
value explicitly in `5225410e`; all values, bodies and assertions are unchanged.
Independent read-only review scanned 285 changed test/example/support files and
found no additional high-confidence warning candidates. The English audit covers
212 changed test source/configuration files and preserves intentional Unicode
initials, composition, shaping, grapheme and typeahead data.

The [initial complete Linux Core run](https://github.com/hemduf/nativeui/actions/runs/37307538738)
completed at `cb601757`: the complete Release Core build passes with strict
default warnings, but only **202/203 unit suites pass**.
`nativeui_widget_rich_text_recovery_tests` segfaults. A serial Debug ASan/UBSan
build and direct run of that same suite reproduce heap-use-after-free in
`Tree::invalidate_node_paint`, reached through `activate_action` after a callback
copy removes its node. The original 27-suite sanitizer selection did not include
this recovery suite. The correction and its targeted GREEN evidence are recorded
above at `615a8b83`. That RED run superseded the
loop-only `5225410e` run because the ten English fixture corrections also change
executable source. The earlier broad Mac runs above qualify that same frozen source;
older `1da893dc` and `b6fd7af7` results are not substituted for these reruns.

Non-blocking coverage follow-up: Calendar/DateInput tests exercise navigation,
leap days, overlays, ranges and later successful operations, but do not assert
exact ISO semantic strings at years 1 and 9999, absence and adjacent out-of-domain
dates. The capacity correction does not claim new endpoint-format coverage. Sidebar tests
cover pointer contacts and publication faults, but lack an explicit hover
entry/row-transition/exit and equal-versus-changed appearance oracle. This is an
advisory coverage gap; the snapshot correction is statically equivalent.

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
