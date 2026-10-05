# Switch<T>

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Switch<T>` sélectionne un sous-arbre par égalité de valeur ; il ne représente pas un interrupteur. Source : [dynamic.hpp](../include/nativeui/dynamic.hpp), `Switch<T>`, `SwitchBranch<T>`, `SwitchComponent<T>`.

MyGo utilise un switch Go lors de composition ; aucun widget autonome à porter. Conserver ce nom pour composition conditionnelle et [Toggle](toggle.md) pour le switch visuel.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante à conserver intégralement :

```cpp
explicit Switch(Binding<T> state);
explicit Switch(State<T>& state);
template<class Child> Switch& when(T value, Child&& child) &;
template<class Child> Switch&& when(T value, Child&& child) &&;
template<class Child> Switch& otherwise(Child&& child) &;
template<class Child> Switch&& otherwise(Child&& child) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<int> page{0};
auto view = ui::Switch<int>{page}.when(0, ui::Label{"Accueil"})
    .when(1, ui::Label{"Options"}).otherwise(ui::Label{"Inconnu"});
```

Préserver les guides de déduction implicites de State/Binding, les overloads lvalue/rvalue et les contraintes d’égalité de T. Première branche égale gagne, même si plusieurs `.when` déclarent la même valeur ; `otherwise` remplace la fallback précédente.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Recettes de branches et valeurs possédées. Binding observé ; clés des branches conservent actuellement `switch:<index>`, fallback `switch:fallback`. Passer d’une valeur à une autre sélectionnant la même branche conserve celle-ci. Branches non actives ne sont pas montées.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun input/focus propre. Branch change sous callback est deferred à Tree ; capture/focus de la branche retirée sont récupérés. Pas d’Échap spécial, gesture de commutation ou événement on_select. L’application observe son state si elle veut une notification métier.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Mesure/préférée de l’unique branche active ; aucun match sans fallback = zéro. Les branches inactives ne contribuent ni taille ni gaps. Tous placements utilisent bounds du host ; pas de transition de layout entre deux branches implicite.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Host paint vide. Une branche inchangée n’est pas démontée pour une notification de state équivalente. Nettoyer les anciens pixels au remplacement. Toute animation optionnelle devrait être un composant explicite ; aucune couche globale de transitions ajoutée ici.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

`None` du host, descendants de la branche active seulement. Réinsertion d’une branche démontée recrée des IDs retenus ; ne pas recycler une identity sémantique defunct. La fallback ne reçoit aucun rôle spécial.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Adapter T vers choix de branche dans le header, puis transmettre index/clé et Spec au noyau type-erased. Une comparaison/factory qui lève ne laisse pas un host à moitié commuté. Conserver les règles de quarantaine/retry explicite du runtime dynamique ; un callback commencé ne se rejoue pas.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend des sources dynamiques et de la réconciliation Tree. T doit rester copiable selon les usages existants et comparable ; ne pas exiger hash ou enum seulement. Cas : aucun match, duplicates, fallback remplacée, state réentrant, equality throwing et switch vide.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/switch.hpp` et `src/switch.cpp`.

switch.hpp garde uniquement adaptateurs de comparaison typed et constructeurs templates ; switch.cpp héberge DynamicHost non template, observation type-erased et publication de branches. Ne pas remplacer cela par instanciations prédéfinies de T. Preserve dynamic.hpp et les signatures publiques historiques.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `switch_first_equal` : première branche duplicate gagne.
- `switch_fallback_replace` : dernière otherwise choisie, none vide.
- `switch_lvalue_overloads` : compiler when/otherwise sur builder nommé et temporaire.
- `switch_custom_key_type` : type utilisateur non enum/non hashable.
- `switch_same_branch_identity` : pas de remount pour même branche.
- `switch_compare_factory_fault` : récupération après equality/factory throw.

Créer l’exemple public futur `examples/features/switch.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
