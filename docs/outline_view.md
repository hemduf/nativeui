# OutlineView<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

OutlineView affiche une hiérarchie sous forme de lignes virtualisées. Il complète [TreeView](tree_view.md) pour les gros datasets et reprend l’ancrage de [ListView](list_view.md). NativeUI ne possède aucun Outline aujourd’hui.

MyGo `ui/outline.go` : `OutlineState`, `flatten`, `Outline`, `setOpen`, `keys`, `closed`. Les clés et l’ouverture survivent aux changements de visibilité dans le listing. Le portage doit conserver identities logiques sans construire un Component par node.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
template<class Key>
OutlineView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
            Binding<std::vector<Key>> expanded);
template<class Key>
OutlineView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
            State<std::vector<Key>>& expanded);
OutlineView&& row(std::function<Spec(const TreeNode<Key>&)>) &&;
OutlineView&& row_heights(ListRowHeights) &&;
OutlineView&& selection_mode(SelectionMode) &&;
OutlineView&& on_activate(std::function<void(const Key&)>) &&;
OutlineView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
OutlineView&& style(OutlineViewStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::TreeNode<int>>> nodes{{
    {1,std::nullopt,"Racine",true,true},{2,1,"Fichier"}}};
ui::State<ui::SelectionSnapshot<int>> chosen{{}};
ui::Selection<int> selection{chosen};
ui::State<std::vector<int>> expanded{{1}};
auto outline = ui::OutlineView<int>{nodes,selection,expanded}
    .row_heights(ui::ListRowHeights{.estimate=28.0,.variable=true});
```

TreeNode modèle fixé dans [TreeView](tree_view.md), Selection/row heights dans ListView. Default Single, estimate24/variable, overscan2. Contrôleur cible `OutlineState<Key>` dans ce couple : `scroll_to_key(key,ScrollAlignment=Nearest) -> bool`, `visible_rows()`, `item_at_visible_index(index) -> optional<Key>`, `depth(key) -> optional<size_t>` ; `.state(OutlineState<Key>&)` est opt-in, vue copie son control token sûr.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Conserver graph metadata owned et flattened visible rows `{key,parent_key,depth,branch}` immutable par génération. Expansion state distinct de selection, unknown keys ignored sans rewrite. Selection order selon préordre visible. Contrôleur de scroll/focus par instance et Binding copied ; key tokens suivent node présent dans dataset même si branche fermée, contrairement à suppression réelle. Snapshots invalid Binding refusent mutation/callback synthétique.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Navigation Right/Left/recursive toggle identique TreeView ; Up/Down/Page/Home/End et typeahead ListView. Cliquer flèche n’active pas la row, doubleclick label n’ouvre pas implicitement sauf action choisie par app. Multi-selection modifiers définis ListView. Focus/active descendant caché par fermeture revient à parent, gesture canonise selection comme TreeView. Wheel ScrollView, aucune capture permanente ; teardown capture/press safe.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Validation/index de graphe à nouvelle génération : O(N log N) avec clés encodables et index trié, jusqu’à O(N²) avec clés seulement comparables par égalité, selon le contrat commun de [ListView](list_view.md) ; traversal du graphe résolu O(N). flatten O(nombre de nodes ouverts visités). Visual Components O(V+overscan+focus/capture exceptions) et scroll O(log visible count+V). Cache hauteurs par key+width, cumuls double. Expansion au-dessus conserve ancre key/inset ; si ancre devient cachée, parent visible puis prochain survivor. Collapsed metadata n’instancie pas rows. Scroll_to_key hidden retourne false, ne force pas ouvrir les ancêtres ; app commande expanded explicitement. Height/depth overflow rejeté avant commit.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

OutlineViewStyle applique indentation/chevron de TreeView et row recipe de liste avec gap/padding explicitement propres. Expansion animation peut tourner chevron 150ms, sans animation non bornée de toutes les lignes virtuelles. Width change remeasure garde ancre ; selected row paint ne reconstruit pas graphe. Timers par instance arrêtés hidden/unmount.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Group/Custom provisoire pour tree semantics absentes ; extension level/parent/action neutre nécessaire. Full logical metadata data-only ; lecture virtuelle n’appelle ni row factory, ni flatten app code. Variable bounds calculés sur shared geometry generation. Fermeture masque nodes accessibles sans faire réutiliser un token pour une autre clé ; dataset removal puis reinsert token nouveau.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer flattened generation et prefix cache avant commit ; invalid graph conserve ancienne génération. RowFactory/measure throwing : window garde ancienne matérialisation cohérente ou dirty work durable, aucun trou publié. Close sous focus/capture résout identité stable au checkpoint. Callback expansion peut réordonner ou supprimer nodes, revalider génération et garder origin keys détenues. Destruction du controller invalide operations sûres.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[TreeView](tree_view.md) model graph, [ListView](list_view.md) variable engine, [ScrollView](scroll_view.md). Source n’est pas l’arbre retenu Tree. Cas : million de nodes sans tous ouverts, cycles/orphans, profondeur grande, noeud branch sans enfant, rows disabled et invalid expansion. Aucun lazy loading réseau implicitement exécuté dans children query ; app livre snapshots ultérieurs.

Les touches `Key::PageUp` et `Key::PageDown` sont des additions cibles en fin de l’enum portable actuel, avec traduction plateforme et tests. Le source actuel ne les définit pas. Typeahead utilise les InputEvent de texte commité ; aucun support natif complet IME n’est supposé.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/outline_view.hpp` et `src/outline_view.cpp`.

outline_view.hpp expose builder/OutlineState et adaptateurs de key/factory ; outline_view.cpp porte flatten/index mapping, disclosure input, variable window, measure/layout/paint. Le noyau virtual list est partagé, pas copié. TreeNode/Selection définis une fois ; pas une classe Tree publique collisionnante.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `outline_virtual_count` : grand arbre, Comp count borné au viewport.
- `outline_expand_anchor` : ouverture/fermeture au-dessus sans saut.
- `outline_hidden_scroll_key` : false et aucune expansion implicite.
- `outline_focus_collapse` : ancêtre récupération single/multiple.
- `outline_variable_indent` : wrapping change selon profondeur et largeur.
- `outline_graph_fault` : cycles/orphans/replacement invalid sans demi-commit.
- `outline_tokens_fold_remove` : fold stable, remove/reinsert nouveau token.
- `outline_factory_reentrant_throw` : current snapshot owned et reprise sûre.
- `outline_controller_lifetime` : état détruit, callbacks/scroll stale inertes.

Créer l’exemple public futur `examples/features/outline_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
