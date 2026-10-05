# TreeView<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

TreeView compose une hiérarchie de taille modérée avec disclosures, choix par clé et navigation parent/enfant. NativeUI `Tree` est le runtime retenu, pas un widget de navigation ; le nouveau nom évite la collision. Fondations : [dynamic.hpp](../include/nativeui/dynamic.hpp) et [component_tree.hpp](../include/nativeui/component_tree.hpp).

MyGo `ui/tree.go` : `Tree`, `TreeItem`. MyGo construit les items imbriqués ouverts et navigue par ordre visible. La cible définit un snapshot de données stable commun à [OutlineView](outline_view.md), qui apporte la virtualisation.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

Modèle commun cible unique dans `collection_model.hpp` :

```cpp
template<class Key> struct TreeNode {
    Key key;
    std::optional<Key> parent;
    std::string label;
    bool enabled=true;
    bool branch=false;
    bool operator==(const TreeNode&) const=default;
};
template<class Key>
TreeView(Binding<std::vector<TreeNode<Key>>> nodes, Selection<Key>& selection,
         Binding<std::vector<Key>> expanded);
template<class Key>
TreeView(State<std::vector<TreeNode<Key>>>& nodes, Selection<Key>& selection,
         State<std::vector<Key>>& expanded);
TreeView&& row(std::function<Spec(const TreeNode<Key>&)>) &&;
TreeView&& selection_mode(SelectionMode) &&;
TreeView&& on_activate(std::function<void(const Key&)>) &&;
TreeView&& on_expansion_change(std::function<void(const std::vector<Key>&)>) &&;
TreeView&& style(TreeViewStyle) &&;
Spec spec() &&;
```

Exemple futur :

```cpp
ui::State<std::vector<ui::TreeNode<std::string>>> nodes{{
    {"src",std::nullopt,"Sources",true,true}, {"main","src","main.cpp",true,false}}};
ui::State<ui::SelectionSnapshot<std::string>> chosen{{}};
ui::Selection<std::string> selection{chosen};
ui::State<std::vector<std::string>> expanded{{"src"}};
auto tree = ui::TreeView<std::string>{nodes,selection,expanded};
```

Rows default Label{node.label}. Defaults Single, indentation 16 DIP, min row 24 DIP ; callbacks/factory possédés. Dataset flat parent nullable ; ordre des siblings = ordre dans snapshot. `branch=true` permet un dossier vide expandable ; un parent possédant des enfants est une branche même si flag false.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Nodes snapshot copié/validé ; key unique, parent référant une clé présente ou nullopt. Expansion externe ordered par ordre dataset et keys uniques ; unknown keys ignorées visuellement sans rewrite automatique. Selection consommée par binding copié, identités par key. Un toggle produit expanded canonique, callback après write effectif ; updates externes ne déclenchent pas gesture callback. Binding invalid de dataset = dernière génération lisible, selection/expanded invalid interdisent leur mutation.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Click chevron toggle expansion sans activation ; click row choisit ; Enter/double click active via callback. Up/Down/Home/End dans ordre visible ; Right ouvre puis sur branche ouverte va au premier child ; Left ferme puis vers parent. Modifiers selection selon ListView. Alt pointer toggle récursif ; clavier expansion récursive via policy portable (Option macOS/Shift ailleurs). Échap annule press/buffer, jamais rollback d’un expanded externe. Focus roving sur une row, Tab sort/entre l’ensemble ; child controls gardent leur focus si configurés.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Traversal préordre des branches ouvertes, indentation depth*16 DIP et chevron 16 DIP. TreeView matérialise les lignes visibles logiquement (toutes les branches ouvertes), sans contrat O(viewport). Utiliser Outline pour grands arbres. Mesurer row naturelle et max width + indentation ; ScrollView interne pour overflow. Depth multiplication checked, rects finite/nonnegative. Fenêtre réduite wrap/clips contenu suivant row factory.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

TreeViewStyle contient padding, indentation, chevron et row styles active/selected/hover/focus. Chevron indique expanded même sans animation. Collapsed descendants exclus de layout/input ; contenu state-by-key conservé lorsque encore monté selon retained policy choisie : ici fermeture démonte les descendants visuels, modèles externalisés. Ne pas conserver Components fermés uniquement pour prétendre virtualiser.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Tree/TreeItem sont absents des rôles actuels : cible Group/Custom avec name, selected et expanded, actions Focus/Select/Expand/Collapse/Activate éligibles. Level/parent/index-in-set requièrent extension neutre explicitement future, pas du native pointer. Snapshots de lecture possédés ; ordre préordre visible et IDs nouveaux après unmount.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer relation parents, détection de cycles et ordre visible avant commit. Initial invalid dataset : invalid_argument à consommation ; remplacement invalide conserve dernière génération acceptée avec diagnostic, prochaine génération valide récupère. Fermer branche qui contient active/focus : active revient à ancêtre ; gesture single sélectionne ancêtre, multiple retire descendants désormais cachés de sa sélection canonique. Une fermeture externe ne réécrit pas Selection automatiquement, mais le focus runtime récupère. Callbacks reentrant sont revalidés par key/generation.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[ListView](list_view.md) modèles et keyboard policy, ScrollView, dynamic lifecycle. Cycles, duplicate key, parent absent : invalides ; forêt multi-roots et vide valides. Nouveau subtree pendant callback, branch supprimée, key retirée/réinsérée et expansion inconnue testés. Aucune child callback récursive demandée depuis snapshot accessible.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/tree_view.hpp` et `src/tree_view.cpp`.

tree_view.hpp templates adaptent TreeNode/Selection, row factory et equality des clés ; tree_view.cpp porte graphe valide, parcours, disclosure, input, mesures et paint du noyau non template. Réutiliser collection_model.hpp, jamais redéfinir ui::Tree. Toutes classes Component publiques nouvellement exposées restent déclarées dans ce header.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `tree_parent_child_keyboard` : Right/Left, préordre et skip disabled.
- `tree_forest_empty_branch` : forêt, vide, branch empty.
- `tree_key_identity` : reorder siblings maintient modèles/focus par clé.
- `tree_cycle_orphan_duplicate` : reject initial, ancienne génération gardée ensuite.
- `tree_recursive_expansion` : modifiers sans boucle de cycle.
- `tree_collapse_focus_selection` : gesture/external distinction et focus ancestor.
- `tree_dataset_during_input` : suppression/replacement reentrant safe.
- `tree_expansion_throw` : callback unique, guards restaurés.

Créer l’exemple public futur `examples/features/tree_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
