# Component implementation tracking

The repository to use is **`/Volumes/T7/Code/nativeui`**. Public declarations are in `include/nativeui/`, implementations in `src/`, and examples in `examples/features/`. The collective [widgets.hpp](../include/nativeui/widgets.hpp) header exposes the components. Working directories under `tmp` contain drafts and intermediate verification results; they are not required to use these sources.

This tracking document accompanies the 83 contracts in [widgets.md](widgets.md). MyGo references and page status labels retain the initial snapshot's context. The file pairs below describe the C++ work present in this checkout.

Extensions remain in progress: compatible extraction does not mean that every acceptance criterion on a page is complete. Controllers and template adapters delegate to `.cpp` kernels; user types are not restricted to a predefined list of instantiations.

## Organization and coverage

All 83 catalog components have a `.hpp` / `.cpp` pair integrated into `NativeUI::Core`. Open extensions are listed below: the presence of a file pair does not mean that every specification's future acceptance criteria are complete.

| Component | Public/implementation pair | Core integration |
| --- | --- | --- |
| [Label](label.md) | [label.hpp](../include/nativeui/label.hpp) / [label.cpp](../src/label.cpp) | Integrated into Core; extensions tracked below |
| [Header](header.md) | [header.hpp](../include/nativeui/header.hpp) / [header.cpp](../src/header.cpp) | Integrated into Core; extensions tracked below |
| [RichText](rich_text.md) | [rich_text.hpp](../include/nativeui/rich_text.hpp) / [rich_text.cpp](../src/rich_text.cpp) | Integrated into Core; extensions tracked below |
| [Canvas](canvas.md) | [canvas.hpp](../include/nativeui/canvas.hpp) / [canvas.cpp](../src/canvas.cpp) | Integrated into Core; extensions tracked below |
| [Knob](knob.md) | [knob.hpp](../include/nativeui/knob.hpp) / [knob.cpp](../src/knob.cpp) | Integrated into Core; extensions tracked below |
| [Divider](divider.md) | [divider.hpp](../include/nativeui/divider.hpp) / [divider.cpp](../src/divider.cpp) | Integrated into Core; extensions tracked below |
| [Button](button.md) | [button.hpp](../include/nativeui/button.hpp) / [button.cpp](../src/button.cpp) | Integrated into Core; extensions tracked below |
| [Link](link.md) | [link.hpp](../include/nativeui/link.hpp) / [link.cpp](../src/link.cpp) | Integrated into Core; extensions tracked below |
| [PopupMenu](popup_menu.md) | [popup_menu.hpp](../include/nativeui/popup_menu.hpp) / [popup_menu.cpp](../src/popup_menu.cpp) | Integrated into Core; extensions tracked below |
| [ContextMenu](context_menu.md) | [context_menu.hpp](../include/nativeui/context_menu.hpp) / [context_menu.cpp](../src/context_menu.cpp) | Integrated into Core; extensions tracked below |
| [ToggleButton](toggle_button.md) | [toggle_button.hpp](../include/nativeui/toggle_button.hpp) / [toggle_button.cpp](../src/toggle_button.cpp) | Integrated into Core; extensions tracked below |
| [ToggleGroup](toggle_group.md) | [toggle_group.hpp](../include/nativeui/toggle_group.hpp) / [toggle_group.cpp](../src/toggle_group.cpp) | Integrated into Core; extensions tracked below |
| [SegmentedControl<T>](segmented_control.md) | [segmented_control.hpp](../include/nativeui/segmented_control.hpp) / [segmented_control.cpp](../src/segmented_control.cpp) | Integrated into Core; extensions tracked below |
| [Toolbar](toolbar.md) | [toolbar.hpp](../include/nativeui/toolbar.hpp) / [toolbar.cpp](../src/toolbar.cpp) | Integrated into Core; extensions tracked below |
| [Checkbox](checkbox.md) | [checkbox.hpp](../include/nativeui/checkbox.hpp) / [checkbox.cpp](../src/checkbox.cpp) | Integrated into Core; extensions tracked below |
| [CheckboxGroup](checkbox_group.md) | [checkbox_group.hpp](../include/nativeui/checkbox_group.hpp) / [checkbox_group.cpp](../src/checkbox_group.cpp) | Integrated into Core; extensions tracked below |
| [RadioButton<T> and RadioGroup<T>](radio_button.md) | [radio_button.hpp](../include/nativeui/radio_button.hpp) / [radio_button.cpp](../src/radio_button.cpp) | Integrated into Core; extensions tracked below |
| [Toggle](toggle.md) | [toggle.hpp](../include/nativeui/toggle.hpp) / [toggle.cpp](../src/toggle.cpp) | Integrated into Core; extensions tracked below |
| [ComboBox<T>](combo_box.md) | [combo_box.hpp](../include/nativeui/combo_box.hpp) / [combo_box.cpp](../src/combo_box.cpp) | Integrated into Core; extensions tracked below |
| [Slider](slider.md) | [slider.hpp](../include/nativeui/slider.hpp) / [slider.cpp](../src/slider.cpp) | Integrated into Core; extensions tracked below |
| [RangeSlider](range_slider.md) | [range_slider.hpp](../include/nativeui/range_slider.hpp) / [range_slider.cpp](../src/range_slider.cpp) | Integrated into Core; extensions tracked below |
| [Stepper](stepper.md) | [stepper.hpp](../include/nativeui/stepper.hpp) / [stepper.cpp](../src/stepper.cpp) | Integrated into Core; extensions tracked below |
| [Rating](rating.md) | [rating.hpp](../include/nativeui/rating.hpp) / [rating.cpp](../src/rating.cpp) | Integrated into Core; extensions tracked below |
| [TextInput](text_input.md) | [text_input.hpp](../include/nativeui/text_input.hpp) / [text_input.cpp](../src/text_input.cpp) | Integrated into Core; extensions tracked below |
| [TextArea](text_area.md) | [text_area.hpp](../include/nativeui/text_area.hpp) / [text_area.cpp](../src/text_area.cpp) | Integrated into Core; extensions tracked below |
| [NumberInput](number_input.md) | [number_input.hpp](../include/nativeui/number_input.hpp) / [number_input.cpp](../src/number_input.cpp) | Integrated into Core; extensions tracked below |
| [SearchField](search_field.md) | [search_field.hpp](../include/nativeui/search_field.hpp) / [search_field.cpp](../src/search_field.cpp) | Integrated into Core; extensions tracked below |
| [EditableComboBox](editable_combo_box.md) | [editable_combo_box.hpp](../include/nativeui/editable_combo_box.hpp) / [editable_combo_box.cpp](../src/editable_combo_box.cpp) | Integrated into Core; extensions tracked below |
| [Autocomplete](autocomplete.md) | [autocomplete.hpp](../include/nativeui/autocomplete.hpp) / [autocomplete.cpp](../src/autocomplete.cpp) | Integrated into Core; extensions tracked below |
| [TokenField](token_field.md) | [token_field.hpp](../include/nativeui/token_field.hpp) / [token_field.cpp](../src/token_field.cpp) | Integrated into Core; extensions tracked below |
| [EditableText](editable_text.md) | [editable_text.hpp](../include/nativeui/editable_text.hpp) / [editable_text.cpp](../src/editable_text.cpp) | Integrated into Core; extensions tracked below |
| [FindBar](find_bar.md) | [find_bar.hpp](../include/nativeui/find_bar.hpp) / [find_bar.cpp](../src/find_bar.cpp) | Integrated into Core; extensions tracked below |
| [Calendar](calendar.md) | [calendar.hpp](../include/nativeui/calendar.hpp) / [calendar.cpp](../src/calendar.cpp) | Integrated into Core; extensions tracked below |
| [DateInput](date_input.md) | [date_input.hpp](../include/nativeui/date_input.hpp) / [date_input.cpp](../src/date_input.cpp) | Integrated into Core; extensions tracked below |
| [TimeInput](time_input.md) | [time_input.hpp](../include/nativeui/time_input.hpp) / [time_input.cpp](../src/time_input.cpp) | Integrated into Core; extensions tracked below |
| [ColorPicker](color_picker.md) | [color_picker.hpp](../include/nativeui/color_picker.hpp) / [color_picker.cpp](../src/color_picker.cpp) | Integrated into Core; extensions tracked below |
| [ColorWell](color_well.md) | [color_well.hpp](../include/nativeui/color_well.hpp) / [color_well.cpp](../src/color_well.cpp) | Integrated into Core; extensions tracked below |
| [ListView<Key>](list_view.md) | [list_view.hpp](../include/nativeui/list_view.hpp) / [list_view.cpp](../src/list_view.cpp) | Integrated into Core; extensions tracked below |
| [TableView<Key>](table_view.md) | [table_view.hpp](../include/nativeui/table_view.hpp) / [table_view.cpp](../src/table_view.cpp) | Integrated into Core; extensions tracked below |
| [TreeView<Key>](tree_view.md) | [tree_view.hpp](../include/nativeui/tree_view.hpp) / [tree_view.cpp](../src/tree_view.cpp) | Integrated into Core; extensions tracked below |
| [OutlineView<Key>](outline_view.md) | [outline_view.hpp](../include/nativeui/outline_view.hpp) / [outline_view.cpp](../src/outline_view.cpp) | Integrated into Core; extensions tracked below |
| [OutlineTableView<Key>](outline_table_view.md) | [outline_table_view.hpp](../include/nativeui/outline_table_view.hpp) / [outline_table_view.cpp](../src/outline_table_view.cpp) | Integrated into Core; extensions tracked below |
| [GridView<Key>](grid_view.md) | [grid_view.hpp](../include/nativeui/grid_view.hpp) / [grid_view.cpp](../src/grid_view.cpp) | Integrated into Core; extensions tracked below |
| [Tabs<T>](tabs.md) | [tabs.hpp](../include/nativeui/tabs.hpp) / [tabs.cpp](../src/tabs.cpp) | Integrated into Core; extensions tracked below |
| [Sidebar<Key>](sidebar.md) | [sidebar.hpp](../include/nativeui/sidebar.hpp) / [sidebar.cpp](../src/sidebar.cpp) | Integrated into Core; extensions tracked below |
| [Breadcrumbs](breadcrumbs.md) | [breadcrumbs.hpp](../include/nativeui/breadcrumbs.hpp) / [breadcrumbs.cpp](../src/breadcrumbs.cpp) | Integrated into Core; extensions tracked below |
| [HistoryButton](history_button.md) | [history_button.hpp](../include/nativeui/history_button.hpp) / [history_button.cpp](../src/history_button.cpp) | Integrated into Core; extensions tracked below |
| [Row](row.md) | [row.hpp](../include/nativeui/row.hpp) / [row.cpp](../src/row.cpp) | Integrated into Core; extensions tracked below |
| [Column](column.md) | [column.hpp](../include/nativeui/column.hpp) / [column.cpp](../src/column.cpp) | Integrated into Core; extensions tracked below |
| [Grid](grid.md) | [grid.hpp](../include/nativeui/grid.hpp) / [grid.cpp](../src/grid.cpp) | Integrated into Core; extensions tracked below |
| [Scroll](scroll.md) | [scroll.hpp](../include/nativeui/scroll.hpp) / [scroll.cpp](../src/scroll.cpp) | Integrated into Core; extensions tracked below |
| [ScrollView](scroll_view.md) | [scroll_view.hpp](../include/nativeui/scroll_view.hpp) / [scroll_view.cpp](../src/scroll_view.cpp) | Integrated into Core; extensions tracked below |
| [Clip](clip.md) | [clip.hpp](../include/nativeui/clip.hpp) / [clip.cpp](../src/clip.cpp) | Integrated into Core; extensions tracked below |
| [Flex](flex.md) | [flex.hpp](../include/nativeui/flex.hpp) / [flex.cpp](../src/flex.cpp) | Integrated into Core; extensions tracked below |
| [Spacer](spacer.md) | [spacer.hpp](../include/nativeui/spacer.hpp) / [spacer.cpp](../src/spacer.cpp) | Integrated into Core; extensions tracked below |
| [Stack](stack.md) | [stack.hpp](../include/nativeui/stack.hpp) / [stack.cpp](../src/stack.cpp) | Integrated into Core; extensions tracked below |
| [Padding](padding.md) | [padding.hpp](../include/nativeui/padding.hpp) / [padding.cpp](../src/padding.cpp) | Integrated into Core; extensions tracked below |
| [SplitView](split_view.md) | [split_view.hpp](../include/nativeui/split_view.hpp) / [split_view.cpp](../src/split_view.cpp) | Integrated into Core; extensions tracked below |
| [Collapsible](collapsible.md) | [collapsible.hpp](../include/nativeui/collapsible.hpp) / [collapsible.cpp](../src/collapsible.cpp) | Integrated into Core; extensions tracked below |
| [Accordion](accordion.md) | [accordion.hpp](../include/nativeui/accordion.hpp) / [accordion.cpp](../src/accordion.cpp) | Integrated into Core; extensions tracked below |
| [Form](form.md) | [form.hpp](../include/nativeui/form.hpp) / [form.cpp](../src/form.cpp) | Integrated into Core; extensions tracked below |
| [Field](field.md) | [field.hpp](../include/nativeui/field.hpp) / [field.cpp](../src/field.cpp) | Integrated into Core; extensions tracked below |
| [Fieldset](fieldset.md) | [fieldset.hpp](../include/nativeui/fieldset.hpp) / [fieldset.cpp](../src/fieldset.cpp) | Integrated into Core; extensions tracked below |
| [Dialog](dialog.md) | [dialog.hpp](../include/nativeui/dialog.hpp) / [dialog.cpp](../src/dialog.cpp) | Integrated into Core; extensions tracked below |
| [Popover](popover.md) | [popover.hpp](../include/nativeui/popover.hpp) / [popover.cpp](../src/popover.cpp) | Integrated into Core; extensions tracked below |
| [Tooltip](tooltip.md) | [tooltip.hpp](../include/nativeui/tooltip.hpp) / [tooltip.cpp](../src/tooltip.cpp) | Integrated into Core; extensions tracked below |
| [Toast](toast.md) | [toast.hpp](../include/nativeui/toast.hpp) / [toast.cpp](../src/toast.cpp) | Integrated into Core; extensions tracked below |
| [ProgressBar](progress_bar.md) | [progress_bar.hpp](../include/nativeui/progress_bar.hpp) / [progress_bar.cpp](../src/progress_bar.cpp) | Integrated into Core; extensions tracked below |
| [Spinner](spinner.md) | [spinner.hpp](../include/nativeui/spinner.hpp) / [spinner.cpp](../src/spinner.cpp) | Integrated into Core; extensions tracked below |
| [Meter](meter.md) | [meter.hpp](../include/nativeui/meter.hpp) / [meter.cpp](../src/meter.cpp) | Integrated into Core; extensions tracked below |
| [Badge](badge.md) | [badge.hpp](../include/nativeui/badge.hpp) / [badge.cpp](../src/badge.cpp) | Integrated into Core; extensions tracked below |
| [Visibility](visibility.md) | [visibility.hpp](../include/nativeui/visibility.hpp) / [visibility.cpp](../src/visibility.cpp) | Integrated into Core; extensions tracked below |
| [Enabled](enabled.md) | [enabled.hpp](../include/nativeui/enabled.hpp) / [enabled.cpp](../src/enabled.cpp) | Integrated into Core; extensions tracked below |
| [ReadOnly](read_only.md) | [read_only.hpp](../include/nativeui/read_only.hpp) / [read_only.cpp](../src/read_only.cpp) | Integrated into Core; extensions tracked below |
| [If](if.md) | [if.hpp](../include/nativeui/if.hpp) / [if.cpp](../src/if.cpp) | Integrated into Core; extensions tracked below |
| [Switch<T>](switch.md) | [switch.hpp](../include/nativeui/switch.hpp) / [switch.cpp](../src/switch.cpp) | Integrated into Core; extensions tracked below |
| [ForEach<T>](for_each.md) | [for_each.hpp](../include/nativeui/for_each.hpp) / [for_each.cpp](../src/for_each.cpp) | Integrated into Core; extensions tracked below |
| [FocusScope](focus_scope.md) | [focus_scope.hpp](../include/nativeui/focus_scope.hpp) / [focus_scope.cpp](../src/focus_scope.cpp) | Integrated into Core; extensions tracked below |
| [CommandScope](command_scope.md) | [command_scope.hpp](../include/nativeui/command_scope.hpp) / [command_scope.cpp](../src/command_scope.cpp) | Integrated into Core; extensions tracked below |
| [StyleScope](style_scope.md) | [style_scope.hpp](../include/nativeui/style_scope.hpp) / [style_scope.cpp](../src/style_scope.cpp) | Integrated into Core; extensions tracked below |
| [ImageView](image_view.md) | [image_view.hpp](../include/nativeui/image_view.hpp) / [image_view.cpp](../src/image_view.cpp) | Integrated into Core; extensions tracked below |
| [IconView](icon_view.md) | [icon_view.hpp](../include/nativeui/icon_view.hpp) / [icon_view.cpp](../src/icon_view.cpp) | Integrated into Core; extensions tracked below |
| [Avatar](avatar.md) | [avatar.hpp](../include/nativeui/avatar.hpp) / [avatar.cpp](../src/avatar.cpp) | Integrated into Core; extensions tracked below |

