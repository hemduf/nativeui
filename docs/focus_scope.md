# FocusScope

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`FocusScope` délimite seulement le domaine de focus ; active=false ne cache pas ses enfants. Source : [focus.hpp](../include/nativeui/focus.hpp), `FocusScope`, `FocusScopeComponent` ; routage dans les services Tree existants.

MyGo `ui/scope.go`, `enterScope`, `arrangeFocus`, `restoreFocus`, possède une modalité liée aux overlays. Ne pas importer ces services : utiliser le scope NativeUI et ses overlays.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> FocusScope(Binding<bool> active, Child&& child);
template<class Child> FocusScope(State<bool>& active, Child&& child);
FocusScope&& trap(bool value=true) &&;
FocusScope&& default_focus(std::size_t focusable_descendant_index) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<bool> active{true};
auto scope = ui::FocusScope{active, ui::Button{"OK", []{}}}
    .trap(true).default_focus(0);
```

Trap par défaut true, default index zéro. FocusScopeComponent reste public, y compris les hooks is_focus_scope/focus_scope_active/traps/default_index.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Binding active copié, trap/index immuables dans la recette. Observer invalide focus et paint selon le runtime, sans changer disponibilité ou modèle enfants. Aucun « current scope » global. L’ordre focusable descendant est déterminé au moment de récupération, pas par pointeurs figés.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- active=false retire descendants des cibles de focus, mais laisse paint/visibilité et input pointeur selon runtime.
- trap=true conserve Tab/Shift-Tab dans le domaine actif.
- default_focus est indice de descendant focusable éligible, pas index d’enfant immédiat.
- Indice hors plage : récupération déterministe sur premier éligible, domaine vide sans focus.
- Le scope ne consomme pas Échap et n’est pas modal à lui seul.
- Les popups/dialogues établissent leur propre policy à travers services existants.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Wrapper transparent : premier enfant fournit minimum/préférée et reçoit bounds. Aucun espace supplémentaire pour focus ring. Le rectangle sémantique/focus des descendants reste leur propre placement.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide ; aucun backdrop ou contour propre. Focus visible est dessiné par descendants. Une bascule active doit nettoyer les anciens focus rings sans transformer la zone en Hidden. Pas d’animation du scope.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None`, lecture de descendants indépendante de focus-active. Le runtime reste autorité du focus accessible. Les demandes Focus vers un domaine inactif sont rejetées sans casser les snapshots de valeurs.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Conserver active Binding après recette, subscription libérée à unmount. Désactiver un scope sous callback de focus doit laisser un checkpoint de récupération durable et éviter récursion de transferts. Les identités de restitution de focus sont stables/weak, aucun Node* retenu.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend Tree focus/domaines, [Visibility](visibility.md) et [Dialog](dialog.md). Cas : nested traps, active false initial, aucun contrôle, index hors plage, enfant disabled et suppression du scope courant. Ne pas copier focus manager MyGo.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/focus_scope.hpp` et `src/focus_scope.cpp`.

focus_scope.hpp/cpp extraient les hooks et mesure non templates ; focus.hpp reste compatible et FocusScopeComponent public. Aucun type natif ou registry globale dans l’API.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `focus_scope_inactive_visible` : pixels présents, Tab ne cible pas enfants.
- `focus_scope_trap_cycle` : Tab/Shift-Tab bouclent correctement.
- `focus_scope_default_index` : indice parmi éligibles, hors plage fallback.
- `focus_scope_nested` : domaines imbriqués et restitution.
- `focus_scope_remove_focused` : suppression sûre avec focus actif.
- `focus_scope_focus_callback_throw` : prochain transfert reste possible.

Créer l’exemple public futur `examples/features/focus_scope.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
