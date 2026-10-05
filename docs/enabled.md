# Enabled

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Enabled` restreint l’éligibilité d’interaction des descendants en conservant leur présence et taille. Source : [component_state.hpp](../include/nativeui/component_state.hpp), `Enabled`, `EnabledComponent`.

La signature actuelle ne prend que State<bool>. MyGo porte la capacité via Disabled sur les éléments ; aucune famille autonome à copier. La cible ajoute Binding et extraction sans changer les règles d’héritage.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API actuelle :

```cpp
template<class Child> Enabled(State<bool>& state, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<bool> allowed{true};
auto controls = ui::Enabled{allowed, ui::Button{"Appliquer", []{}}};
```

API cible supplémentaire : `template<class Child> Enabled(Binding<bool>, Child&&)`. State délègue au Binding ; aucun callback on_enabled propre, l’application observe son état.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Enabled effectif = combinaison restrictive des ancêtres/descendant. Un parent false ne peut pas être annulé par un enfant true. Le modèle reste inchangé au disable. Nouvelle subscription Binding RAII par instance ; aucun listener global.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

False empêche activation et mutation, retire les descendants inéligibles du ciblage focus selon Tree. Une gesture en cours est terminée par availability recovery ; aucune activation au re-enable. Le wrapper n’est ni focusable ni targetable autonome. Ne pas transformer disable en ReadOnly : navigation/copie des champs suit leur contrat disabled.

- Réactiver un widget ne rétablit pas automatiquement une touche ou un pointeur resté physiquement pressé.
- Un pointer-up ancien après disable ne doit pas activer le contrôle réactivé.
- Le retrait du focus n’autorise pas le wrapper à lancer la validation métier d’un formulaire.
- Les nœuds disabled gardent leurs données de lecture, même si leurs hit targets sont exclus.
- Un raccourci routé à un autre scope suit ses propres contraintes ; le wrapper n’altère pas une table globale de raccourcis.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Minimum/préférée et placement transparents ; disabled garde son espace de layout. Un style enfant peut modifier ses métriques selon états, mais le wrapper ne réduit jamais la taille seul. Les bornes de hit test sont celles du descendant, avec disponibilité effective.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide. Les descendants résolvent l’apparence disabled ; aucune opacité uniforme appliquée aveuglément au sous-arbre. Paint ou layout seulement si la recette du descendant change effectivement ; pas d’invalidation full viewport du wrapper.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None`. Descendants restent présents lorsque pertinents avec `enabled=false` et rejettent actions d’activation/mutation. Ne pas retirer automatiquement leurs noms/valeurs du snapshot au seul motif disabled.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Lors du disable réentrant pendant une activation, finir l’invocation commencée au plus une fois, puis récupérer les gestes au checkpoint. Cible Binding invalide : état inerte sûr, sans dereference State*. Démontage déconnecte availability invalidator avant perte du propriétaire.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Runtime de disponibilité, [ReadOnly](read_only.md), [Visibility](visibility.md) et styles descendants. Cas : disable sur pointer-down, clavier enfoncé, listener modifiant enabled récursivement, deux instances avec bindings séparés. Aucun appel système pour griser un contrôle.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/enabled.hpp` et `src/enabled.cpp`.

enabled.hpp/cpp portent le wrapper et le noyau d’availability ; component_state.hpp réexporte pour compatibilité. Aucun changement de signatures existantes State& et aucune relocalisation de l’état dans Theme.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `enabled_inheritance` : restriction parent non annulable.
- `enabled_preserves_layout` : mêmes bounds et modèle.
- `enabled_armed_button` : disable avant up ne déclenche pas click.
- `enabled_semantics` : valeur lisible, actions mutantes refusées.
- `enabled_binding_lifetime` : state expire sans UAF.
- `enabled_reentrant_notify` : observer réentrant/throw puis reprise.

Créer l’exemple public futur `examples/features/enabled.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
