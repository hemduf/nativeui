# Spécifications des composants NativeUI

Ce catalogue est le point d’entrée de **83 spécifications de composants publics**.
Il couvre les **54 familles documentaires de MyGo**, leurs variantes autonomes,
les widgets supplémentaires de NativeUI et ses conteneurs de composition.
Une famille MyGo n’équivaut pas nécessairement à un composant : les alertes sont
une variante de Dialog, tandis que Form, Field et Fieldset ont des responsabilités
et des couples de fichiers distincts.

[Suivi de l’implémentation](widgets_implementation.md) : fichiers intégrés, validations et contrats encore à réaliser.

[Galerie de démonstration](widgets_demo.md) : application interactive utilisant les 83 composants, lancement et self-test.

## 1. Périmètre et lecture

Ces documents conservent le snapshot documentaire initial et spécifient la cible de développement. L’implémentation demandée ensuite est suivie séparément dans [widgets_implementation.md](widgets_implementation.md). Chaque page sépare API existante vérifiée, API
cible proposée et critères de livraison. Les exemples cibles ne sont pas
présentés comme compilés contre la version actuelle.

Références figées pour cet état des lieux, le 4 octobre 2026 :

- NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c`, checkout étudié
  `/Volumes/T7/Code/nativeui` ; [architecture](../DESIGN.md),
  [contrat de review](../CODE_REVIEW.md), [API widgets](../include/nativeui/widgets.hpp).
- MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`, checkout étudié
  `/Volumes/T7/Code/mygo` ; `docs/ui/README.md`, sources `ui/` et exemples
  `examples/gallery/`. Les références de chaque page donnent les fichiers et
  fonctions consultés à cette version.

Les mentions de statut décrivent le travail restant à ce snapshot ; elles ne
remplacent pas les statuts d’issues GitHub :

| Statut documentaire | Signification |
| --- | --- |
| **existant à extraire** | Capacité présente ; préserver comportement/API en séparant header et cpp. |
| **existant à enrichir** | Capacité présente ; extraction et extensions explicitement séparées dans la page. |
| **nouveau à implémenter** | Widget public absent ; API et comportements décrits comme cibles. |

Chaque page suit onze rubriques : objectif/état actuel, API, état/propriété,
interactions, layout, présentation, accessibilité, cycle de vie/récupération,
dépendances/cas limites, fichiers/compatibilité, tests/acceptation.

## 2. Catalogue et fichiers cibles

Le nom de page, le header et le cpp utilisent le même `snake_case`. Les chemins
C++ du tableau sont **des cibles**, même si un header de même nom existe déjà.
Les variantes, modèles d’items et classes d’implémentation privées restent dans
le couple de leur composant. Les types de données et contrôleurs ne reçoivent
pas un cpp vide pour satisfaire artificiellement cette règle.

### Texte et dessin

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Label](label.md) | existant à extraire | `include/nativeui/label.hpp` | `src/label.cpp` |
| [Header](header.md) | existant à enrichir | `include/nativeui/header.hpp` | `src/header.cpp` |
| [RichText](rich_text.md) | nouveau à implémenter | `include/nativeui/rich_text.hpp` | `src/rich_text.cpp` |
| [Canvas](canvas.md) | existant à enrichir | `include/nativeui/canvas.hpp` | `src/canvas.cpp` |
| [Knob](knob.md) | existant à enrichir | `include/nativeui/knob.hpp` | `src/knob.cpp` |
| [Divider](divider.md) | nouveau à implémenter | `include/nativeui/divider.hpp` | `src/divider.cpp` |

### Actions

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Button](button.md) | existant à enrichir | `include/nativeui/button.hpp` | `src/button.cpp` |
| [Link](link.md) | nouveau à implémenter | `include/nativeui/link.hpp` | `src/link.cpp` |
| [PopupMenu](popup_menu.md) | existant à enrichir | `include/nativeui/popup_menu.hpp` | `src/popup_menu.cpp` |
| [ContextMenu](context_menu.md) | nouveau à implémenter | `include/nativeui/context_menu.hpp` | `src/context_menu.cpp` |
| [ToggleButton](toggle_button.md) | nouveau à implémenter | `include/nativeui/toggle_button.hpp` | `src/toggle_button.cpp` |
| [ToggleGroup](toggle_group.md) | nouveau à implémenter | `include/nativeui/toggle_group.hpp` | `src/toggle_group.cpp` |
| [SegmentedControl<T>](segmented_control.md) | nouveau à implémenter | `include/nativeui/segmented_control.hpp` | `src/segmented_control.cpp` |
| [Toolbar](toolbar.md) | nouveau à implémenter | `include/nativeui/toolbar.hpp` | `src/toolbar.cpp` |

