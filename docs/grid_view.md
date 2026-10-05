# GridView<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Collection d’items virtualisée en grille à colonnes adaptatives, avec sélection et navigation 2D. NativeUI possède [Grid](grid.md), un layout eager, mais pas ce modèle de grande collection.

MyGo `ui/gridview.go` : `GridState`, `GridView`, `gridFit`, `keys`, `reorder`. Les colonnes sont calculées depuis une largeur minimale et les lignes ont une hauteur fixe ; redimensionner conserve l’item en haut de viewport.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée, signatures membres de `GridView<Key>` :

```cpp
GridView(Binding<std::vector<CollectionItem<Key>>> items, Selection<Key>& selection);
GridView(State<std::vector<CollectionItem<Key>>>& items, Selection<Key>& selection);
GridView&& minimum_cell_width(double value) &&;
GridView&& cell_height(double value) &&;
GridView&& gap(double value) &&;
GridView&& cell(std::function<Spec(const CollectionItem<Key>&)>) &&;
GridView&& selection_mode(SelectionMode) &&;
GridView&& on_activate(std::function<void(const Key&)>) &&;
GridView&& on_reorder(std::function<void(const std::vector<Key>&,std::optional<Key>)>) &&;
GridView&& style(GridViewStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::CollectionItem<int>>> items{{{1,"Photo 1"},{2,"Photo 2"}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
auto grid = ui::GridView<int>{items,selection}.minimum_cell_width(140.0)
    .cell_height(120.0)
    .cell([](const auto& item){return std::move(ui::Label{item.label}).spec();});
```

Defaults min_width140 DIP, cell_height120 DIP, gap8 DIP, Single, overscan2 lignes. Contrôleur cible `GridViewState<Key>` dans ce couple avec scroll_to_key/index, visible_range last exclusive ; `.state(GridViewState<Key>&)` optionnelle, control token sûr. Les modèles item/selection sont ceux de [ListView](list_view.md).

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Items Binding copié, clé unique et tokens monotones. section_header n’a pas de sens dans GridView : reject true dans dataset, employer liste pour sections. Selection selected ordonné selon dataset ; active/anchor par key. Modifier nombre de colonnes ne modifie pas selected ni ses bindings. Les invalid bindings refusent writes/activation mutante/callback synthétique.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Left/Right suivent l’ordre linéaire du dataset (−1/+1) : droite sur le dernier item d’une rangée passe au premier de la suivante, gauche fait l’inverse ; aucun bouclage entre début et fin du dataset. Up/Down sautent de column_count et visent même colonne, clamp dernière ligne. Home/End début/fin dataset ; PageUp/Down sautent du nombre de rangées visibles. Modifiers multiple selection/toggle et typeahead selon ListView, Shift plage linéaire dataset. Enter/doubleclick activate. Reorder opt-in : dragged selected keys dans ordre dataset, insertion before key/nullopt fin, app remplace dataset ; Échap/cancel/removal annule, autoscroll local borné. Molette via ScrollView.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Columns = max(1,floor((viewport_width+gap)/(minimum_cell_width+gap))). Width effective `(max(0,W)-(C−1)*gap)/C` bornée zéro ; minimum demandé sert au calcul de C ; avec C=1 dans un viewport étroit, la cellule prend la largeur disponible même inférieure au minimum demandé, sans division par zéro. Le contenu intrinsèque débordant est clippé. Last row garde cellules mêmes widths, espaces vides non interactifs. Lignes fixes height+gap avec double cumuls ; matérialisation viewport/overscan et exceptions active capture. Resize columns conserve premier visible key+inset, jamais un ancien index de row. Extent overflow reject avant commit ; scroll O(V), copie des métadonnées O(N) ; validation/rematching de clés à nouvelle génération O(N log N) pour clés encodables et jusqu’à O(N²) pour clés equality-only, comme [ListView](list_view.md).

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

GridViewStyle : cellule selected/hover/pressed/focus, padding et insertion marker ; le contenu (image/titre) est factory. Séparer focus active et selection multiple. Gap/cell sizes changent layout/window ; color only paint. Aucune charge d’image synchrone dans layout/paint, les ressources sont applicatives et par instance.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Utiliser ListView/ListItem ou Custom pour semantics provisoires ; GridRole/row/column relations nécessitent une extension neutre future. Full dataset data-only shared metadata, item bounds calculés depuis current columns/row sizes snapshot. Aucune factory de cell lors lecture sémantique. Reflow publie une géométrie cohérente et pas des indices d’ancienne grille.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Resize/dataset replacement prépare nouvelle window et mapping stable avant commit. Une cell factory qui lève ne publie pas une rangée incomplète ; dirty recovery durable. Remove dragged key termine capture/autoscroll. on_reorder reentrant peut reconstruire dataset, ne continuer qu’avec snapshot IDs own. Controller expiré rend operations safe/no-op.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[ListView](list_view.md) sélection/tokens, [ScrollView](scroll_view.md), modèles communs. Min width/cell height strictement positives finies ; gap fini nonnegative, invalid_argument pour options invalides. Items vide/duplicate, très petite width, million items, dernière rangée partielle et child controls focusables. Key PageUp/PageDown = extension additive future du système input, translations plateformes nécessaires.

Les touches `Key::PageUp` et `Key::PageDown` sont des additions cibles en fin de l’enum portable actuel, avec traduction plateforme et tests. Le source actuel ne les définit pas. Typeahead utilise les InputEvent de texte commité ; aucun support natif complet IME n’est supposé.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/grid_view.hpp` et `src/grid_view.cpp`.

grid_view.hpp contient style/state/templates key/factory ; grid_view.cpp porte fit columns, window 2D, selection navigation, drag transactions et paint/layout. Ne pas détourner GridComponent en collection ni dupliquer son auto-placement de spans. Corps de virtualization et tokens réutilisés depuis kernels communs.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `grid_linear_row_boundary` : gauche/droite traversent les rangées, restent bornées aux extrémités et conservent l’extension Shift linéaire.

- `grid_view_fit_columns` : seuils min width+gap et width nulle.
- `grid_view_last_row` : mêmes widths, blank cells inertes.
- `grid_view_resize_anchor` : item/inset stable quand colonnes changent.
- `grid_view_navigation_2d` : arrows/Home/End/Page et skip disabled.
- `grid_view_range_selection` : Shift plage linéaire avec anchor stable.
- `grid_view_reorder_keys_cancel` : callback one-shot et removal sous drag.
- `grid_view_million_items` : Components bornés par rows visible.
- `grid_view_geometry_overflow_fault` : reject sans nouvelle génération partielle.
- `grid_view_semantic_reflow` : metadata pointer stable, geometry change data-only.

Créer l’exemple public futur `examples/features/grid_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
