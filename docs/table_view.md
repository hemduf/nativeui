# TableView<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Table de lignes virtualisées et colonnes redimensionnables/réordonnables, avec demande de tri. Aucun TableView générique dans NativeUI ; [ListView](list_view.md) et [ScrollView](scroll_view.md) sont fondations.

MyGo `ui/table.go` : `TableColumn`, `SortOrder`, `TableLayout`, `Table`, `tableHeader`, `tableFit`. Autofit mesure header/cells matérialisées ; l’application trie les rows sur demande. La cible rend persistable le layout par IDs stables et ne trie pas des objets métier opaques.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
using ColumnId=std::string;
enum class SortDirection { Ascending, Descending };
struct SortOrder {
    ColumnId column;
    SortDirection direction=SortDirection::Ascending;
    bool operator==(const SortOrder&) const=default;
};
struct TableColumn {
    ColumnId id;
    std::string title;
    double width=0.0;
    double minimum_width=60.0;
    std::optional<double> maximum_width;
    Align align=Align::Start;
    bool sortable=false;
    bool fixed=false;
    bool operator==(const TableColumn&) const=default;
};
struct TableLayout {
    std::vector<ColumnId> order;
    std::map<ColumnId,double> widths;
    bool operator==(const TableLayout&) const=default;
};
template<class Key>
TableView(Binding<std::vector<CollectionItem<Key>>> rows, Selection<Key>& selection);
template<class Key>
TableView(State<std::vector<CollectionItem<Key>>>& rows, Selection<Key>& selection);
TableView&& columns(std::vector<TableColumn>) &&;
TableView&& cell(std::function<Spec(const CollectionItem<Key>&,const ColumnId&)>) &&;
TableView&& layout(Binding<TableLayout>) &&;
TableView&& sort(Binding<std::optional<SortOrder>>) &&;
TableView&& row_heights(ListRowHeights) &&;
TableView&& selection_mode(SelectionMode) &&;
TableView&& autofit_width(std::function<double(const ColumnId&)>) &&;
TableView&& on_sort_request(std::function<void(const SortOrder&)>) &&;
TableView&& on_layout_change(std::function<void(const TableLayout&)>) &&;
TableView&& on_activate(std::function<void(const Key&)>) &&;
TableView&& style(TableViewStyle) &&;
Spec spec() &&;
```

State overloads de layout/sort délèguent aux Binding. Sans binding layout/sort, layout interne par instance ; headers tri non activables sans sort binding ou on_sort_request. Exemple futur :

```cpp
ui::State<std::vector<ui::CollectionItem<int>>> rows{{{1,"Ada"},{2,"Lin"}}};
ui::State<ui::SelectionSnapshot<int>> selected{{}};
ui::Selection<int> selection{selected};
auto table = ui::TableView<int>{rows,selection}
    .columns({ui::TableColumn{.id="name",.title="Nom",.sortable=true}})
    .cell([](const auto& row,const auto&){return std::move(ui::Label{row.label}).spec();});
