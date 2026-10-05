# Scroll

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Scroll` fournit viewport, clip et translation de ScrollState, sans les gestes/barres de [ScrollView](scroll_view.md). Source : [layout_builders.inc](../include/nativeui/detail/layout_builders.inc), [layout_components.inc](../include/nativeui/detail/layout_components.inc), `ScrollComponent`, et [scroll_view.inc](../include/nativeui/detail/scroll_view.inc), `RetainedScrollComponent` utilisé par le builder.

MyGo `ui/scroll.go` : `ScrollState`, `TrackScroll`, `reveal`. Préserver la séparation primitive de layout / conteneur interactif.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> Scroll(ScrollState& state, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::ScrollState offset{ui::ScrollAxis::Vertical};
auto scroll = ui::Scroll{offset, ui::Column{ui::Label{"Contenu"}}};
```

`ScrollState(ScrollAxis=Vertical)`, `set_offset(Point)`, `scroll_by(Point)`, `observe(std::function<void(Point)>)`, getters offset/viewport/content/max et lifetime token restent publics. Aucun Binding<Point> ne les remplace.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Contrôleur externe propriétaire de l’offset et de ses listeners, emprunté avec LifetimeToken. Un contrôleur distinct par viewport indépendant. Axis fixé à construction ; offset assaini et limité aux axes permis. L’observation réentrante garde la discipline actuelle de ScrollState.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun focus/capture propre et aucune promesse de gestures de ScrollView. Déplacement par contrôleur. Les coordonnées descendants tiennent compte de la translation. Clavier, molette, validation et annulation restent routés normalement vers les autres composants.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Mesurer sans borne sur l’axe scrollable, borner l’axe orthogonal.
- Viewport issu des bounds ; contenu conserve la taille intrinsèque.
- Publier tailles et offset borné comme état cohérent.
- Translation `−offset` puis intersection du clip viewport/ancêtres.
- Contenu/viewport vide et shrink de contenu recalculent max_offset.
- Conserver Point/Size float et les protections de lifetime.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide ; offset demande repaint du viewport sans refaire des intrinsèques inchangées. Taille de contenu demande layout et nouveaux offsets maximum. Scroll ne dessine aucune barre et ne crée aucune animation autonome.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle de layout `None`, descendants conservés. Bounds accessibles transformés une fois en logique. Aucun faux Slider représentant l’offset. Un descendant retiré ne reste pas vivant uniquement pour la lecture sémantique.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Le token invalide tout accès après destruction du state, même dans un callback observe. Désabonner avant démontage. Erreur listener : restaurer dispatcher/guards et préserver les écritures acceptées non commencées ; prochain scroll reste possible.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend du ScrollState commun déclaré une seule fois, layout et clipping. Cas : non fini/négatif, Both, contenu retiré, state détruit sous observer et contrôleurs séparés. Aucun service natif supplémentaire.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/scroll.hpp` et `src/scroll.cpp`.

Conserver `ScrollComponent` public même si le builder utilise le runtime interne sûr. `scroll.cpp` porte le noyau retenu. `scroll.hpp` donne accès à ScrollState/ScrollAxis ; `scroll_view.hpp` réutilise les mêmes types et `layout.hpp` reste compatible.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `scroll_clamp_axis` : axes, limites, négatif/NaN/inf.
- `scroll_transform_clip` : géométrie, pixels et hit testing.
- `scroll_content_shrink` : offset reborner.
- `scroll_state_lifetime` : destruction pendant observe sans UAF.
- `scroll_observer_throw` : reprise après exception.
- Préserver `scroll_layout_tests` et contrats de `ScrollState`.

Créer l’exemple public futur `examples/features/scroll.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
