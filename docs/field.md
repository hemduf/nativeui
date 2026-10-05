# Field

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Field associe label, contrôle, aide et message d’erreur ; en Form son label participe à l’alignement commun. NativeUI ne possède pas de Field actuel ; composer Label/TextInput ne relie pas automatiquement leurs sémantiques.

MyGo `ui/form.go` : `Field`, `fieldControl`, `focusIn`, `namesItself`, `Description`, `Error`. La cible reprend le premier contrôle éligible par défaut, avec possibilité de cible explicite pour les compositions ambiguës.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
template<class Child> Field(std::string label, Child&& control);
Field&& description(std::string text) &&;
Field&& description(Binding<std::string> text) &&;
Field&& error(std::string text) &&;
Field&& error(Binding<std::string> text) &&;
Field&& target(std::string descendant_key) &&;
Field&& required(bool value=true) &&;
Field&& style(FieldStyle) &&;
Spec spec() &&;
```

Surcharges State<string>& pour description/error délèguent au Binding. Exemple futur :

```cpp
ui::State<std::string> email{""};
ui::State<std::string> error{""};
auto field = ui::Field{"Email",ui::TextInput{"",email}}
    .description("Pour le reçu").error(error.binding()).required();
```

`target` nomme une clé retenue applicative stable, jamais une adresse ou index ; en son absence sélectionner le premier descendant focusable éligible ou groupe de contrôle reconnu.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Field possède label/Spec et chaînes value-form ; Binding forms restent observées. Il ne possède ni valeur du contrôle ni règle de validation métier. Required est une indication sémantique/visuelle, pas un validateur. Error vide = absence ; changement de string invalide métriques et description seulement si effectif changé.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Click label focus la cible éligible. Pour checkbox/toggle/radio, déléguer Activate/Toggle/Select à la cible uniquement quand l’action est annoncée et permise ; ne pas synthétiser des events pointeur. Aucun activation du TextInput en dehors du focus. Pas d’arrêt Tab du label décoratif. Échap/Enter/molette restent au contrôle. Disabled/read-only de cible respectés au moment de la demande.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Hors Form, stack label/control/descriptions ; en Form, label à gauche selon contexte et contrôle à droite. Baseline hook cible du Form ; sinon première ligne label centrée sur la hauteur du contrôle. Description et erreur sous contrôle, pas sous la largeur combinée. Long labels/errors wrap selon width disponible et n’écrasent pas les minima du contrôle.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

FieldStyle contient gaps, label/error/help typographies et couleurs. Erreur s’exprime par texte et indication visuelle, pas rouge seul ; décor d’erreur n’écrase pas les styles propres du widget. Disparition d’erreur demande layout et effacement ancien texte. Pas de blink ou alerte globale implicite.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Nom composable : si contrôle possède déjà name explicite, le garder et donner label au groupe ; sinon fournir le label comme name. Ajouter aide+erreur possédées à description sans remplacer description explicite. Nécessite enrichissement scoped de sémantique dans Tree ; aucune relation labelled-by native actuelle revendiquée. Required/error riche peut nécessiter champs neutres futurs ; fallback description texte.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Résoudre target par identité stable au moment du click, pas conserver Component*. Cible retirée/hidden/disabled : label click no-op déterministe, prochain layout peut résoudre une autre cible par défaut. La disparition d’un Binding aide/erreur devient texte absent sans UAF. Subscription relâchée avant teardown.

Comme Binding ne notifie pas la destruction de State, appliquer la politique « aide/erreur absente après expiration » à la prochaine lecture sûre de measure/input/paint, puis invalider si l’effet affiché change. Ne jamais déréférencer State ou promettre une notification instantanée de destruction.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[Form](form.md), [Fieldset](fieldset.md), focus/action sémantique UI-thread et overrides scoped cibles. Field vide ou sans focusable reste présentation légale ; target explicite manquant n’active pas un autre contrôle par surprise. Aucun parsing/validation automatique de champ.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/field.hpp` et `src/field.cpp`.

field.hpp déclare style/builder/overloads texte ; field.cpp porte résolution de cible, assistance sémantique, baseline/layout/input label et paint des textes. Field reste composant distinct de Form et Fieldset ; pas d’implémentation entièrement inline dans un helper.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `field_label_focus` : label cible premier contrôle ou clé explicite.
- `field_checkbox_label_action` : une action permise, zéro si disabled/read-only.
- `field_existing_name` : préserver name explicite et composer description.
- `field_error_wrap` : erreur longue, vide puis disparition sans artefact.
- `field_target_removed` : target absent no-op et aucune adresse stale.
- `field_binding_expired` : aides safe après expiration.
- `field_reentrant_action_throw` : pas de seconde activation automatique.

Créer l’exemple public futur `examples/features/field.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
