# ScrollView

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`ScrollView` assemble Scroll, barres superposées, molette, pan facultatif et révélation des descendants focusés. Source : [scroll_view.inc](../include/nativeui/detail/scroll_view.inc), `ScrollView`, `ScrollViewComponent`, `ScrollbarComponent`, `ensure_visible`.

MyGo `ui/scroll.go` : `TrackScroll`, `ScrollIntoView`, `reveal`, `nearest`. Cette extraction conserve le comportement retenu actuel.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante :

```cpp
template<class Child> ScrollView(ScrollState& state, Child&& child);
ScrollView&& pointer_pan(bool enabled=true) &&;
ScrollView&& style(ScrollbarStyle value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::ScrollState offset{ui::ScrollAxis::Both};
auto view = ui::ScrollView{offset, ui::Label{"Contenu"}}.pointer_pan();
```

Préserver `ScrollAlignment`, les fonctions publiques `ensure_visible`, les surcharges de ScrollViewComponent et `ScrollbarStyle`. Pan désactivé par défaut.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

State externe emprunté avec lifetime token, style possédé. Aucun callback de fin de gesture ajouté. Les observers peuvent modifier l’offset ou détruire le contrôleur : revalider le token après chaque notification.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Hors Tab : focusable faux, pointer_targetable vrai.
- Molette bubbled handled si offset change ; aux limites, laisser l’ancêtre scroller.
- Pan opt-in : down capture, move depuis origine, up/cancel libère.
- Cancel conserve l’offset déjà publié, sans commit/cancel applicatif fictif.
- Thumb draggable et track paging suivent leur logique actuelle.
- Hit testing inverse : barres visibles prioritaires sur le contenu recouvert.
- Focus descendant appelle ensure_visible Nearest.
- Hidden/désactivation/démontage terminent capture et pan.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Mesure naturelle sur axes scrollables. Les barres ne retirent pas de place au contenu : overlays de layout, peintes après lui. Visible seulement si contenu dépasse viewport. Coin commun raccourcit les pistes. Bounds minuscules donnent épaisseurs/rectangles finis non négatifs.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Style ScrollbarStyle existant. Hover/thumb = paint local ; épaisseur = placement des overlays. Offset = repaint viewport. Aucun registre global de scrollbars ou animation permanente quand cachée.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None`. L’enum actuel ne possède pas Scrollbar : ne pas revendiquer ce rôle comme disponible. Une future sémantique de scroll doit étendre séparément le contrat ; descendants accessibles conservent disponibilité et bounds logiques.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

State expiré pendant pan : remettre pan à faux, relâcher capture sans relire state. Exception observer : restaurer bookkeeping/capture avant propagation, prochaine molette utilisable. Ne jamais retenir InputContext dans un callback observe.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend de [Scroll](scroll.md), ScrollState, ScrollbarStyle, focus reveal et clipping. Couvrir nested scroll, un seul axe dépassé, viewport nul, thickness non finie, descendant plus grand que viewport et état détruit au milieu du pan.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/scroll_view.hpp` et `src/scroll_view.cpp`.

Gestes/placement et barres internes dans scroll_view.cpp, API ScrollViewComponent préservée. Réexporter le header ScrollbarStyle existant. `layout.hpp` reste le point d’entrée historique ; ne pas dupliquer le contrôleur ScrollState.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `scroll_view_wheel_bubble` : limite inner, mouvement outer.
- `scroll_view_pan_cancel` : capture relâchée, offset conservé.
- `scroll_view_bar_priority` : track avant contenu.
- `scroll_view_reveal` : Nearest avec grand descendant.
- `scroll_view_dead_state` : expiration pendant callback/pan.
- `scroll_view_tiny_bounds` : Both sans rectangle négatif.
- `scroll_view_notify_fault` : nouvelle gesture valide après exception.

Créer l’exemple public futur `examples/features/scroll_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