### Choix

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Checkbox](checkbox.md) | existant à extraire | `include/nativeui/checkbox.hpp` | `src/checkbox.cpp` |
| [CheckboxGroup](checkbox_group.md) | nouveau à implémenter | `include/nativeui/checkbox_group.hpp` | `src/checkbox_group.cpp` |
| [RadioButton<T> et RadioGroup<T>](radio_button.md) | existant à extraire | `include/nativeui/radio_button.hpp` | `src/radio_button.cpp` |
| [Toggle](toggle.md) | existant à extraire | `include/nativeui/toggle.hpp` | `src/toggle.cpp` |
| [ComboBox<T>](combo_box.md) | existant à extraire | `include/nativeui/combo_box.hpp` | `src/combo_box.cpp` |
| [Slider](slider.md) | existant à extraire | `include/nativeui/slider.hpp` | `src/slider.cpp` |
| [RangeSlider](range_slider.md) | existant à enrichir | `include/nativeui/range_slider.hpp` | `src/range_slider.cpp` |
| [Stepper](stepper.md) | nouveau à implémenter | `include/nativeui/stepper.hpp` | `src/stepper.cpp` |
| [Rating](rating.md) | nouveau à implémenter | `include/nativeui/rating.hpp` | `src/rating.cpp` |

### Saisie

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [TextInput](text_input.md) | existant à enrichir | `include/nativeui/text_input.hpp` | `src/text_input.cpp` |
| [TextArea](text_area.md) | existant à enrichir | `include/nativeui/text_area.hpp` | `src/text_area.cpp` |
| [NumberInput](number_input.md) | nouveau à implémenter | `include/nativeui/number_input.hpp` | `src/number_input.cpp` |
| [SearchField](search_field.md) | nouveau à implémenter | `include/nativeui/search_field.hpp` | `src/search_field.cpp` |
| [EditableComboBox](editable_combo_box.md) | nouveau à implémenter | `include/nativeui/editable_combo_box.hpp` | `src/editable_combo_box.cpp` |
| [Autocomplete](autocomplete.md) | nouveau à implémenter | `include/nativeui/autocomplete.hpp` | `src/autocomplete.cpp` |
| [TokenField](token_field.md) | nouveau à implémenter | `include/nativeui/token_field.hpp` | `src/token_field.cpp` |
| [EditableText](editable_text.md) | nouveau à implémenter | `include/nativeui/editable_text.hpp` | `src/editable_text.cpp` |
| [FindBar](find_bar.md) | nouveau à implémenter | `include/nativeui/find_bar.hpp` | `src/find_bar.cpp` |

### Dates et couleurs

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Calendar](calendar.md) | nouveau à implémenter | `include/nativeui/calendar.hpp` | `src/calendar.cpp` |
| [DateInput](date_input.md) | nouveau à implémenter | `include/nativeui/date_input.hpp` | `src/date_input.cpp` |
| [TimeInput](time_input.md) | nouveau à implémenter | `include/nativeui/time_input.hpp` | `src/time_input.cpp` |
| [ColorPicker](color_picker.md) | nouveau à implémenter | `include/nativeui/color_picker.hpp` | `src/color_picker.cpp` |
| [ColorWell](color_well.md) | nouveau à implémenter | `include/nativeui/color_well.hpp` | `src/color_well.cpp` |

