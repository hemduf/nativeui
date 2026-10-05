# Galerie de démonstration NativeUI

L’application **NativeUI — Le laboratoire des composants** utilise les **83 composants publics** du [catalogue](widgets.md), dans huit écrans interactifs en français. Son code final est [examples/features/widgets_gallery.cpp](../examples/features/widgets_gallery.cpp). Elle utilise les API publiques actuelles, avec `NativeUI::Core` pour les widgets et la plateforme NativeUI pour la fenêtre.

![Écran Texte et dessin de la galerie NativeUI](assets/widgets_gallery.png)

## Lancer l’application

Le bundle macOS compilé dans ce dépôt se trouve ici :

```text
/Volumes/T7/Code/nativeui/build-widgets/nativeui_example_widgets_gallery.app
```

```sh
open /Volumes/T7/Code/nativeui/build-widgets/nativeui_example_widgets_gallery.app
```

Pour reconstruire avec la configuration existante :

```sh
TMPDIR=/Volumes/T7/tmp CMAKE_BUILD_PARALLEL_LEVEL=1 cmake -S /Volumes/T7/Code/nativeui -B /Volumes/T7/Code/nativeui/build-widgets
TMPDIR=/Volumes/T7/tmp CMAKE_BUILD_PARALLEL_LEVEL=1 cmake --build /Volumes/T7/Code/nativeui/build-widgets --target nativeui_example_widgets_gallery
```

La découverte CMake des exemples ajoute automatiquement la cible, son identifiant de consommateur `org.nativeui.example.widgets-gallery` et son test CTest. Sur macOS, `nativeui_add_application` fournit le bundle et le pont plateforme propre à ce consommateur. Sur les autres plateformes, utiliser l’exécutable produit par cette même cible ; la validation réalisée ici concerne macOS.

## Explorer les composants

| Écran | Composants et expériences |
| --- | --- |
| Contrôles et actions | Button, Link, PopupMenu, ContextMenu, ToggleButton, ToggleGroup, SegmentedControl, Checkbox, CheckboxGroup, RadioButton, Toggle, ComboBox, Slider, RangeSlider, Knob, Stepper, NumberInput et Rating. Le clic droit ouvre le menu contextuel ; le curseur pilote aussi la progression. |
| Saisie et formulaires | Form, Field, Fieldset, TextInput, TextArea, SearchField, EditableComboBox, Autocomplete, TokenField, EditableText et FindBar. Saisir un nom, choisir une suggestion, ajouter des tags et renommer un fichier. Les résultats de recherche suivent le contenu réel des notes ; la navigation indique le résultat courant dans la barre d’état. |
| Dates et couleurs | Calendar, DateInput, TimeInput, ColorPicker et ColorWell. Calendrier et champ de date partagent la sélection. Le Picker présente les canaux, l’alpha, le champ hexadécimal et les nuances ; ColorWell ouvre un sélecteur. |
| Collections | ListView, TableView, TreeView, OutlineView, OutlineTableView, GridView, Tabs, Breadcrumbs et HistoryButton. Table et grille contiennent 120 éléments, modifiables par le bouton d’ajout ; les vues arborescentes exposent un projet. Le tri par Nom est appliqué aux données. Chaque vue possède sa sélection propre. |
| Conteneurs | Row, Column, Grid, Scroll, ScrollView, Clip, Flex, Spacer, Stack, Padding, SplitView, Collapsible et Accordion. Déplacer le séparateur, ouvrir les sections, déplacer le contenu horizontal et comparer les layouts. |
| Composition | Visibility, Enabled, ReadOnly, If, Switch, ForEach, FocusScope, CommandScope et StyleScope. Modifier l’état d’un sous-arbre, changer de branche, ajouter/retirer des éléments et observer un accent local. |
| Texte et dessin | Label, Header, RichText, Canvas, Divider, ImageView, IconView et Avatar. Texte multilingue et lien RichText, ressources SVG, avatars et toile réagissant au clic. Le nom du formulaire met à jour les initiales de l’avatar. |
| Messages et indicateurs | Dialog, Popover, Tooltip, Toast, ProgressBar, Spinner, Meter et Badge. Ouvrir les présentations, confirmer ou annuler un dialogue, attendre l’expiration d’un toast et régler les indicateurs. |

