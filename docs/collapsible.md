# Collapsible

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Un header de disclosure ouvre/ferme un contenu. NativeUI ne possède pas ce builder ; [Visibility](visibility.md), [If](if.md) et focus servent de fondations.

MyGo `ui/collapsible.go` : `CollapsibleBase`, `Collapsible`, `disclosureArrow`. La cible propose une recette publique complète, contenu maintenu monté pour conserver les bindings et identités ; une policy explicite permet le démontage.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
enum class DisclosureContentPolicy { Retain, UnmountWhenClosed };
template<class Child> Collapsible(std::string title, Binding<bool> open, Child&& child);
template<class Child> Collapsible(std::string title, State<bool>& open, Child&& child);
Collapsible&& content_policy(DisclosureContentPolicy) &&;
Collapsible&& on_change(std::function<void(bool)> callback) &&;
Collapsible&& style(CollapsibleStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> open{false};
auto section = ui::Collapsible{"Avancé", open, ui::Label{"Réglages"}};
```

Defaults : closed selon binding, policy Retain, animation visuelle 150 ms respectant reduced motion ; title vide autorisé si nom accessible fourni par style/composition de header cible. Le header est une partie interne, pas un fichier autonome.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Bool externe autoritaire ; gesture écrit le Binding, on_change après write effectif seulement. Write applicatif externe ajuste le contenu sans on_change utilisateur. Retain conserve enfant monté collapsed ; UnmountWhenClosed retire sa structure et remonte une nouvelle identité à l’ouverture.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Click header/chevron ou Enter/Space bascule une fois au release ; drag hors hit target annule le press sans write. Left ferme et Right ouvre si l’état change ; Échap annule un press mais ne ferme pas une section déjà ouverte. Pas de molette. Focus header conservé ; si contenu contient focus au close, restituer au header au safe checkpoint. Read-only bloque toggle sans rendre texte illisible.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Header mesure label/chevron, contenu dessous avec gap/style. Fermé : métriques du header seul et aucune zone ciblable du contenu. Pendant animation, contenu clipped et non focusable dès close commencé ; layout peut animer hauteur avec borne finie. Reduced motion saute à la hauteur finale. Largeur parent contraint les descendants.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

CollapsibleStyle détient gap/padding/chevron/radius et état hover/pressed/focus. Chevron rotation donne état, pas seule indication disponible. Animation par instance et arrêté caché/démonté ; changement de open égal ne redémarre pas timeline. Aucun slot Theme prétendu déjà présent.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Header cible `Custom` ou `Button` avec name, expanded, Focus/Expand/Collapse. Le rôle Disclosure n’existe pas encore. Contenu fermé absent du snapshot actionable selon disponibilité ; la relation header/panel est une extension neutre future, pas un mapping natif livré.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer le nouveau state avant write ; après callback réentrant fermer/open, relire la valeur autoritaire sans reboucler les callbacks. Retain évite remontage pendant animation. En policy Unmount, factory/mount throwing utilise transaction Tree, sans child fantôme. Timer/pending invalidation weak annulé au teardown.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Binding bool, [Visibility](visibility.md), [If](if.md), clipping et focus. Cas : contenu vide, title long, fermeture pendant text edit, state expiré, open/close rapides et exceptions on_change. Ne pas porter Local/MyGo c.context global.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/collapsible.hpp` et `src/collapsible.cpp`.

collapsible.hpp déclare policy, style, builder et callbacks ; conversion enfant seule template. collapsible.cpp contient header/panel, animation/mesure/layout/input/paint et adaptation au service Tree existant. Pas de wrapper .cpp vide autour de Visibility.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `collapsible_keyboard_pointer` : Enter/Space/click exactly one toggle.
- `collapsible_retain_vs_unmount` : identité/compteurs de mount conformes.
- `collapsible_close_focus` : focus contenu revient au header.
- `collapsible_rapid_animation` : inversions cohérentes et reduced motion.
- `collapsible_closed_hit` : child n’est plus targetable dès close.
- `collapsible_reentrant_change` : externe final autoritaire, callback non rejoué.
- `collapsible_hidden_timer` : aucune invalidation après unmount.

Créer l’exemple public futur `examples/features/collapsible.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
