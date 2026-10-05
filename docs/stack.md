# Stack

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Stack` superpose les enfants en ordre de composition. Source : `Stack` / `StackComponent` dans [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) et [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo utilise ses boîtes et le placement absolu de `ui/layout.go`, `layoutAbsolute`. Aucun portage de moteur CSS n’est requis pour ce composant déjà retenu.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class... Children> explicit Stack(Children&&... children);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto layered = ui::Stack{ui::Label{"Fond"}, ui::Label{"Avant-plan"}};
```

`StackComponent` demeure public. Aucun binding z-order ou callback ajouté à l’extraction.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Le builder possède les Spec dans leur ordre ; le runtime garde l’identité des enfants. Pas de registre de couches et pas de pointeur conservé vers un nœud. Réordonner des enfants suit les contrats de réconciliation, pas un tri local opaque.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Paint dans l’ordre, hit testing inverse : l’enfant supérieur éligible reçoit le pointeur. Un enfant transparent non targetable ne bloque pas les autres. Tab reste logique selon le runtime ; Stack ne crée pas de scope modal. Molette/clavier/annulation restent ceux de l’enfant ciblé.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Minimum/préférée = maximum axe par axe parmi les enfants ; vide = zéro. Tous les enfants reçoivent les bounds communs. Aucun offset implicite ni clip ; employer [Padding](padding.md), [Clip](clip.md) et wrappers de placement explicites. Bounds réduits ne produisent pas de taille négative.

- La taille préférée n’est pas la somme des tailles des couches.
- L’enfant placé en dernier n’est pas déclaré modal automatiquement.
- Les minima de chaque couche restent disponibles au parent même s’ils dépassent les bounds.
- L’overflow d’une couche reste son propre contenu, sans translation compensatoire de ses frères.
- Les couches sans pointer_targetable ne deviennent pas interceptantes du seul fait de leur z-order.
- Un enfant remplacé de même position ne reçoit pas l’ancienne capture par adresse réutilisée.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Aucun paint propre ; nettoyer les zones anciennes lors du retrait d’une couche. Ne pas confondre Stack et overlay : Stack appartient au layout ordinaire, les popups restent gérés par le service Overlay. Invalidation locale respecte les couches recouvertes.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`. L’ordre sémantique reste l’ordre logique des enfants ; un décor non sémantique ne remplace pas la lecture des contrôles au-dessous. Une couche visible superposée n’instaure pas automatiquement une modalité.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Retirer une couche sous focus/capture utilise les mêmes safe checkpoints que les autres enfants. Une erreur de paint restaure l’état painter et ne laisse pas la frame suivante dans un ordre de layers modifié. Aucune callback native détenue par Stack.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend de Tree pour paint et hit testing inverse. Tester overlays indépendants, enfants hidden/collapsed, tailles inégales et réordonnancement. Stack ne doit pas réserver de surface raster pour chaque enfant.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/stack.hpp` et `src/stack.cpp`.

Le constructeur variadique dans `stack.hpp` reste l’adaptateur. `stack.cpp` contient les méthodes publiques de `StackComponent`. Préserver l’include historique `layout.hpp` et l’ordre actuel de paint.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `stack_max_intrinsic` : mesure maximum, vide zéro.
- `stack_shared_bounds` : tous les placements identiques.
- `stack_reverse_hit` : dernier enfant targetable prioritaire.
- `stack_hidden_layer` : couche hidden n’intercepte pas le pointeur.
- `stack_remove_top` : repaint restaure couche inférieure sans artefact.
- `stack_paint_throw` : état du painter restauré après erreur.

Créer l’exemple public futur `examples/features/stack.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
