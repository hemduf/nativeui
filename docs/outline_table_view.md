# OutlineTableView<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Hiérarchie virtualisée avec cellules en colonnes : disclosure dans la colonne principale, tri/resize/reorder dans le header. NativeUI ne possède pas ce composant. Il combine les contrats d’[OutlineView](outline_view.md) et [TableView](table_view.md).

MyGo `ui/outline.go` : `OutlineTable`, `prefix`, `keys`, et `ui/table.go` : `table`, `tableHeader`. La première colonne déclarée porte le disclosure. La cible donne un ID explicite pour que déplacer les colonnes ne déplace pas arbitrairement la hiérarchie.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
template<class Key>
OutlineTableView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
                 Binding<std::vector<Key>> expanded);
template<class Key>
OutlineTableView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
                 State<std::vector<Key>>& expanded);
OutlineTableView&& columns(std::vector<TableColumn>) &&;
OutlineTableView&& tree_column(ColumnId id) &&;
OutlineTableView&& cell(std::function<Spec(const TreeNode<Key>&,const ColumnId&)>) &&;
OutlineTableView&& layout(Binding<TableLayout>) &&;
OutlineTableView&& sort(Binding<std::optional<SortOrder>>) &&;
OutlineTableView&& row_heights(ListRowHeights) &&;
OutlineTableView&& selection_mode(SelectionMode) &&;
OutlineTableView&& on_sort_request(std::function<void(const SortOrder&)>) &&;
OutlineTableView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
OutlineTableView&& on_activate(std::function<void(const Key&)>) &&;
OutlineTableView&& autofit_width(std::function<double(const ColumnId&)>) &&;
OutlineTableView&& style(OutlineTableViewStyle) &&;
Spec spec() &&;
```

State overloads de layout/sort conservés dans cette cible. Exemple futur :

```cpp
ui::State<std::vector<ui::TreeNode<int>>> nodes{{{1,std::nullopt,"Dossier",true,true}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
ui::State<std::vector<int>> expanded{{}};
auto outline = ui::OutlineTableView<int>{nodes,selection,expanded}
    .columns({ui::TableColumn{.id="name",.title="Nom"}}).tree_column("name")
    .cell([](const auto& node,const auto&){return std::move(ui::Label{node.label}).spec();});
```

Default tree column = première déclarée si non vide ; IDs/types sont ceux de TableView, TreeNode de TreeView et Selection de ListView, jamais duplications.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Graph, expansion, selection et column layout sont quatre domaines séparés. Row keys suivent les nodes, columns suivent IDs. Demande de tri ne produit pas un tri global brisant parents : l’application renvoie un snapshot triant siblings tout en conservant parents. Aucune persistence fichier interne. Un Binding invalide de domaine interdit ses mutations, sans émettre callback fictif.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Header gestures TableView : resize/reorder/click sort/autofit distincts. Body gestures OutlineView : disclosure click, navigation Right/Left, activation row. Le disclosure conserve la même tree_column quand son header est déplacé. Keys Left/Right vont à la hiérarchie si focus sur row ; cell editor qui consomme ses touches conserve priorité. PointerCancel/Échap annule column drag sans fermer nodes. Wheel X header/body synchronisée, Y body uniquement.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Flatten visible tree metadata avant fenêtre virtuelle. Cell tree_column réserve depth*indent + chevron dans sa largeur, les autres gardent padding normal. Height row = max cell metrics ; width change remeasure variable heights tout en conservant key/inset. Header Y fixe et X commun. Tree_column narrow clippe content avec chevron hit target borné, sans déplacer les autres colonnes. Zero columns signifie aucune cell factory, graph state gardé mais body presentation vide.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

OutlineTableViewStyle réutilise les types row/header/disclosure des deux parents, avec composition owned et priorité explicite locale ; ne pas copier slots Theme imaginaires. Guide de resize/reorder et chevron sont peints par le .cpp. Selected/expanded/sort marker et focus distincts ; invalidations classées par domaine.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

TreeTable/Cell/ColumnHeader absents des rôles actuels ; Group/Custom provisoires, extension neutre future pour relations hiérarchiques et colonnes. Logical metadata et geometry generation immutables ; aucun cell/row factory depuis lecteur. Expansion, selection et columns publiés dans une génération cohérente, pas mélange ancien ordre avec nouveaux widths.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Un changement dataset et layout pendant input prépare une nouvelle vue cohérente au checkpoint. ColumnId/tree_column supprimée : si nouvelle config ne la contient plus, default première colonne et diagnostic ; explicit tree_column invalide initial refuse config. Retrait node capturé/focus récupéré via Outline. Callback sort/expansion réentrant peut modifier graph ; la pile termine sans référence cell stale. Failure provider fit conserve widths antérieurs.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend kernels OutlineView/TableView/ListView et modèles communs. Valider graph et column IDs ; cycles/duplicate/orphan refusés selon Outline. Min/max finites, tree_column absente initial invalid_argument. Empty graph/columns, très grande depth, largeur nulle, colonne fixée ou déplacée et external sort invalid sont couverts. Pas de Router ou lazy I/O.

Les touches `Key::PageUp` et `Key::PageDown` sont des additions cibles en fin de l’enum portable actuel, avec traduction plateforme et tests. Le source actuel ne les définit pas. Typeahead utilise les InputEvent de texte commité ; aucun support natif complet IME n’est supposé.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/outline_table_view.hpp` et `src/outline_table_view.cpp`.

outline_table_view.hpp expose son builder/style et adaptateurs typed ; outline_table_view.cpp est le noyau orchestration de graphe visible/columns/gestures/layout/paint. Partager algorithmes non templates des deux composants sans maintenir deux caches concurrents de hauteur ou scroll. Aucun fichier supplémentaire par cell, disclosure ou header.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `outline_table_tree_column_id` : reorder conserve disclosure dans son ID.
- `outline_table_sibling_sort` : callback tri, parents préservés par snapshot app.
- `outline_table_header_body_sync` : horizontal exact et vertical header fixe.
- `outline_table_indent_autofit` : inclut indent/chevron dans largeur de la colonne.
- `outline_table_resize_anchor` : variable rows avec anchor stable.
- `outline_table_editor_keys` : Left/Right consommés par edit cell respectés.
- `outline_table_remove_column_node` : capture/focus safe, fallback explicite.
- `outline_table_combined_fault` : pas de génération mixte lors failure graph/layout.
- `outline_table_semantics_no_factory` : snapshots sans callback cell.

Créer l’exemple public futur `examples/features/outline_table_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