### Collections et navigation

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [ListView<Key>](list_view.md) | existant à enrichir | `include/nativeui/list_view.hpp` | `src/list_view.cpp` |
| [TableView<Key>](table_view.md) | nouveau à implémenter | `include/nativeui/table_view.hpp` | `src/table_view.cpp` |
| [TreeView<Key>](tree_view.md) | nouveau à implémenter | `include/nativeui/tree_view.hpp` | `src/tree_view.cpp` |
| [OutlineView<Key>](outline_view.md) | nouveau à implémenter | `include/nativeui/outline_view.hpp` | `src/outline_view.cpp` |
| [OutlineTableView<Key>](outline_table_view.md) | nouveau à implémenter | `include/nativeui/outline_table_view.hpp` | `src/outline_table_view.cpp` |
| [GridView<Key>](grid_view.md) | nouveau à implémenter | `include/nativeui/grid_view.hpp` | `src/grid_view.cpp` |
| [Tabs<T>](tabs.md) | existant à extraire | `include/nativeui/tabs.hpp` | `src/tabs.cpp` |
| [Sidebar<Key>](sidebar.md) | nouveau à implémenter | `include/nativeui/sidebar.hpp` | `src/sidebar.cpp` |
| [Breadcrumbs](breadcrumbs.md) | nouveau à implémenter | `include/nativeui/breadcrumbs.hpp` | `src/breadcrumbs.cpp` |
| [HistoryButton](history_button.md) | nouveau à implémenter | `include/nativeui/history_button.hpp` | `src/history_button.cpp` |

### Conteneurs

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Row](row.md) | existant à extraire | `include/nativeui/row.hpp` | `src/row.cpp` |
| [Column](column.md) | existant à extraire | `include/nativeui/column.hpp` | `src/column.cpp` |
| [Grid](grid.md) | existant à enrichir | `include/nativeui/grid.hpp` | `src/grid.cpp` |
| [Scroll](scroll.md) | existant à extraire | `include/nativeui/scroll.hpp` | `src/scroll.cpp` |
| [ScrollView](scroll_view.md) | existant à extraire | `include/nativeui/scroll_view.hpp` | `src/scroll_view.cpp` |
| [Clip](clip.md) | existant à extraire | `include/nativeui/clip.hpp` | `src/clip.cpp` |
| [Flex](flex.md) | existant à extraire | `include/nativeui/flex.hpp` | `src/flex.cpp` |
| [Spacer](spacer.md) | existant à extraire | `include/nativeui/spacer.hpp` | `src/spacer.cpp` |
| [Stack](stack.md) | existant à extraire | `include/nativeui/stack.hpp` | `src/stack.cpp` |
| [Padding](padding.md) | existant à extraire | `include/nativeui/padding.hpp` | `src/padding.cpp` |
| [SplitView](split_view.md) | nouveau à implémenter | `include/nativeui/split_view.hpp` | `src/split_view.cpp` |
| [Collapsible](collapsible.md) | nouveau à implémenter | `include/nativeui/collapsible.hpp` | `src/collapsible.cpp` |
| [Accordion](accordion.md) | nouveau à implémenter | `include/nativeui/accordion.hpp` | `src/accordion.cpp` |
| [Form](form.md) | nouveau à implémenter | `include/nativeui/form.hpp` | `src/form.cpp` |
| [Field](field.md) | nouveau à implémenter | `include/nativeui/field.hpp` | `src/field.cpp` |
| [Fieldset](fieldset.md) | nouveau à implémenter | `include/nativeui/fieldset.hpp` | `src/fieldset.cpp` |

### Dialogues et messages

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Dialog](dialog.md) | existant à enrichir | `include/nativeui/dialog.hpp` | `src/dialog.cpp` |
| [Popover](popover.md) | nouveau à implémenter | `include/nativeui/popover.hpp` | `src/popover.cpp` |
| [Tooltip](tooltip.md) | existant à enrichir | `include/nativeui/tooltip.hpp` | `src/tooltip.cpp` |
| [Toast](toast.md) | nouveau à implémenter | `include/nativeui/toast.hpp` | `src/toast.cpp` |

### Indicateurs

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [ProgressBar](progress_bar.md) | existant à enrichir | `include/nativeui/progress_bar.hpp` | `src/progress_bar.cpp` |
| [Spinner](spinner.md) | nouveau à implémenter | `include/nativeui/spinner.hpp` | `src/spinner.cpp` |
| [Meter](meter.md) | existant à enrichir | `include/nativeui/meter.hpp` | `src/meter.cpp` |
| [Badge](badge.md) | nouveau à implémenter | `include/nativeui/badge.hpp` | `src/badge.cpp` |