```

TableColumn/TableLayout/SortOrder partagés dans ce couple, réutilisés par OutlineTableView ; Selection/CollectionItem/ListRowHeights définis dans [ListView](list_view.md). Defaults sélection Single, hauteurs variables estimées 24 DIP, overscan deux.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

- Rows Binding copié, un snapshot cohérent par génération ; clés uniques.
- Layout order mentionne des ColumnId ; inconnus ignorés, nouveaux après les connus dans ordre de déclaration.
- Fixed colonnes restent à leur place et largeur déclarée, sans resize/reorder utilisateur.
- Width map utilisateur sur colonnes non fixed seulement, bornée min/max ; width=0 distribue le reste de largeur disponible.
- selected/active/anchor suivent les clés rows, pas indices ; ancien snapshot de selection reste lisible si binding invalid, writes interdits.
- Click sort : Ascending initial puis inverse ; set sort effectif + callback demande, dataset trié explicitement par l’application.
- Updates externes de layout/sort/rows ne déclenchent pas leurs callbacks de geste.
- Aucune sérialisation automatique sur disque : application sauvegarde son TableLayout.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Body reprend selection/navigation/typeahead/activation ListView ; child cell qui consomme event a priorité. Header : bord 6 DIP arme resize, threshold 4 DIP avant drag de déplacement ; les opérations ne déclenchent pas sort. Up resize publie layout final, Échap/cancel revient au snapshot initial si pas d’external update. Double-click bord auto-fit : max header+cell widths actuellement mesurés ; autofit_width optionnel fournit une mesure applicative tous rows sans construire tout le dataset. Header sort au click release et Enter/Space sous focus. Molette horizontale synchronise header/body ; vertical ne déplace pas header. Colonnes fixed peuvent encore trier si sortable.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Header fixe Y, translation X identique au contenu body, clip commun. Row hauteur = max cells+padding et min header style. Section header row traverse les colonnes et reçoit cell factory avec première ColumnId seulement. Largeurs effectives partagées au pixel logique près entre header/cells. Déficit crée scroll horizontal, pas réduction sous minima. Width change invalide height cache/row wrapping et garde anchor key/inset. Pas de full materialisation pour autofit default ; ordre et widths préparés avant publication.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

TableViewStyle : header/body typography, padding, separator, sort marker, row style, insertion marker et focus rings. Colonne active ou sélection row distinctes. Resize marque guide ; reorder marque insertion boundary. Paint/layout classés : widths/order demandent layout ; sort marker paint sauf changement de label metrics. Style issu de Theme resolved, slots nouveaux explicites.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Table/Row/Cell/ColumnHeader n’existent pas dans SemanticRole actuel. Cible Group/Custom avec labels/range/actions permises en attendant une extension neutre séparée. Full logical rows et virtual item snapshots data-only nécessaires, derived de l’extension ListView ; aucun appel cell factory depuis bridge. Columns conservent stable IDs et name/order dans le snapshot, selection/value cohérents par génération.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Resize/reorder state contient ColumnId+generation, pas index ou pointer header. Colonne retirée/external layout modifié pendant drag : cancel capture, ne rollback pas externe. Callback autofit qui lève conserve widths précédentes et disarme interaction. Callback sort/layout qui lève n’est pas réessayé ; dirty committed layout reste durable. Remount reçoit bindings/layout actuels et recrée subscriptions locales.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[ListView](list_view.md) variable heights, [ScrollView](scroll_view.md), Shared Selection et metadata, TableColumn types de ce couple. Duplicate id/empty id, invalid width/min/max/non fini : invalid_argument lors config/validation avant commit. External malformed layout : ignorer inconnus, garder valeurs valides bornées et diagnostics ; pas write automatique. Aucun column = table vide de cells, aucune factory ; rows empty garde headers et offset zero.

Les touches `Key::PageUp` et `Key::PageDown` sont des additions cibles en fin de l’enum portable actuel, avec traduction plateforme et tests. Le source actuel ne les définit pas. Typeahead utilise les InputEvent de texte commité ; aucun support natif complet IME n’est supposé.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/table_view.hpp` et `src/table_view.cpp`.

table_view.hpp déclare modèles columns/style/public builder ; templates Key adaptent metadata/factory au noyau non template table_view.cpp. Le .cpp porte headers, column transactions, synchronized scrolling, measure/layout/paint/input, pas un switch générique de widgets. Types de colonnes réutilisés par include, jamais dupliqués dans OutlineTableView.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `table_column_ids_layout` : order/widths restaurés, unknown/new/fixed.
- `table_resize_bounds_cancel` : min/max, callback counts, cancel sans externe rollback.
- `table_header_gesture_priority` : resize/reorder ne déclenchent pas sort.
- `table_autofit_materialized` : header+visible cells mesurés, aucune factory hors viewport.
- `table_autofit_provider_fault` : provider throw conserve ancien layout.
- `table_sort_request` : asc/desc, callback unique, app reorder keys stable.
- `table_header_body_scroll` : mêmes X/bounds, header reste Y fixe.
- `table_variable_row_resize` : wrap change anchor sans saut.
- `table_column_removed_drag` : id stale/capture récupérés.
- `table_empty_invalid_models` : zero columns/rows et validation atomique.
- `table_virtual_semantics` : snapshots sans cell callbacks.

Créer l’exemple public futur `examples/features/table_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
