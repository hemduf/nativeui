# Padding

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Padding` réserve une marge intérieure uniforme autour d’un enfant. Sources : `Padding` / `PaddingComponent` dans [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) et [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo `ui/layout.go`, `padX`, `padY`, `contentX`, `contentY`, traite padding et bordure séparément. NativeUI conserve ici le wrapper uniforme, sans importer la boîte entière.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> Padding(float padding, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto padded = ui::Padding{12.0f, ui::Label{"Contenu"}};
```

Préserver `PaddingComponent(float)` et la précision `float`. Une variante asymétrique ne fait pas partie de l’extraction spécifiée.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Padding et Spec enfant sont possédés. Aucun état externe, notification ou callback propre. La taille intérieure se calcule à partir des bounds de la passe, sans cache global de dimensions.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun arrêt de focus ni activation. La marge ne devient pas hit target ; seuls les descendants éligibles réagissent. Le clavier et la molette gardent leur routage. Validation/annulation appartiennent au contrôle enfant.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Préférée/minimum : métriques enfant + deux paddings sur chaque axe. Contraintes enfant inset ; placement à `(x+p,y+p)` avec `max(0,w−2p)` et `max(0,h−2p)`. Padding négatif ou non fini devient zéro. Un padding supérieur aux bounds conserve un rectangle intérieur vide et fini.

- Le code actuel emploie `std::max(0.0f,padding)` ; la garantie finite ci-dessus est une exigence cible de robustesse, pas la preuve d’un traitement actuel de +inf.
- Aucune bordure n’est ajoutée à la mesure ; padding et stroke sont distincts.
- Mesurer un child à largeur réduite peut augmenter sa hauteur par wrapping.
- Le parent reçoit les métriques recalculées, pas une taille issue d’un ancien viewport.
- L’inset des contraintes doit conserver min<=max après assainissement.
- Le changement de facteur d’échelle reste une conversion backend, pas une multiplication du padding dans la recette.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide, sans couleur de fond. Ne pas peindre des marges en couleur de surface. Changement de padding par reconstruction demande layout ; changement de paint enfant ne redemande pas intrinsèques si ses métriques sont identiques.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`, enfant aplati. Les bounds accessibles du contrôle incluent uniquement ses bounds de layout, pas toute la marge du wrapper. Le padding ne modifie pas le nom/description du contrôle.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Ne pas conserver de référence aux contraintes entre passes. Les exceptions de mesure/layout restent récupérables via transaction Tree. Démontage ne crée aucune notification de taille applicative.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend des types de géométrie et de l’inset des contraintes. Cas limites : vide, padding excessif, clipping enfant, scroll transform, valeurs non finies. `Column::padding` reste distinct et compatible.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/padding.hpp` et `src/padding.cpp`.

Extraire `PaddingComponent` dans `padding.cpp` avec déclaration publique dans `padding.hpp`. Le constructeur enfant template reste header. Préserver `layout.hpp` et le sens uniforme historique.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `padding_intrinsic` : ajout de 2p aux minima/préférées.
- `padding_inset_bounds` : placement intérieur exact.
- `padding_excess` : petites bounds, largeur/hauteur jamais négatives.
- `padding_nonfinite` : NaN/inf/négatif assainis.
- `padding_hit_geometry` : marge ne déclenche pas l’enfant.
- `padding_fault_recovery` : erreur enfant puis prochaine passe correcte.

Créer l’exemple public futur `examples/features/padding.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
