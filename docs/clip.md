# Clip

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Clip` limite paint et hit testing au rectangle alloué tout en conservant la mesure intrinsèque de l’enfant. Sources : `Clip` et `ClipComponent` dans [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) / [layout_components.inc](../include/nativeui/detail/layout_components.inc).

MyGo exprime cette capacité par `Element.Clip` et l’état de boîte dans `ui/layout.go`. Il ne faut pas transformer ce wrapper en scroll ni en masque raster spécifique au backend.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> explicit Clip(Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto clipped = ui::Clip{ui::Row{ui::Label{"Texte long"}}};
```

Conserver le `ClipComponent` public et ses hooks de clipping. Aucun radius, état ou callback n’est ajouté dans cette extraction.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Un unique `Spec` possédé ; aucune observation et aucun modèle externe. Le clip effectif est l’intersection de ce rectangle avec les clips ancêtres. Son identité reste celle d’un nœud retenu indépendant, sans bitmap offscreen mutable.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Aucun focus propre ; les enfants peuvent être focusables.
- Hit testing ne cible jamais une portion située hors de l’intersection effective.
- Un enfant capturé continue de recevoir les événements selon le contrat de capture Tree ; le clip ne casse pas sa récupération.
- Molette et touches circulent vers l’enfant/parent selon les règles normales.
- Échap n’a aucun sens propre au clip.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Transmettre les contraintes parent et rapporter minimum/préférée du seul enfant. Placer l’enfant sur les bounds du wrapper ; le clipping ne change pas ses métriques de contenu. Bounds vides impliquent une zone peinte/ciblable vide, sans modifier le modèle. Ne pas arrondir aux pixels en layout.

- Un clip vide ne supprime pas le modèle enfant et ne crée pas une visibilité Collapsed implicite.
- Le conteneur ne promet pas un clip arrondi ; ce besoin doit être fourni par une API distincte.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Le wrapper ne peint pas. Mettre à jour la région d’invalidation quand bounds/clip changent ; nettoyer l’ancienne région visible. Le scoping du clip painter est RAII et restauré même si le paint de l’enfant lève. Les frères ne doivent jamais hériter du clip par accident.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`. Les bounds accessibles restent logiques selon le runtime ; le clip ne crée aucune action. Les descendants non éligibles à cause de disponibilité sont absents, sans inventer une règle « hors viewport = détruit » pour les objets accessibles.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Le runtime possède la pile de clips ; `Clip` n’en conserve aucune référence au-delà de paint. Démontage sous capture se traite par Tree. Après exception de paint, le frère suivant et la frame suivante retrouvent un clip identique à leur contrat normal.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend uniquement des services de layout/paint/hit testing. Couvrir clips imbriqués, transformations par scroll, recouvrement Stack, enfant overflow et rectangle nul. Ne pas porter de primitive `SkCanvas` dans l’API.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/clip.hpp` et `src/clip.cpp`.

Le constructeur template reste dans `clip.hpp` ; `ClipComponent` déclare publiquement sa capacité. `clip.cpp` porte mesure, placement, hooks de clip et no-op paint. Préserver `layout.hpp` et éviter une seconde implémentation de la pile de clipping.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `clip_paint_intersection` : deux clips imbriqués, pixel hors intersection inchangé.
- `clip_hit_rejection` : enfant overflow ne reçoit pas pointer-down hors clip.
- `clip_capture_teardown` : retirer sous capture puis aucun accès périmé.
- `clip_restore_after_throw` : paint qui lève, frère et frame suivante corrects.
- `clip_scroll_transform` : coordonnées logiques après offset et échelle.
- Conserver les régressions de `t075_scoped_clipping_tests`.

Créer l’exemple public futur `examples/features/clip.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
