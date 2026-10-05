# Flex

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Flex` fournit les poids grow/shrink de son enfant au conteneur Row/Column. `FlexComponent` et builder existent dans [layout_components.inc](../include/nativeui/detail/layout_components.inc) et [layout_builders.inc](../include/nativeui/detail/layout_builders.inc).

MyGo : `ui/layout.go`, `resolveFlexible`. Le contrat cible extrait le wrapper ; il conserve la répartition NativeUI et ne reproduit pas une propriété CSS complète.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> explicit Flex(Child&& child);
Flex&& grow(float value) &&;
Flex&& shrink(float value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto row = ui::Row{ui::Label{"Nom"},
    ui::Flex{ui::Label{"Description"}}.grow(1.0f).shrink(1.0f)};
```

Poids par défaut : zéro/zéro. Préserver `FlexFactors` et `FlexComponent(float grow, float shrink)`.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Les facteurs sont copiés dans le composant retenu. Aucun binding ni callback. L’enfant conserve son modèle propre ; grow/shrink ne signifie pas permission de mutation. Ne pas stocker ces poids dans une registry attachée à l’adresse de l’enfant.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Transparent au pointeur, clavier, focus et validation. Les interactions des descendants gardent leurs coordonnées relatives au placement final. Aucune capture ni shortcut. Le wrapper ne consomme pas la molette afin de préserver les scrolls ancêtres.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Minimum/préférée viennent de l’enfant et ne sont pas multipliés par les poids.
- `flex_factors()` expose les poids au parent ; le noyau d’allocation traite négatif/non fini comme zéro.
- Le parent répartit l’excédent selon grow et le déficit selon shrink en respectant les minima.
- Le wrapper place son enfant sur tout le rectangle obtenu.
- Hors Row/Column, les poids ne produisent pas de mise à l’échelle indépendante.
- Les wrappers imbriqués ne multiplient pas implicitement les poids.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide, héritage de thème transparent. Changer les facteurs par reconstruction requiert un layout ; le renderer n’a aucun style Flex. Aucune animation automatique d’extension, aucune couleur de hover.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`, descendants aplatis. La taille allouée apparaît dans leurs bounds ; aucune valeur numeric ou range sémantique n’est attachée aux facteurs grow/shrink.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Aucun abonnement à libérer ; retirer l’enfant retire son état retenu suivant Tree. Une erreur de layout ne publie pas un facteur partiellement changé. Ne pas laisser de référence à `ChildMetrics` entre deux passes.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend du contrat `flex_factors()` et de l’allocateur commun. Cas limites : poids tous zéro, inf/NaN, child vide, minima dépassant bounds, déficit après gap/padding et facteur très grand. La précision reste `float` pour compatibilité actuelle.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/flex.hpp` et `src/flex.cpp`.

Extraire les méthodes non templates de `FlexComponent` vers `flex.cpp`. Le template de construction reste un adaptateur make_spec. Préserver les includes `layout.hpp` et l’API publique, sans convertir silencieusement les poids existants en double.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `flex_intrinsic_passthrough` : poids n’altèrent pas minimum/préférée.
- `flex_weight_sanitization` : zéro/négatif/NaN/inf.
- `flex_proportional_grow` : poids 1:2, distribution attendue.
- `flex_shrink_freeze_minimum` : retrait redistribué après un minimum atteint.
- `flex_without_linear_parent` : pas d’effet propre sur Stack.
- `flex_nested_lifetime` : retirer/recréer conserve le bon modèle descendant.

Créer l’exemple public futur `examples/features/flex.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
