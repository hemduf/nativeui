# Visibility

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Visibility` hérite la disponibilité de son sous-arbre sans en changer l’identité montée. Le wrapper existe dans [component_state.hpp](../include/nativeui/component_state.hpp), classes `Visibility` et `detail::VisibilityComponent` ; les modes dans [component_base.hpp](../include/nativeui/component_base.hpp).

Il emprunte actuellement `State<VisibilityMode>` ou `State<bool>` et ne possède pas de constructeur Binding. MyGo exprime les états par ses flags d’Element ; il n’existe pas de famille documentée séparée équivalente. L’enrichissement ajoute le même accès sûr à Binding que les autres wrappers.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante à conserver :

```cpp
template<class Child> Visibility(State<VisibilityMode>& state, Child&& child);
template<class Child> Visibility(State<bool>& visible, Child&& child);
Visibility&& mode(VisibilityMode value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<bool> visible{true};
auto panel = ui::Visibility{visible, ui::Label{"Détails"}}
    .mode(ui::VisibilityMode::Collapsed);
```

Cible : surcharges identiques prenant `Binding<VisibilityMode>` et `Binding<bool>`. Les surcharges State délèguent à binding ; conserver mode uniquement utile pour la forme bool. False + mode Visible est assaini en Hidden.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

- `Visible` : sous-arbre peint/ciblable selon ses autres états.
- `Hidden` : pas de paint/input/semantics, mais espace de layout conservé.
- `Collapsed` : pas de place, paint/input/semantics.
- Les enfants restent montés : `If` fournit la suppression structurelle quand elle est souhaitée.
- Les abonnements observent le mode et invalident availability ; état égal ne change rien.
- Les enfants ne peuvent pas rétablir Visible si un ancêtre est indisponible.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Lors du masquage, Tree récupère focus/hover/capture et traite les gestures annulées. Réapparition ne synthétise pas de pointer-down ni d’activation. Le wrapper ne possède ni focus propre ni validation. Les interactions enfants restent inchangées tant que Visible et disponibles.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Wrapper transparent, mesure/minimum de l’enfant et mêmes bounds. Hidden conserve les métriques ; Collapsed est retiré de l’allocation par runtime, sans laisser un gap fantôme. Une transition mode peut invalider layout seulement si son effet sur les métriques change.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide ; l’ancienne zone visible doit être nettoyée au masquage. Les descendants classifient leurs invalidations, le wrapper ne doit pas imposer repaint global. Animations descendants suspendues quand non visibles, reprises avec état cohérent sans rejouer les actions.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

`None` pour le wrapper. Hidden/Collapsed absents du snapshot, descendants compris ; réapparition conserve les identités retenues encore vivantes. Aucune annonce de changement de valeur fictive ; publier structure/focus suivant le runtime.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Nouveau Binding doit rester inerte si son propriétaire a expiré ; supprimer les pointeurs State retenus dans la cible sans changer les signatures publiques. Retrait pendant notification respecte les checkpoints de Tree. Après erreur d’invalidation, garder le mode effectif/dirty durable afin de finir la récupération au prochain checkpoint.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend de availability recovery et [If](if.md) pour une alternative structurelle. Cas : bool false/mode Visible, ancêtres Hidden/Collapsed, sélection texte sous focus, pan capturé au masquage et state qui expire. Aucun service « visibility manager » indépendant.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/visibility.hpp` et `src/visibility.cpp`.

Extraire builder/runtime dans visibility.hpp/cpp ; garder component_state.hpp en façade compatible. Runtime non template de disponibilité et Binding possédé dans le .cpp. Conserver les enums et les `detail` sans les transformer en nouveaux services publics.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `visibility_modes_space` : Hidden garde taille, Collapsed la retire.
- `visibility_bool_sanitize` : false+Visible donne Hidden.
- `visibility_inherited` : enfant Visible ne rétablit pas ancêtre Hidden.
- `visibility_capture_focus` : masking annule capture et récupère focus.
- `visibility_binding_expired` : expiration sans State* périmé.
- `visibility_fault_reconcile` : invalider qui lève puis état utilisable.

Créer l’exemple public futur `examples/features/visibility.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
