# Suivi de l’implémentation des composants

Le dépôt à utiliser est **`/Volumes/T7/Code/nativeui`**. Les déclarations publiques sont dans `include/nativeui/`, les implémentations dans `src/`, les exemples dans `examples/features/`. Le header collectif [widgets.hpp](../include/nativeui/widgets.hpp) expose les composants. Les dossiers de travail sous `tmp` contiennent les brouillons et les résultats intermédiaires de vérification ; ils ne sont pas nécessaires pour utiliser ces sources.

Ce suivi accompagne les 83 contrats de [widgets.md](widgets.md). Les références MyGo et les statuts documentaires des pages restent ceux du snapshot initial. Les couples de fichiers ci-dessous décrivent le travail C++ présent dans ce checkout.

Les extensions restent en cours : une extraction compatible ne signifie pas que tous les critères de la page sont réalisés. Les contrôleurs et les adaptateurs templates délèguent à des noyaux `.cpp` ; les types utilisateurs ne sont pas limités à une liste d’instanciations.

## Organisation et couverture

Les 83 composants du catalogue disposent de leur couple `.hpp` / `.cpp`, intégré à `NativeUI::Core`. Les extensions encore ouvertes sont indiquées ci-dessous : la présence du couple de fichiers ne signifie pas que tous les critères futurs de chaque spécification sont terminés.

