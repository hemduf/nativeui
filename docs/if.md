# If

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`If` insère ou retire structurellement un enfant selon bool. Source : [dynamic.hpp](../include/nativeui/dynamic.hpp), `If`, `detail::IfComponent`, `DynamicChildrenSource`.

Pas de famille MyGo autonome : MyGo reconstruit impérativement sous condition. Le composant NativeUI diffère de Visibility : false démonte l’enfant, true construit une nouvelle instance retenue.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> If(Binding<bool> state, Child&& child);
template<class Child> If(State<bool>& state, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<bool> detailed{false};
auto branch = ui::If{detailed, ui::Label{"Détails"}};
```

Préserver ces surcharges et la clé logique interne `if:true`. Le Spec enfant est gardé comme recette possédée, pas comme instance de Component réutilisée après démontage.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Binding bool copié, recette de Spec partagée immutable. Observe marque la structure dirty ; Tree réconcilie au checkpoint sûr. True répété conserve l’identité courante ; false retire les nodes montés. État local d’un enfant détruit n’est pas retenu : externaliser si nécessaire.

- La recette n’est pas exécutée pour une branche false initiale.
- Retourner true après démontage recrée les objets Component, pas leurs modèles externes.
- Une suite true/false avant checkpoint adopte la dernière valeur autoritaire sans fabriquer d’action utilisateur.
- L’application qui veut conserver l’instance montante choisit Visibility au lieu de If.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun focus/input propre. Retrait sous focus/capture utilise les mécanismes Tree avant disparition. Au retour true, aucune activation ni gesture synthétique. Tab suit seulement les descendants présents. L’annulation de geste relève du runtime, pas d’un callback on_false.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Vide = Size zéro ; true rapporte les métriques du seul enfant et place sur bounds. Parent reçoit invalidation structure/layout lors du changement. Un hidden enfant reste monté mais If false n’a pas de métrique fantôme.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide. Nettoyer ancienne région au retrait. La construction/reconciliation ne doit pas déclencher repaint pendant paint en cours ; exécution au safe checkpoint. Le même bool effectif ne remonte pas inutilement le sous-arbre.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None`. Enfant false absent du snapshot ; retour true reçoit les IDs de nouveaux nodes, les anciens proxies restent defunct. L’ordre sémantique des frères est préservé.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Une exception de factory/mount laisse l’ancienne structure cohérente ou une récupération durable du runtime, sans moitié de child publié. Ne pas répéter automatiquement un mount callback déjà commencé. Une bascule supplémentaire après failure doit pouvoir récupérer, selon la quarantaine existante de composition dynamique.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Réutiliser dynamic reconciliation, lifecycle transactions, availability recovery et [Visibility](visibility.md). Cas : false initial, bascules rapides avant checkpoint, retrait réentrant dans callback enfant, factory throwing, owner détruit avant invalidation deferred.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/if.hpp` et `src/if.cpp`.

if.hpp expose builder et adaptateur enfant ; if.cpp porte le host dynamique bool et sa source de clés/children. Préserver include dynamic.hpp et les types utilitaires partagés une seule fois. Ne pas réimplémenter la transaction Tree dans ce composant.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `if_initial_false` : aucun mount/paint/focus enfant.
- `if_toggle_lifecycle` : mount/unmount exact et nouvel ID à réinsertion.
- `if_reentrant_remove` : retrait sous input, pas UAF.
- `if_factory_fault` : pas de child partiellement publié, bascule suivante récupérable.
- `if_multiple_pending` : état final au checkpoint sans action doublée.
- Conserver `dynamic_composition_tests` et leurs scénarios de récupération.

Créer l’exemple public futur `examples/features/if.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