## Historical milestone validation and current limitations

- The 83-component milestone compiles in Release without warnings: PopupMenu, ContextMenu, RichText, the five new collections, Sidebar, Popover, Toast, ToggleGroup, SegmentedControl and Toolbar are integrated. The latest milestone's 38 targeted suites pass, as do the 14 new public `--self-test` examples and 17 separately compiled header probes. Regressions cover owned selection, reentrant changes, focus after removal, Toolbar space sharing, selected-state metrics, overlay closure and Toast action recovery after queue rejection. These results do not replace running all suites or complete sanitizer qualification.
- The NativeUI baseline was compiled before extraction. Its full suite reports 199 passes out of 200; `nativeui_fractal_noise_gpu_reference_tests` also fails in an isolated rerun because of a GPU/raster divergence. This pre-existing failure remains separate from the new validation.
- The 55-component batch and public examples compiled in Release without warnings. The latest milestone's 16 targeted checks pass, including routing, drop, layout, Fieldset and historical T038/T174 examples. New oracles also check mutation guards, form callbacks, theme recovery and State revisions. Those milestone checks preceded the complete integrated qualification recorded below.
- The 58-component milestone compiles with three new public examples. The latest milestone's 10 targeted tests pass, along with additional shared-list, recycled-row and reactivation suites and the three `--self-test` examples. `State::snapshot()` provides an owned copy with protection against reentrant writes; `set_if()` revalidates the condition at final commit. The strict long-copy test originally exposed the dependency boundary documented in the historical baseline below; the subsequent review correction qualifies internal StyleScope recovery.
- The 69-component milestone adds ComboBox, EditableComboBox, Autocomplete, TokenField, Calendar, DateInput, TimeInput, ColorPicker, ColorWell, Breadcrumbs and HistoryButton. The 19 new behavioral suites and 11 public `--self-test` examples pass. Reentrancy oracles cover publication after contact/focus loss, retention of rejected tokens and provider recovery. Virtual semantic geometry supports variable rectangles and owned multiple selection. Context-request routing uses the target and geometry of the same event, plus Menu and Shift+F10 keys. The 12 context scenarios pass after the drain fix: a command created during an event is processed afterward, and a child source without a command does not mask the parent menu.
- The five native macOS tests that produced AppKit reports were rerun with normal access to the graphical session: all five passed. The reports originated in `_RegisterApplication` during restricted launch, before widget creation.
- Animation tests use a Dispatcher per owner, a manual clock and deterministic faults. Scheduling failures preserve static rendering; painting and measurement do not rearm a rejected timer.
- New retained contexts resolve a weak lifetime, stable NodeId and contact generation. Focus requests are deferred to the Tree checkpoint. Geometry hooks are published after a successful layout transaction.
- T068 accessibility bridges and complete IME preedit remain platform dependencies. Neutral roles/actions and headless editing models do not establish VoiceOver, UIA or AT-SPI validation.