### Composition retenue

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [Visibility](visibility.md) | existant à enrichir | `include/nativeui/visibility.hpp` | `src/visibility.cpp` |
| [Enabled](enabled.md) | existant à enrichir | `include/nativeui/enabled.hpp` | `src/enabled.cpp` |
| [ReadOnly](read_only.md) | existant à enrichir | `include/nativeui/read_only.hpp` | `src/read_only.cpp` |
| [If](if.md) | existant à extraire | `include/nativeui/if.hpp` | `src/if.cpp` |
| [Switch<T>](switch.md) | existant à extraire | `include/nativeui/switch.hpp` | `src/switch.cpp` |
| [ForEach<T>](for_each.md) | existant à extraire | `include/nativeui/for_each.hpp` | `src/for_each.cpp` |
| [FocusScope](focus_scope.md) | existant à extraire | `include/nativeui/focus_scope.hpp` | `src/focus_scope.cpp` |
| [CommandScope](command_scope.md) | existant à extraire | `include/nativeui/command_scope.hpp` | `src/command_scope.cpp` |
| [StyleScope](style_scope.md) | existant à extraire | `include/nativeui/style_scope.hpp` | `src/style_scope.cpp` |

### Images

| Composant | Statut | Header cible | Implémentation cible |
| --- | --- | --- | --- |
| [ImageView](image_view.md) | nouveau à implémenter | `include/nativeui/image_view.hpp` | `src/image_view.cpp` |
| [IconView](icon_view.md) | nouveau à implémenter | `include/nativeui/icon_view.hpp` | `src/icon_view.cpp` |
| [Avatar](avatar.md) | nouveau à implémenter | `include/nativeui/avatar.hpp` | `src/avatar.cpp` |

## 3. Correspondance exhaustive avec MyGo

| Famille du catalogue MyGo | Spécification NativeUI |
| --- | --- |
| Button | [button](button.md) |
| Link | [link](link.md) |
| Menu button | [popup_menu](popup_menu.md) |
| Context menu | [context_menu](context_menu.md) |
| Toggle | [toggle_button](toggle_button.md), [toggle_group](toggle_group.md) |
| Segmented control | [segmented_control](segmented_control.md) |
| Toolbar | [toolbar](toolbar.md) |
| Checkbox | [checkbox](checkbox.md), [checkbox_group](checkbox_group.md) |
| Switch | [toggle](toggle.md) |
| Radio | [radio_button](radio_button.md) |
| Select | [combo_box](combo_box.md) |
| Slider | [slider](slider.md) |
| Range slider | [range_slider](range_slider.md) |
| Stepper | [stepper](stepper.md) |
| Rating | [rating](rating.md) |
| Text input | [text_input](text_input.md), [text_area](text_area.md) |
| Number input | [number_input](number_input.md) |
| Search field | [search_field](search_field.md) |
| Combobox | [editable_combo_box](editable_combo_box.md) |
| Autocomplete | [autocomplete](autocomplete.md) |
| Token field | [token_field](token_field.md) |
| Editable text | [editable_text](editable_text.md) |
| Find bar | [find_bar](find_bar.md) |
| Calendar | [calendar](calendar.md) |
| Date input | [date_input](date_input.md) |
| Time input | [time_input](time_input.md) |
| Color picker | [color_picker](color_picker.md), [color_well](color_well.md) |
| List | [list_view](list_view.md) |
| Table | [table_view](table_view.md) |
| Tree | [tree_view](tree_view.md) |
| Outline | [outline_view](outline_view.md), [outline_table_view](outline_table_view.md) |
| Grid view | [grid_view](grid_view.md) |
| Scroll view | [scroll_view](scroll_view.md) |
| Grid | [grid](grid.md) |
| Split view | [split_view](split_view.md) |
| Collapsible | [collapsible](collapsible.md) |
| Accordion | [accordion](accordion.md) |
| Form | [form](form.md), [field](field.md), [fieldset](fieldset.md) |
| Tabs | [tabs](tabs.md) |
| Sidebar | [sidebar](sidebar.md) |
| Breadcrumbs | [breadcrumbs](breadcrumbs.md) |
| Back and forward buttons | [history_button](history_button.md) |
| Dialog | [dialog](dialog.md) |
| Alert dialog | [dialog](dialog.md) |
| Popover | [popover](popover.md) |
| Tooltip | [tooltip](tooltip.md) |
| Toast | [toast](toast.md) |
| Progress bar | [progress_bar](progress_bar.md) |
| Spinner | [spinner](spinner.md) |
| Meter | [meter](meter.md) |
| Badge | [badge](badge.md) |
| Image | [image_view](image_view.md) |
| Icon | [icon_view](icon_view.md) |
| Avatar | [avatar](avatar.md) |

