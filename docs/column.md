# Column

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Column` empile verticalement les enfants avec padding uniforme, alignement horizontal et facteurs flex. Sources : [layout_builders.inc](../include/nativeui/detail/layout_builders.inc), [layout_components.inc](../include/nativeui/detail/layout_components.inc), `ColumnComponent`.

Référence MyGo : `ui/layout.go`, `flexLayout` et `justifyOffsets`. MyGo combine davantage de propriétés de boîte ; l’extraction NativeUI conserve ses valeurs historiques explicites.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class... Children> explicit Column(Children&&... children);
Column&& gap(float value) &&;
Column&& padding(float value) &&;
Column&& align(Align value) &&;
Column&& justify(Justify value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto column = ui::Column{ui::Label{"Titre"}, ui::Label{"Contenu"}}
    .padding(12.0f).gap(6.0f).align(ui::Align::Stretch);
```

Valeurs actuelles : gap `16`, padding `24`, Start/Start. `ColumnComponent(float gap, float padding, Align, Justify)` reste public.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

- Aucun état applicatif propre ; `Spec` et ordre des enfants sont possédés.
- Les métriques préférées et minima dérivent des enfants à chaque passe acceptée.
- Le padding appartient à cette instance et n’est pas un override de thème global.
- Les changements de données descendants demandent une mesure, sans notification utilisateur du conteneur.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Le conteneur ne prend ni focus, ni capture, ni molette. Les champs gardent leurs interactions, gestes et validation. Tab suit l’ordre de construction. Hidden/Collapsed affectent l’éligibilité via le runtime ; `Column` ne synthétise pas d’annulation supplémentaire.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Largeur préférée : maximum des largeurs enfants + deux paddings.
- Hauteur : somme des hauteurs + N−1 gaps + deux paddings ; vide conserve deux paddings.
- Les contraintes sont inset du padding ; axe Y libre pour la mesure intrinsèque.
- Le placement Y reçoit les facteurs flex, X reçoit `Align`.
- `SpaceBetween` distribue l’excédent après padding et gaps.
- Padding/gap non finis ou négatifs deviennent zéro. La zone intérieure ne devient jamais négative.
- Pas de scroll ou clipping implicite ; employer [ScrollView](scroll_view.md) si le contenu dépasse.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Aucun paint autonome. Un changement de métriques fait invalider layout et zones déplacées ; couleur d’un seul enfant demande seulement son paint. `StyleScope` hérite vers les descendants mais ne remplace pas padding/gap explicites de `Column`.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`, sans arrêt de focus et sans action. L’ordre sémantique est celui des enfants ; la justification ne le réordonne pas. Un label au-dessus d’un champ n’établit pas à lui seul une relation sémantique : employer [Field](field.md).

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Mesurer puis publier les placements par transaction de Tree. Une exception dans un enfant préserve le dernier layout cohérent ; la passe suivante recalcule l’ensemble. Le conteneur ne garde ni `ChildMetrics&`, ni invalidateur capturant un enfant après la passe.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Réutilise layout flex commun ; ne pas porter tout le moteur de boîte MyGo. Cas à couvrir : empty, un enfant collapsed, padding supérieur aux bounds, minimum supérieur à disponible, enfants flex à poids zéro, resize en cours de focus.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/column.hpp` et `src/column.cpp`.

Déplacer `ColumnComponent` et ses fonctions dans `column.cpp`, maintenir les déclarations publiques. `layout.hpp` devient un include compatible vers les headers individuels. `Column::padding(float)` demeure même si [Padding](padding.md) existe séparément.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `column_padding_intrinsic` : minimum/préférée avec vide et enfants multiples.
- `column_cross_stretch` : largeur bornée, Start/Center/End/Stretch.
- `column_grow_shrink` : allocation verticale et respect des minima.
- `column_small_bounds` : padding excessif sans largeur/hauteur négative.
- `column_focus_order` : Tab identique sous toutes justifications.
- `column_layout_fault` : mesure qui lève puis nouveau resize accepté.

Créer l’exemple public futur `examples/features/column.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