| Composant | Couple public/implémentation | Intégration Core |
| --- | --- | --- |
| [Label](label.md) | [label.hpp](../include/nativeui/label.hpp) / [label.cpp](../src/label.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Header](header.md) | [header.hpp](../include/nativeui/header.hpp) / [header.cpp](../src/header.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [RichText](rich_text.md) | [rich_text.hpp](../include/nativeui/rich_text.hpp) / [rich_text.cpp](../src/rich_text.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Canvas](canvas.md) | [canvas.hpp](../include/nativeui/canvas.hpp) / [canvas.cpp](../src/canvas.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Knob](knob.md) | [knob.hpp](../include/nativeui/knob.hpp) / [knob.cpp](../src/knob.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Divider](divider.md) | [divider.hpp](../include/nativeui/divider.hpp) / [divider.cpp](../src/divider.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Button](button.md) | [button.hpp](../include/nativeui/button.hpp) / [button.cpp](../src/button.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Link](link.md) | [link.hpp](../include/nativeui/link.hpp) / [link.cpp](../src/link.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [PopupMenu](popup_menu.md) | [popup_menu.hpp](../include/nativeui/popup_menu.hpp) / [popup_menu.cpp](../src/popup_menu.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ContextMenu](context_menu.md) | [context_menu.hpp](../include/nativeui/context_menu.hpp) / [context_menu.cpp](../src/context_menu.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ToggleButton](toggle_button.md) | [toggle_button.hpp](../include/nativeui/toggle_button.hpp) / [toggle_button.cpp](../src/toggle_button.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ToggleGroup](toggle_group.md) | [toggle_group.hpp](../include/nativeui/toggle_group.hpp) / [toggle_group.cpp](../src/toggle_group.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [SegmentedControl<T>](segmented_control.md) | [segmented_control.hpp](../include/nativeui/segmented_control.hpp) / [segmented_control.cpp](../src/segmented_control.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Toolbar](toolbar.md) | [toolbar.hpp](../include/nativeui/toolbar.hpp) / [toolbar.cpp](../src/toolbar.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Checkbox](checkbox.md) | [checkbox.hpp](../include/nativeui/checkbox.hpp) / [checkbox.cpp](../src/checkbox.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [CheckboxGroup](checkbox_group.md) | [checkbox_group.hpp](../include/nativeui/checkbox_group.hpp) / [checkbox_group.cpp](../src/checkbox_group.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [RadioButton<T> et RadioGroup<T>](radio_button.md) | [radio_button.hpp](../include/nativeui/radio_button.hpp) / [radio_button.cpp](../src/radio_button.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Toggle](toggle.md) | [toggle.hpp](../include/nativeui/toggle.hpp) / [toggle.cpp](../src/toggle.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ComboBox<T>](combo_box.md) | [combo_box.hpp](../include/nativeui/combo_box.hpp) / [combo_box.cpp](../src/combo_box.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Slider](slider.md) | [slider.hpp](../include/nativeui/slider.hpp) / [slider.cpp](../src/slider.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [RangeSlider](range_slider.md) | [range_slider.hpp](../include/nativeui/range_slider.hpp) / [range_slider.cpp](../src/range_slider.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Stepper](stepper.md) | [stepper.hpp](../include/nativeui/stepper.hpp) / [stepper.cpp](../src/stepper.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Rating](rating.md) | [rating.hpp](../include/nativeui/rating.hpp) / [rating.cpp](../src/rating.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TextInput](text_input.md) | [text_input.hpp](../include/nativeui/text_input.hpp) / [text_input.cpp](../src/text_input.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TextArea](text_area.md) | [text_area.hpp](../include/nativeui/text_area.hpp) / [text_area.cpp](../src/text_area.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [NumberInput](number_input.md) | [number_input.hpp](../include/nativeui/number_input.hpp) / [number_input.cpp](../src/number_input.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [SearchField](search_field.md) | [search_field.hpp](../include/nativeui/search_field.hpp) / [search_field.cpp](../src/search_field.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [EditableComboBox](editable_combo_box.md) | [editable_combo_box.hpp](../include/nativeui/editable_combo_box.hpp) / [editable_combo_box.cpp](../src/editable_combo_box.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Autocomplete](autocomplete.md) | [autocomplete.hpp](../include/nativeui/autocomplete.hpp) / [autocomplete.cpp](../src/autocomplete.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TokenField](token_field.md) | [token_field.hpp](../include/nativeui/token_field.hpp) / [token_field.cpp](../src/token_field.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [EditableText](editable_text.md) | [editable_text.hpp](../include/nativeui/editable_text.hpp) / [editable_text.cpp](../src/editable_text.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [FindBar](find_bar.md) | [find_bar.hpp](../include/nativeui/find_bar.hpp) / [find_bar.cpp](../src/find_bar.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Calendar](calendar.md) | [calendar.hpp](../include/nativeui/calendar.hpp) / [calendar.cpp](../src/calendar.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [DateInput](date_input.md) | [date_input.hpp](../include/nativeui/date_input.hpp) / [date_input.cpp](../src/date_input.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TimeInput](time_input.md) | [time_input.hpp](../include/nativeui/time_input.hpp) / [time_input.cpp](../src/time_input.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ColorPicker](color_picker.md) | [color_picker.hpp](../include/nativeui/color_picker.hpp) / [color_picker.cpp](../src/color_picker.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ColorWell](color_well.md) | [color_well.hpp](../include/nativeui/color_well.hpp) / [color_well.cpp](../src/color_well.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ListView<Key>](list_view.md) | [list_view.hpp](../include/nativeui/list_view.hpp) / [list_view.cpp](../src/list_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TableView<Key>](table_view.md) | [table_view.hpp](../include/nativeui/table_view.hpp) / [table_view.cpp](../src/table_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [TreeView<Key>](tree_view.md) | [tree_view.hpp](../include/nativeui/tree_view.hpp) / [tree_view.cpp](../src/tree_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [OutlineView<Key>](outline_view.md) | [outline_view.hpp](../include/nativeui/outline_view.hpp) / [outline_view.cpp](../src/outline_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [OutlineTableView<Key>](outline_table_view.md) | [outline_table_view.hpp](../include/nativeui/outline_table_view.hpp) / [outline_table_view.cpp](../src/outline_table_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [GridView<Key>](grid_view.md) | [grid_view.hpp](../include/nativeui/grid_view.hpp) / [grid_view.cpp](../src/grid_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Tabs<T>](tabs.md) | [tabs.hpp](../include/nativeui/tabs.hpp) / [tabs.cpp](../src/tabs.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Sidebar<Key>](sidebar.md) | [sidebar.hpp](../include/nativeui/sidebar.hpp) / [sidebar.cpp](../src/sidebar.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Breadcrumbs](breadcrumbs.md) | [breadcrumbs.hpp](../include/nativeui/breadcrumbs.hpp) / [breadcrumbs.cpp](../src/breadcrumbs.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [HistoryButton](history_button.md) | [history_button.hpp](../include/nativeui/history_button.hpp) / [history_button.cpp](../src/history_button.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Row](row.md) | [row.hpp](../include/nativeui/row.hpp) / [row.cpp](../src/row.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Column](column.md) | [column.hpp](../include/nativeui/column.hpp) / [column.cpp](../src/column.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Grid](grid.md) | [grid.hpp](../include/nativeui/grid.hpp) / [grid.cpp](../src/grid.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Scroll](scroll.md) | [scroll.hpp](../include/nativeui/scroll.hpp) / [scroll.cpp](../src/scroll.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ScrollView](scroll_view.md) | [scroll_view.hpp](../include/nativeui/scroll_view.hpp) / [scroll_view.cpp](../src/scroll_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Clip](clip.md) | [clip.hpp](../include/nativeui/clip.hpp) / [clip.cpp](../src/clip.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Flex](flex.md) | [flex.hpp](../include/nativeui/flex.hpp) / [flex.cpp](../src/flex.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Spacer](spacer.md) | [spacer.hpp](../include/nativeui/spacer.hpp) / [spacer.cpp](../src/spacer.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Stack](stack.md) | [stack.hpp](../include/nativeui/stack.hpp) / [stack.cpp](../src/stack.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Padding](padding.md) | [padding.hpp](../include/nativeui/padding.hpp) / [padding.cpp](../src/padding.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [SplitView](split_view.md) | [split_view.hpp](../include/nativeui/split_view.hpp) / [split_view.cpp](../src/split_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Collapsible](collapsible.md) | [collapsible.hpp](../include/nativeui/collapsible.hpp) / [collapsible.cpp](../src/collapsible.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Accordion](accordion.md) | [accordion.hpp](../include/nativeui/accordion.hpp) / [accordion.cpp](../src/accordion.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Form](form.md) | [form.hpp](../include/nativeui/form.hpp) / [form.cpp](../src/form.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Field](field.md) | [field.hpp](../include/nativeui/field.hpp) / [field.cpp](../src/field.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Fieldset](fieldset.md) | [fieldset.hpp](../include/nativeui/fieldset.hpp) / [fieldset.cpp](../src/fieldset.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Dialog](dialog.md) | [dialog.hpp](../include/nativeui/dialog.hpp) / [dialog.cpp](../src/dialog.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Popover](popover.md) | [popover.hpp](../include/nativeui/popover.hpp) / [popover.cpp](../src/popover.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Tooltip](tooltip.md) | [tooltip.hpp](../include/nativeui/tooltip.hpp) / [tooltip.cpp](../src/tooltip.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Toast](toast.md) | [toast.hpp](../include/nativeui/toast.hpp) / [toast.cpp](../src/toast.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ProgressBar](progress_bar.md) | [progress_bar.hpp](../include/nativeui/progress_bar.hpp) / [progress_bar.cpp](../src/progress_bar.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Spinner](spinner.md) | [spinner.hpp](../include/nativeui/spinner.hpp) / [spinner.cpp](../src/spinner.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Meter](meter.md) | [meter.hpp](../include/nativeui/meter.hpp) / [meter.cpp](../src/meter.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Badge](badge.md) | [badge.hpp](../include/nativeui/badge.hpp) / [badge.cpp](../src/badge.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Visibility](visibility.md) | [visibility.hpp](../include/nativeui/visibility.hpp) / [visibility.cpp](../src/visibility.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Enabled](enabled.md) | [enabled.hpp](../include/nativeui/enabled.hpp) / [enabled.cpp](../src/enabled.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ReadOnly](read_only.md) | [read_only.hpp](../include/nativeui/read_only.hpp) / [read_only.cpp](../src/read_only.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [If](if.md) | [if.hpp](../include/nativeui/if.hpp) / [if.cpp](../src/if.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Switch<T>](switch.md) | [switch.hpp](../include/nativeui/switch.hpp) / [switch.cpp](../src/switch.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ForEach<T>](for_each.md) | [for_each.hpp](../include/nativeui/for_each.hpp) / [for_each.cpp](../src/for_each.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [FocusScope](focus_scope.md) | [focus_scope.hpp](../include/nativeui/focus_scope.hpp) / [focus_scope.cpp](../src/focus_scope.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [CommandScope](command_scope.md) | [command_scope.hpp](../include/nativeui/command_scope.hpp) / [command_scope.cpp](../src/command_scope.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [StyleScope](style_scope.md) | [style_scope.hpp](../include/nativeui/style_scope.hpp) / [style_scope.cpp](../src/style_scope.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [ImageView](image_view.md) | [image_view.hpp](../include/nativeui/image_view.hpp) / [image_view.cpp](../src/image_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [IconView](icon_view.md) | [icon_view.hpp](../include/nativeui/icon_view.hpp) / [icon_view.cpp](../src/icon_view.cpp) | Intégré à Core ; extensions suivies ci-dessous |
| [Avatar](avatar.md) | [avatar.hpp](../include/nativeui/avatar.hpp) / [avatar.cpp](../src/avatar.cpp) | Intégré à Core ; extensions suivies ci-dessous |

## Vérifications et limites actuelles

- Le palier de 83 composants compile en Release sans avertissement : PopupMenu, ContextMenu, RichText, les cinq nouvelles collections, Sidebar, Popover, Toast, ToggleGroup, SegmentedControl et Toolbar sont intégrés. Les 38 suites ciblées du dernier palier passent, ainsi que les 14 nouveaux exemples publics `--self-test` et les 17 probes de headers compilés séparément. Les régressions couvrent la sélection possédée, les changements réentrants, le focus après retrait, le partage d’espace Toolbar, les métriques sélectionnées, les fermetures d’overlay et la reprise des actions Toast après un refus de queue. Ces résultats ne remplacent pas une exécution complète de toutes les suites ni une qualification complète par sanitizers.

- Le baseline NativeUI a été compilé avant extraction. Sa suite complète donne 199 réussites sur 200 ; `nativeui_fractal_noise_gpu_reference_tests` échoue également en reprise isolée sur une divergence GPU/raster. Cet échec préexistant reste distinct des nouvelles validations.
- La compilation Release du lot de 55 composants, avec exemples publics, a réussi sans avertissement. Les 16 vérifications ciblées du dernier palier passent, dont routage, drop, layout, Fieldset et les exemples historiques T038/T174. Les nouveaux oracles vérifient aussi les gardes de mutation, les callbacks des formulaires, la récupération du thème et les révisions de State. La suite complète et les sanitizers restent à relancer après les prochaines corrections.
- Le palier de 58 composants compile avec les trois nouveaux exemples publics. Les 10 tests ciblés du dernier palier passent, ainsi que les suites supplémentaires de listes partagées, de lignes recyclées, de réactivation et les trois exemples `--self-test`. `State::snapshot()` offre une copie possédée avec protection des écritures réentrantes ; `set_if()` revalide la condition au commit final. Le test strict de copie longue sous manque de mémoire conserve l’échec de dépendance décrit ci-dessous.
- Le palier de 69 composants ajoute ComboBox, EditableComboBox, Autocomplete, TokenField, Calendar, DateInput, TimeInput, ColorPicker, ColorWell, Breadcrumbs et HistoryButton. Les 19 nouvelles suites comportementales et les 11 exemples publics `--self-test` passent. Les oracles de réentrance couvrent les publications après perte de contact/focus, la conservation des tokens refusés et la récupération des fournisseurs. La géométrie sémantique virtuelle accepte des rectangles variables et une sélection multiple possédée. Le routage des demandes contextuelles utilise la cible et la géométrie du même événement, ainsi que les touches Menu et Maj+F10. Les 12 scénarios contextuels passent après correction du drainage : une commande créée pendant l’événement est traitée ensuite, et une source enfant sans commande ne masque pas le menu parent.
- Les cinq tests natifs macOS ayant produit les rapports AppKit ont été relancés avec accès normal à la session graphique : cinq réussites. Les rapports venaient de `_RegisterApplication` pendant le lancement restreint, avant la création des widgets.
- Les tests d’animation utilisent un Dispatcher par propriétaire, une horloge manuelle et des fautes déterministes. Les échecs d’ordonnancement conservent un rendu statique ; peinture et mesure ne réarment pas un timer refusé.
- Les nouveaux contextes retenus résolvent une durée de vie faible, un NodeId stable et la génération du contact. Les demandes de focus sont différées au checkpoint du Tree. Les hooks de géométrie sont publiés après une transaction de layout réussie.
- Les ponts d’accessibilité T068 et la préédition IME complète restent des dépendances plateforme. Les rôles/actions neutres et les modèles d’édition headless ne constituent pas une validation VoiceOver, UIA ou AT-SPI.

## Validation de la PR du 5 octobre 2026

La branche `codex/nativeui-widgets-and-gallery`, au commit source `fd2013fd`, a été recompilée entièrement en Release avec `CMAKE_BUILD_PARALLEL_LEVEL=1` : Core, plateforme macOS, tous les exemples et les probes des 83 headers publics. La compilation réussit sans avertissement NativeUI. Cette configuration active plateforme, exemples et tests ; les contrats de package installable et l’inspecteur restent désactivés.

La commande `ctest --test-dir build-widgets --output-on-failure`, exécutée en série avec `TMPDIR`, `TMP` et `TEMP` dans `/Volumes/T7/tmp/`, donne **379 réussites sur 384 tests**, en 303 secondes. Les six validations de la galerie et des correctifs Calendar, DateInput, Sidebar, ColorPicker et ColorWell restent réussies.

| Test en échec | Diagnostic à traiter avant fusion |
| --- | --- |
| `nativeui_example_t035_combo_popup_self_test` | Un ancien opener ComboBox n’est pas supprimé après Tab. |
| `nativeui_example_t038_closure_invalidation_self_test` | Une apparence de survol ComboBox inchangée invalide l’arbre. |
| `nativeui_example_t038_menu_item_invalidation_self_test` | Une apparence de highlight MenuItem inchangée invalide l’arbre. |
| `nativeui_t035_combo_popup_tests` | Échap est consommé alors que le test attend `EventResult::Ignored` (`tests/t035_combo_popup_tests.cpp:256`). |
| `nativeui_widget_style_scope_binding_recovery_tests` | L’oracle d’injection de manque de mémoire termine le processus enfant ; une faute injectée a été atteinte. Cette limite de récupération reste ouverte. |

Ce résultat n’efface pas les résultats historiques ci-dessus et ne constitue pas une qualification des sanitizers ou des autres plateformes. La PR reste en brouillon : ces cinq échecs et les conflits avec les changements récents de `main` doivent être résolus avant fusion. Les 849 liens documentaires locaux vérifiés existent.

## Contrats encore à compléter dans les composants présents

- Header : projection sémantique distincte des textes du titre et sous-titre.
- RangeSlider : deux identités sémantiques retenues pour les deux poignées.
- Clip : conserver la mesure historique et arbitrer explicitement la cible de mesure contrainte avant modification.
- Scroll/ScrollView : qualifier l’invalidation et les grandes coordonnées lors des extensions de collections.
- StyleScope : la frontière `noexcept` conserve le dernier thème ; la révision du Binding permet désormais de récupérer au checkpoint un changement commis dont une notification antérieure a levé. La récupération d’un manque de mémoire lors de la copie longue de `Theme` reste non qualifiée : l’oracle strict termine dans un helper `std::string` coalescé depuis la Skia précompilée, sans métadonnées d’unwind. Le test est conservé et cet échec reste ouvert.
- Collections virtualisées : les fenêtres de matérialisation, contacts et métriques sont isolés par compilation. Le premier attachement publie les métriques historiques du contrôleur ; cette responsabilité est transférée au checkpoint après son retrait. Les trois suites dédiées passent. TableView, TreeView, OutlineView, OutlineTableView et GridView ajoutent des modèles possédés, sélections multiples, fenêtres virtualisées et géométrie sémantique variable. Les garanties qualifiées sont distinguées des extensions à venir dans leurs pages et les tests.
- Les oracles de désactivation/réactivation passent : un ancien événement ne peut plus écrire après une nouvelle activation. Les vérifications de performance du routage clavier restent à terminer.
- Tooltip : les styles et l’association de description sont disponibles ; le découpage complet des graphèmes reste à qualifier. Dialog : les alertes et actions sont disponibles ; le corps multiligne des alertes reste à compléter. CheckboxGroup publie les changements ordonnés sans rollback ni rejeu des observateurs.
- Chaque ligne du catalogue possède maintenant une implémentation réelle. Les contrats encore ouverts ci-dessus, les ponts plateforme et la qualification complète restent suivis séparément.