### Correspondances qui changent de nom

- MyGo Switch → NativeUI **Toggle** ; `Switch<T>` demeure la composition
  conditionnelle, et aucun alias `Switch = Toggle` n’est introduit.
- MyGo Toggle → **ToggleButton** ; ToggleGroup est le groupe de boutons pressés.
- MyGo Select → **ComboBox<T>** ; MyGo Combobox → **EditableComboBox**.
- MyGo MenuButton → **PopupMenu** ; ContextMenu a un décorateur propre.
- MyGo Text/Textf → **Label** ; TextLabel reste l’alias actuel. RichText
  spécifie les spans et liens intégrés, en plus des 54 familles du catalogue.
- MyGo Box/Column → **Column** ; Row/Spacer/Divider restent des primitives
  identifiées, sans créer un composant Box redondant.
- ImageView et IconView sont des widgets ; **Image** et **SvgIcon** sont
  les ressources existantes. TreeView affiche une hiérarchie ; **Tree**
  reste le runtime de composition.
- Les bases sans apparence MyGo servent de références comportementales et de
  personnalisation dans les pages ; elles ne deviennent pas 15 classes publiques
  supplémentaires sans responsabilité autonome.
- PrimaryButton est une variante de Button ; AlertDialog une variante de
  Dialog ; SplitVertical une orientation de SplitView ; Back/ForwardButton
  deux directions de HistoryButton. Items d’arbre, sections de Sidebar et
  items d’Accordion restent dans le module du parent.

## 4. Architecture commune

### Composition et état

Les builders produisent un `ui::Spec`, normalement par `spec() &&`, et le Tree
possède leurs composants retenus. Les contrôleurs existants, notamment Dialog,
gardent leur API de contrôleur ; cette convention ne les transforme pas en
builders. Les constructeurs/templates nécessaires à la composition C++ restent
des adaptateurs, pas un moteur d’exécution par frame transplanté de MyGo.

Les opérations de Tree, widgets et State appartiennent au thread UI/main.
Un adaptateur audio ou hôte applique ses mises à jour après passage par son
propre pont sûr entre threads. NativeUI ne reçoit ni IDs de paramètres, ni
automatisation, ni gestes de formats audio. Les mises à jour externes sont
reflétées sans action utilisateur synthétique ; une édition temporaire ne doit
jamais réécrire une ancienne valeur après une mise à jour externe.

Les API existantes conservent leurs types numériques, notamment `float`.
Les nouveaux contrôles numériques utilisent `double`, sauf conversion explicite
vers une primitive existante. Le State source doit respecter la durée de vie
documentée ; un Binding est un lien au modèle, pas une permission de conserver
des pointeurs applicatifs périmés. Quand le constructeur convertit State en Binding, le control block reste lisible
après destruction du State : valid() devient faux, get() donne la dernière
valeur, set() est ignoré et observe() retourne une inscription inactive. Il
n’existe pas de notification implicite de destruction ; toute nouvelle mutation
vérifie valid() et ne publie pas de callback utilisateur si le modèle a disparu.
Les véritables références empruntées des API historiques conservent leurs
exigences de durée de vie, précisées dans leurs pages. Les abonnements, animations
et overlays sont possédés par l’instance et libérés au démontage.

### Input, focus et notifications

Réutiliser le routage retenu, la capture de pointeur, les focus scopes,
les commandes et les overlays du toolkit. Une interaction est annulée proprement
quand son composant est retiré, caché ou désactivé. Tab traverse les widgets ;
les groupes qui utilisent les flèches conservent une entrée de Tab cohérente.
Le typeahead des collections utilise par défaut un repli ASCII sans casse ;
les caractères UTF-8 non ASCII restent comparés exactement. Ce choix v1 est
distinct du casefold Unicode MyGo, sans introduire de backend Unicode implicite.
Le buffer et la navigation sont décrits par ListView puis réutilisés par ses
consommateurs. Les règles locales de Space, Enter, Escape, molette et relâchement sont définies
dans chaque page : l’extraction ne les uniformise pas silencieusement.
Le Key actuel ne couvre pas PageUp/PageDown, les touches de fonction ou Menu ;
Command couvre actuellement les commandes d’édition. Les besoins de Calendar,
collections, FindBar, ContextMenu et Form exigent des ajouts explicitement cibles
aux enums et aux traductions plateforme. Ajouter ces valeurs en fin d’enum,
préserver les valeurs numériques historiques et tester les événements normalisés.
InputType::ContextMenu existe déjà ; sa présence ne valide pas tous les raccourcis
natifs. Submit/Cancel restent des commandes explicites, sans convertir
automatiquement Enter de tous les éditeurs en validation du formulaire.

