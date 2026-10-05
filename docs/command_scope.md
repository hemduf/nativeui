# CommandScope

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`CommandScope` traite des commandes portables remontant des descendants, sans intercepter l’input ordinaire. Source : [command.hpp](../include/nativeui/command.hpp), `CommandCallback`, `CommandScope`, `CommandScopeComponent`.

MyGo `ui/scope.go`, `overlayShortcut`, couvre certaines commandes de scopes. NativeUI possède déjà son routage portable ; aucune dépendance Router ni accélérateur système à ajouter.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
using CommandCallback=std::function<EventResult(Command)>;
template<class Child> CommandScope(CommandCallback callback, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto scope = ui::CommandScope{
    [](ui::Command){return ui::EventResult::Ignored;},
    ui::Button{"Action", []{}}};
```

Le callback est possédé ; il retourne Handled/Ignored. Conserver CommandScopeComponent public et `input(const InputEvent&, InputContext&)`.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Le callback et le Spec enfant sont détenus par l’instance, sans binding supplémentaire. Application conserve tout modèle capturé avec durée de vie adéquate. Aucune map mutable process-global de shortcuts ; un scope ne modifie pas les commandes d’un autre UI.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- InputType::Command uniquement ; Command::None ignoré.
- Sans callback : Ignored.
- Callback Handled arrête la propagation ; Ignored la continue suivant Tree.
- Les inputs pointeur/key ordinaires passent aux descendants.
- Pas de focus propre ni capture, validation/annulation sont des commandes seulement si le routage existant les fournit.
- Un scope intérieur peut traiter la commande avant l’extérieur.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Minimum/préférée du premier enfant, mêmes bounds ; le wrapper ne crée ni padding ni focus target. Son routage ne dépend pas de sa surface peinte ; les descendants gardent leurs coordonnées.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide, pas d’invalidation pour un callback qui ignore une commande. Si une commande modifie State, le widget concerné invalide son état. Aucune notification globale « command executed » implicite.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`. Les actions accessibles vont au contrôle qui les annonce ; CommandScope ne les remplace pas par des key events. Une commande applicative ne reçoit pas automatiquement une action sémantique supplémentaire.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Un callback peut demander le retrait du scope via reconciliation, sans accès au callback déplacé après invocation. Une exception doit restaurer la pile de dispatch puis être propagée/convertie à la frontière appropriée, sans retry. Déconnecter les tâches deferred qui capturent l’owner.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend du Command enum, EventResult et bubbling Tree. Cas : callback vide, None, nested scopes, retirer le child/scope durant commande, callback qui lève, fermeture UI demandée. Ne pas retenir InputContext ni activer un owner global.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/command_scope.hpp` et `src/command_scope.cpp`.

command_scope.hpp/cpp gardent déclarations publiques et handler portable non template ; command.hpp réexporte. Aucun changement de CommandCallback, types ou ownership de retour EventResult.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `command_scope_none` : ignoré sans invocation.
- `command_scope_non_command` : pointer/key passthrough.
- `command_scope_bubble` : inner Ignored vers outer, Handled arrête.
- `command_scope_owned_callback` : closure vit assez longtemps.
- `command_scope_reentrant_remove` : demande suppression sûre.
- `command_scope_throw` : commande suivante dispatchable, invocation non rejouée.

Créer l’exemple public futur `examples/features/command_scope.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
