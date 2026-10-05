# StyleScope

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`StyleScope` applique des overrides lexicaux typés sur le Theme hérité. Source : [style_scope.hpp](../include/nativeui/style_scope.hpp), `StyleScope`, `StyleScopeOverrides`, `apply_style_scope_overrides`, `classify_style_scope_change`, `StyleScopeComponent`.

Pas de famille MyGo autonome à porter : MyGo hérite ses propriétés de style. L’API NativeUI est déjà publique, avec override immutable ou Binding ; son extraction doit préserver le contrat de priorité champ par champ.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> StyleScope(StyleScopeOverrides overrides, Child&& child);
template<class Child> StyleScope(Binding<StyleScopeOverrides> overrides, Child&& child);
template<class Child> StyleScope(State<StyleScopeOverrides>& overrides, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::StyleScopeOverrides patch;
patch.palette.accent=ui::Color{0.8f,0.3f,0.1f,1.0f};
auto scope = ui::StyleScope{patch, ui::Button{"Action", []{}}};
```

Conserver tous les types Palette/Typography/Spacing/Radii/ControlOverrides et les fonctions apply/classify. Options restent optional et leur absence signifie inherit ; ne pas ajouter contraintes ou modèle dans le patch.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Value form immutable pour la durée retenue ; Binding form remplacement UI-thread. State délègue déjà à binding et ne garde pas State*. Résolution du plus externe au plus interne, nearest-scope-wins par champ, puis recette locale du widget. Chaque scope possède son Theme résolu et observer weak.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun input/focus/capture propre. Les contrôles conservent leur interaction et disponibilité ; changer palette ne désactive pas un enfant. Les scopes peuvent s’imbriquer sans mode « active style » global. Aucun raccourci de thème.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Pass-through des contraintes, minimum/préférée et bounds enfant. Overrides typographiques/controls peuvent changer métriques descendants, mais width/height/Flex/Grid/scroll state explicites ne sont pas représentables dans StyleScopeOverrides et restent autoritaires.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Comparer Theme effectif avant/après, pas seulement patch brut : égalité effective None, couleur Paint, métriques Layout selon classify_theme_change existant. Ne pas re-mesurer pour accent seulement. La présence d’un override identique à la valeur héritée peut être un no-op effectif ; changement de parent doit recalculer descendants.

- La comparaison des couleurs conserve les composantes r/g/b/a du type Color existant.
- Une override family vide est une valeur explicite ; elle ne signifie pas nullopt/inherit.
- Une police fallback modifiée est une modification métrique potentielle à classer par le Theme resolver.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None`. Le style ne modifie aucun nom, rôle ou action. Les couleurs doivent conserver les contrastes choisis par la recette/app ; ne pas promettre un audit automatisé de contraste livré par ce composant.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Observer weak déconnecté avant destruction ; inherited Theme doit appartenir au runtime vivant, pas à un temporaire. Préparer prochain Theme avant publication, classer invalidation ; erreur de callback invalidator laisse pending work durable sans dangling theme pointer. Deux scopes ne doivent pas partager de resolved Theme mutable.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend Theme, ThemeBinding et classify_theme_change ; types float existants conservés. Cas : patch vide, inner champ nul, palette égale, font-family/fallbacks modifiés, parent Theme changé, state expiré. Aucun override de disponibilité/callback/model accepté.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/style_scope.hpp` et `src/style_scope.cpp`.

Le header de ce nom existe déjà : le conserver comme API canonique, déplacer implémentations non templates dans style_scope.cpp. Garder fonctions publiques/equality et types ; seules les trois conversions enfant templates restent inline. Les types détail ne deviennent pas un service global.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `style_scope_nearest_field` : priorité par champ, inherit par absence.
- `style_scope_effective_noop` : patch égal au Theme courant = aucune invalidation.
- `style_scope_paint_layout` : couleur Paint, typographie Layout.
- `style_scope_parent_change` : recalcul d’un nested patch partiel.
- `style_scope_local_recipe` : recette widget appliquée après scope.
- `style_scope_lifetime_fault` : expiration/throw puis callback stale inerte.

Créer l’exemple public futur `examples/features/style_scope.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