Un callback déjà commencé qui lève n’est pas automatiquement rejoué. Restaurer
captures, transactions et indicateurs de dispatch avant propagation C++.
Les callbacks susceptibles de détruire leur propriétaire terminent le travail
sur le composant avant l’appel, ou revalident une identité sûre après l’appel.
Les fermetures différées portent des identités/lifetimes sûrs ; un échec de
queue n’autorise pas une destruction synchrone sans garantie de durée de vie. Destruction et libération
des abonnements restent no-throw.

### Layout, style et ressources

La géométrie publique est logique ; la frontière plateforme applique le facteur
de framebuffer. Hit-test et peinture utilisent les mêmes clips. Toute invalidation
de métriques entraîne le layout nécessaire, tandis qu’un changement purement
visuel n’impose pas la reconstruction du sous-arbre. Form et Field spécifient un futur hook optionnel de première ligne de base,
absent de ChildMetrics et Component actuels. L’extraction des layouts existants
ne présuppose pas ce hook. Les recettes de style
sont typées et leurs états disabled/read-only/selected/focused restent distincts.

Les widgets dessinent avec Painter ; Skia reste son implémentation. Décodage
d’image, parsing SVG, métriques préparées, shaders et ressources sont préparés
hors paint quand leur coût ou leur allocation l’exige. Aucun widget ne dépend
de Pugl, AppKit, Win32, Xlib, HTML ou du moteur de rendu MyGo.

### Plateforme, menus et navigation

ContextMenu, PopupMenu et l’overflow de Toolbar utilisent le même modèle de
menu et les overlays retenus. MyGo emploie des menus système ; cette cible
choisit des menus dessinés par NativeUI, avec les mêmes fonctions utilisateur
explicitement spécifiées et sans créer une nouvelle abstraction de fenêtre.
Toast spécifie une extension additive `OverlayPlacement::ViewportBottomCenter`
pour placer une pile à sa taille naturelle ; elle reste dans le service Overlay
existant. Ce placement n’est pas disponible dans le snapshot étudié.
Link réutilise DesktopServices pour ouvrir une URL ou un callback pour une
action locale. HistoryButton reçoit disponibilité et action de l’application ;
le Router MyGo, son routage URL et son cache de pages ne sont pas portés ici.

### Accessibilité et IME

Le contrat de référence est [accessibility.md](accessibility.md), avec
[SemanticInfo](../include/nativeui/semantics.hpp) et le hook
`Component::semantics()`. La présence du hook ou d’un rôle dans l’enum ne prouve
pas que tous les widgets actuels publient déjà ces informations. Chaque page
indique la sémantique cible et les actions réellement permises par l’état.

Les ponts natifs macOS/Windows/Linux restent une dépendance de T068 différée
dans le [roadmap](../ROADMAP.md). Aucun exemple n’atteste une conformité
VoiceOver, UI Automation ou AT-SPI. Un nouveau rôle absent de l’enum v1
utilise une composition des rôles existants ou Custom/Group ; toute extension
du contrat fermé exige sa propre décision d’architecture, pas un enum supposé.

Les widgets texte peuvent posséder des chemins de composition testables en
headless. Le transport natif complet de préédition IME et des rectangles de
candidats reste distinct du texte Unicode validé, conformément à DESIGN §17.4.
Les nouveaux widgets consomment ce modèle commun au lieu d’inventer chacun
un pont IME. Les relations label/description/erreur et les annonces de statut
qui dépassent SemanticInfo actuel sont des extensions explicites.

<a id="modeles-partages"></a>
## 5. Modèles partagés

Les modèles suivants sont des API cibles lorsqu’ils ne sont pas présents dans
le snapshot. Les signatures complètes et règles de validation appartiennent
aux pages qui les définissent ; les composants consommateurs réutilisent ces
types plutôt que créer des variantes incompatibles.

