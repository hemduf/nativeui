# Spacer

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Spacer` est une feuille de layout de taille fixe et sans paint. Source : `Spacer` / `SpacerComponent` dans [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) et [layout_components.inc](../include/nativeui/detail/layout_components.inc).

Pas de famille autonome nécessaire à porter de MyGo : l’équivalent se compose par `Box` et ses contraintes. Ici le composant NativeUI supplémentaire reste public.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
explicit Spacer(float height);
Spacer(float width, float height);
explicit Spacer(Size size);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto column = ui::Column{ui::Label{"A"}, ui::Spacer{0.0f, 12.0f}, ui::Label{"B"}};
```

`Spacer(float)` signifie hauteur, largeur zéro. Préserver `SpacerComponent(Size)`.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

La taille est copiée à la consommation du builder. Aucun enfant, binding, callback ou animation. Le spacer n’emploie pas d’état global et n’acquiert aucune ressource.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun focus, hit target ou capture. Il ne réagit pas au pointeur, à la molette ni au clavier et ne déclenche ni validation ni annulation. Son espace n’empêche pas le parent de recevoir les événements.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Minimum et préférée sont la taille assainie : chaque axe négatif ou non fini devient zéro. Le parent conserve son autorité sur les bounds attribués. Une largeur/hauteur zéro reste légale. `Flex{Spacer{...}}` peut servir d’espace extensible via les poids du wrapper.

- Ne pas utiliser la présence de pixels comme critère de taille intrinsèque.
- Le spacer de largeur zéro dans Row ne réserve que le gap normal entre enfants.
- Dans Column, une hauteur zéro reste un enfant logique et suit le comptage des gaps actuel.
- Les contraintes externes n’écrivent pas une nouvelle Size dans la recette.
- Si l’application désire retirer les gaps, elle utilise Collapsed/If au lieu d’un spacer invisible.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Aucun pixel peint. Une modification de taille par reconstruction demande layout ; aucun style, focus ring ni state hover. Ne pas peindre le fond du thème à la place du spacer.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`, sans enfant ; aucune entrée sémantique exposée. L’espace décoratif ne doit pas être annoncé comme texte vide ou séparateur navigable.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Construction et démontage sans effets utilisateur. Une exception d’allocation avant publication de Spec ne laisse rien enregistré. Les cycles créer/détruire et deux spacers n’ont aucune dépendance croisée.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend des types `Size` et `Component`. Couvrir toutes les surcharges, contraintes réduites et valeurs non finies ; aucune dépendance sur Skia, timers ou plateformes.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/spacer.hpp` et `src/spacer.cpp`.

Conserver les trois surcharges et `SpacerComponent` public. `spacer.cpp` contient les opérations de mesure/minimum/paint et assainissement ; `layout.hpp` réexporte le header individuel.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `spacer_height_overload` : `(12)` vaut `(0,12)`.
- `spacer_size_overload` : Size et deux floats équivalents.
- `spacer_sanitization` : axe négatif/NaN/inf ramené indépendamment à zéro.
- `spacer_in_flex` : allocation par poids sans changement du minimum.
- `spacer_no_input_semantics` : aucun focus/hit target/nœud sémantique.
- `spacer_headless` : pixels inchangés après paint.

Créer l’exemple public futur `examples/features/spacer.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