## Historical PR baseline on October 5, 2026

Branch `codex/nativeui-widgets-and-gallery`, at source commit `fd2013fd`, was fully rebuilt in Release with `CMAKE_BUILD_PARALLEL_LEVEL=1`: Core, the macOS platform, all examples and probes for the 83 public headers. The build succeeds without NativeUI warnings. This configuration enables the platform, examples and tests; installed-package contracts and the inspector remain disabled.

The command `ctest --test-dir build-widgets --output-on-failure`, executed serially with `TMPDIR`, `TMP` and `TEMP` under `/Volumes/T7/tmp/`, reports **379 passes out of 384 tests**, in 303 seconds. The six gallery and Calendar, DateInput, Sidebar, ColorPicker and ColorWell checks continue to pass.

| Failure at the historical baseline | Pre-correction diagnostic |
| --- | --- |
| `nativeui_example_t035_combo_popup_self_test` | A stale ComboBox opener is not suppressed after Tab. |
| `nativeui_example_t038_closure_invalidation_self_test` | An unchanged resolved ComboBox hover appearance invalidates the tree. |
| `nativeui_example_t038_menu_item_invalidation_self_test` | An unchanged resolved MenuItem highlight appearance invalidates the tree. |
| `nativeui_t035_combo_popup_tests` | Escape is consumed although the test expects `EventResult::Ignored` (`tests/t035_combo_popup_tests.cpp:256`). |
| `nativeui_widget_style_scope_binding_recovery_tests` | The memory-exhaustion injection oracle terminates the child process; one injected fault was reached. This was an open recovery limitation at that baseline; it is corrected below. |