| Modèle | Définition et consommateurs |
| --- | --- |
| `SelectionSnapshot<Key>`, `Selection<Key>`, `CollectionItem<Key>` | [ListView](list_view.md) définit sélection ordonnée, clé active, ancre et métadonnées ; TableView, GridView et outlines les réutilisent. |
| `TreeNode<Key>` et état d’expansion | [TreeView](tree_view.md) définit snapshot hiérarchique, clés/parent et branches ; [OutlineView](outline_view.md) et Sidebar consomment cette identité. |
| `TableColumn`, `TableLayout`, `SortOrder` | [TableView](table_view.md) définit IDs stables, géométrie/persistance et demande de tri ; [OutlineTableView](outline_table_view.md) réutilise ces types. |
| `RadioGroup<T>` | [RadioButton](radio_button.md) garde le contrôleur de sélection partagé actuel ; il ne devient pas un widget autonome artificiel. |
| `ScrollState`, `VirtualListState<Key>` | [Scroll](scroll.md), [ScrollView](scroll_view.md) et [ListView](list_view.md) préservent les contrôleurs actuels et définissent leurs enrichissements séparément. |
| `PopupMenuItem` | [PopupMenu](popup_menu.md) garde actions/séparateurs existants et spécifie les extensions de menu ; ContextMenu et Toolbar les réutilisent. |
| `OverlayHandle`, `DialogSpec`, `DialogResult` | [Dialog](dialog.md), [Popover](popover.md), [Tooltip](tooltip.md), [Toast](toast.md) conservent un propriétaire Overlay unique par UI. |
| Date et heure civiles | [Calendar](calendar.md)/DateInput utilisent `std::chrono::sys_days`, absence via optional ; [TimeInput](time_input.md) utilise des secondes depuis minuit, sans fuseau horaire. |
| Couleur et image | ColorPicker/ColorWell consomment `ui::Color` en sRGB ; ImageView/IconView partagent les handles immuables Image/SvgIcon existants. |

Les collections utilisent des clés copiables et comparables par égalité ; elles
n’imposent pas que les clés soient des entiers ou hashables. Le noyau non
template reçoit des identités opaques/lifetime-safe et des adaptateurs de
comparaison, construction et notification. Une clé retirée ne réactive pas
une ancienne identité sémantique si elle est réinsérée. Les index de tokens
séparent coût de préparation et coût de scroll : avec clés encodables, la
préparation peut utiliser un index trié O(N log N) ; le fallback de clés
comparables seulement par égalité peut coûter O(N²). Les copies de métadonnées
restent O(N), et le scroll ne refait pas cette résolution de clés.

La virtualisation doit construire les lignes visibles et l’overscan, tout en
exposant un dataset logique cohérent. Les snapshots de métadonnées O(N) sont
partagés pendant les simples changements de scroll/focus ; les lectures
sémantiques ne déclenchent ni row factory, ni mutation du Tree. Hauteurs
variables, multisélection et hiérarchies doivent étendre ce contrat explicitement.

## 6. Un header et un cpp par composant

Pour chaque nom du catalogue :

```text
docs/<composant>.md
include/nativeui/<composant>.hpp
src/<composant>.cpp
```

Le `.hpp` contient les déclarations publiques, les types de configuration
nécessaires, et uniquement les adaptations templates indispensables.
Le `.cpp` contient un véritable noyau non template : comportement retenu,
mesure/layout, interactions, résolution de présentation et peinture.
L’effacement de type conserve les contraintes et capacités des clés utilisateurs ;
des instanciations explicites sur trois types connus ne satisfont pas ce contrat.

Les variantes (bouton accent, slider stepped, alerte, orientations, bouton
précédent/suivant), items/sections et classes de runtime internes restent dans
le couple du parent. Les contrôleurs partagés et types de données peuvent avoir
leurs propres modules utiles, mais ne sont pas comptés comme widgets autonomes.
Un cpp vide et une façade qui laisse toute l’implémentation dans un `.inc`
ne valident pas la séparation.

Préserver les includes historiques : `widgets.hpp`, `layout.hpp`,
`combo_popup.hpp`, `dynamic.hpp`, `component_state.hpp`, `nativeui.hpp`,
ainsi que les headers de services et styles déjà publics. Ils deviennent
des agrégateurs/compatibilité au fur et à mesure des extractions ; leurs
includes transitifs actuellement utilisés sont conservés et testés.
Préserver aussi les classes Component visibles publiquement, aliases,
overloads State/Binding, guides de déduction, contraintes templates,
qualifications `&`/`&&`, types d’options et styles.