La navigation permanente utilise **Sidebar**. Le bandeau utilise **Toolbar**, IconView, Label et Badge ; la barre d’état affiche les actions réalisées. Les composants communs réapparaissent dans plusieurs écrans : la couverture est de **83 types distincts**, sans compter RadioGroup, les sélections et les contrôleurs comme widgets supplémentaires.

Tab et Maj+Tab parcourent les contrôles. Échap ferme les présentations qui le prévoient. La molette et les barres défilantes donnent accès à tous les exemples. En fenêtre réduite, les pages conservent une largeur de lecture de 1 000 points et peuvent défiler horizontalement. Les collections ont des hauteurs bornées afin de conserver leur virtualisation.

`ForEach` conserve son contrat actuel de superposition : la galerie donne explicitement à chaque enfant sa position verticale à partir de sa clé. Cette démonstration utilise l’ajout/retrait en fin de liste. `HistoryButton` et Breadcrumbs montrent leurs callbacks injectés ; la barre d’état décrit la destination, sans introduire un Router.

Les valeurs restent en mémoire dans la fenêtre. « Enregistrer » démontre Toast et Dialog ; il n’écrit pas de fichier de projet.

## Validation sans affichage

```sh
/Volumes/T7/Code/nativeui/build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --self-test
TMPDIR=/Volumes/T7/tmp ctest --test-dir /Volumes/T7/Code/nativeui/build-widgets -R '^nativeui_(example_widgets_gallery_self_test|widget_(calendar|date_input|sidebar|color_picker|color_well)_tests)$' --output-on-failure
```

Le self-test rend les huit écrans à 1 280 × 900, puis à 780 × 600 avec une échelle de 1,25. Il vérifie les trois onglets de collections, les données vides des table/grille, l’ajout après tri décroissant sans clé dupliquée, le nombre de résultats de recherche, les transitions de composition, le popover, le dialogue fermé par Échap, l’expiration du toast avec horloge manuelle et deux modèles indépendants.

Les captures PPM de chaque écran se produisent sans serveur graphique :

```sh
/Volumes/T7/Code/nativeui/build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --self-test --snapshot-dir /Volumes/T7/tmp/nativeui-gallery-snapshots
```

L’export ajoute huit fichiers `page_1.ppm` à `page_8.ppm` dans le dossier choisi. Il n’est pas exécuté pendant le test CTest ordinaire.

Le test natif crée une vraie fenêtre macOS, attend son premier rendu, puis demande sa fermeture différée :

```sh
/Volumes/T7/Code/nativeui/build-widgets/nativeui_example_widgets_gallery.app/Contents/MacOS/nativeui_example_widgets_gallery --window-self-test
```

Validation locale du 5 octobre 2026 : **6/6 tests CTest réussis**, compilation Release sans diagnostic d’avertissement, rendu des huit écrans, démarrage et fermeture de la fenêtre macOS réussis. La couverture du catalogue et les durées de vie ont également fait l’objet d’une relecture indépendante.

## Corrections découvertes par la galerie

- **Calendar / DateInput :** le calendrier crée désormais ses 44 enfants initiaux depuis le runtime propre à chaque compilation : deux boutons de navigation et 42 cellules. Les tests vérifient le diagnostic structurel vide, la projection sémantique initiale et le rendu du calendrier popup.
- **Sidebar :** les textes des lignes et les chevrons utilisent le centre vertical attendu par Painter. Un test de rendu reproduit le défaut de texte tronqué, puis vérifie les glyphes aux échelles 1 et 2.

- **ColorPicker / ColorWell :** les nuances sont disponibles dès le premier rendu ; leur sélection conserve les contrôles de mutation et respecte les états désactivé/lecture seule ainsi que l’expiration du binding. Le champ hexadécimal utilise une géométrie compacte et ne déborde plus sur le nuancier. Les styles explicites du champ sont conservés.

La galerie n’ajoute ni backend, ni état global mutable, ni sémantique audio. Le modèle appartient à l’application et vit plus longtemps que son UI. Dialog et Toast appartiennent à la même fenêtre ; leurs callbacks n’effacent pas leurs propriétaires et ne conservent pas de contexte d’événement. Le dispatcher de la fenêtre pilote les présentations temporisées. Les limitations IME et des ponts natifs d’accessibilité restent celles du framework : cette application ne constitue pas une qualification supplémentaire de ces ponts.