This is the pre-correction baseline. All five failures were subsequently reproduced and corrected under [the mandatory code review](widgets_review.md). Its earlier serial Release build, 384/384 CTest checks, 66/66 focused checks, 27/27 ASan/UBSan checks at executable source `cb601757`, public-header probes, headless gallery and native macOS window checks pass. Linux subsequently exposed RichText recovery use-after-free. The bounded correction at `615a8b83` passes five RichText sanitizer suites, six Release checks and the relinked headless/native gallery; complete Linux qualification is separate. The October 5 main integration resolves all thirteen conflicts and preserves the extracted APIs alongside upstream edit sessions, rendering caches and focus fixes. Final executable source `2c36d801` passes its complete zero-warning Release rebuild, 408/408 CTest checks, 24/24 ASan/UBSan suites and seven additional native checks. The final Knob interaction and overlay fault-location corrections receive independent review with 0 Blocking / 0 Important. Linux Core CI is **still running** on this same source revision and remains a merge gate. The PR stays Draft while that qualification is active. Exact commands, source identity and platform limits are in the review record. The earlier milestone checked 849 local links.

## Historical English documentation refresh

The earlier English refresh preserved the C++ signature blocks, source references and named acceptance tests. Documentation checks cover all 83 specification structures, all 54 MyGo mappings, matching component file pairs and 849 valid local links. The complete demo, including built-in widget captions, placeholders, validation messages and backend-neutral accessibility text, now uses English. Calendar uses fixed English month/weekday labels without a global locale service. The Release rebuild has no warnings; the eight-page headless self-test, 22 affected CTest tests and native macOS first-render/deferred-close test pass. C++ changes preserve API and behavior tokens and are confined to translated literals and their existing assertions. That initial English refresh did not rerun the complete suite. The subsequent mandatory review separately reran the complete 384-test suite, 66 focused checks and 27 sanitizer suites at `cb601757`, as recorded above and in the review report.