Les `.cpp` futurs sont inscrits dans `NativeUI::Core`. Leur extraction ne
modifie ni le pin Skia/Pugl, ni les bridges macOS propres aux consommateurs,
ni le modèle installed-package. Le remplacement d’un `.inc` n’intervient
qu’après la migration de son contenu ; aucune seconde définition active
du même runtime n’est conservée.

## 7. Dépendances et ordre de réalisation

L’ordre ci-dessous organise les chantiers ; chaque composant conserve ses
critères propres et les dépendances explicites de sa page.

1. **Extraction compatible et exemples** : widgets/layout/wrappers existants,
   headers autonomes, linkage Core et includes collectifs ; tests de baseline
   avant tout changement comportemental.
2. **Petits contrôles et conteneurs** : ToggleButton/ToggleGroup, SegmentedControl,
   CheckboxGroup, Stepper/NumberInput, SearchField, Divider, Badge,
   Collapsible/Accordion, SplitView et Form/Field/Fieldset.
3. **Overlays et saisie composée** : modèle de menu enrichi, ContextMenu,
   Popover, EditableComboBox/Autocomplete, EditableText, TokenField,
   FindBar et Toast. Ces widgets réutilisent focus/input/text/overlay.
4. **Collections** : modèle commun de sélection et dataset, enrichissement
   ListView, TableView et TreeView, puis OutlineView/OutlineTableView,
   GridView et Sidebar ; mesurer les coûts de scroll et publication des snapshots.
5. **Navigation et présentation** : Breadcrumbs/HistoryButton, Toolbar et
   overflow, ImageView/IconView/Avatar, RichText, progression indéterminée,
   Spinner et seuils de Meter.
6. **Dates et couleurs** : Calendar → DateInput ; TimeInput ; ColorPicker →
   ColorWell. Aucun service de calendrier, timezone ou gestion système de
   couleur implicite n’est introduit.

Le graphe concret comporte notamment TextInput → NumberInput/SearchField/
EditableComboBox ; EditableComboBox → Autocomplete/TokenField ; ScrollView →
ListView → TableView/GridView ; TreeView + ListView → OutlineView ;
OutlineView + TableView → OutlineTableView ; PopupMenu + Overlay →
ContextMenu/Toolbar ; Form + Field + Fieldset partagent une seule mesure
des labels ; ressources Image/SvgIcon → ImageView/IconView/Avatar.

## 8. Validation et livraison future

### Documentation

Le contrôle documentaire vérifie la présence des 83 pages et onze rubriques,
la couverture exacte des 54 familles MyGo, les liens locaux, les noms/API
entre pages, et un couple header/cpp cible distinct par composant. Il contrôle
que le diff du dépôt ne contient que les documents demandés. Les API futures
sont relues comme propositions ; elles ne sont pas annoncées compilées ou
testées sans implémentation.

### Implémentation de chaque composant

- Compiler son header seul et à travers les includes historiques ; tester
  aussi State/Binding, clés personnalisées et plusieurs translation units.
- Ajouter l’exemple public `examples/features/<composant>.cpp` et son
  `nativeui_example_<composant> --self-test`, utilisable sans affichage quand
  le composant le permet. Les cpp et exemples sont des livrables futurs.
- Tester pointeur/clavier, disabled/read-only, updates externes, limites,
  retrait en cours d’interaction et précision du hit-test/layout.
- Tester failure injection, récupération après exception/échec de scheduling,
  destruction différée et scénario détruire A/continuer B dans deux UI.
- Vérifier métriques/headless/goldens spécifiques, animations à l’arrêt au
  repos et allocation/complexité des collections lorsque pertinent.
- Appliquer CODE_REVIEW, tests/sanitizers/platform/package requis par le ticket,
  avec les builds locaux exécutés en série et zéro avertissement non approuvé.

Une spécification écrite n’est pas un statut Ready/Done automatique. Les issues,
décisions d’architecture nécessaires, preuves d’exécution et gates de merge
continuent de suivre [AGENTS.md](../AGENTS.md). Ces documents n’altèrent pas
les dépendances officielles ni le contrat d’accessibilité v1 par simple rédaction.