## Contracts still to complete in existing components

- Header: separate semantic projections for title and subtitle text.
- RangeSlider: two retained semantic identities for its two thumbs.
- Clip: preserve historical measurement and explicitly decide on the constrained-measurement target before changing it.
- Scroll/ScrollView: qualify invalidation and large coordinates when extending collections.
- StyleScope: the `noexcept` boundary preserves the last theme; the Binding revision now allows checkpoint recovery of a committed change whose earlier notification threw. Internal Theme/override text copies now reconstruct owned strings from bytes, avoiding the non-unwinding helper coalesced from prebuilt Skia. Four real allocation stages, checkpoint recovery and a subsequent update pass in Release and ASan/UBSan. Arbitrary allocation failure inside prebuilt third-party code remains outside this qualification; see [the review record](widgets_review.md).
- Virtualized collections: materialization windows, contacts and metrics are isolated per compilation. The first attachment publishes the controller's historical metrics; this responsibility transfers at the checkpoint after its removal. The three dedicated suites pass. TableView, TreeView, OutlineView, OutlineTableView and GridView add owned models, multiple selection, virtualized windows and variable semantic geometry. Their pages and tests distinguish qualified guarantees from future extensions.
- Deactivation/reactivation oracles pass: an old event can no longer write after a new activation. Keyboard-routing performance checks remain to be completed.
- Tooltip: styles and description association are available; complete grapheme segmentation remains to be qualified. Dialog: alerts and actions are available; multiline alert bodies remain to be completed. CheckboxGroup publishes ordered changes without rollback or observer replay.
- Every catalog row now has a real implementation. The open contracts above, platform bridges and complete qualification remain tracked separately.
