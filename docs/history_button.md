# HistoryButton

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

HistoryButton représente Retour ou Avance avec historique optionnel pour saut multiple. Aucun composant NativeUI dédié ; les actions doivent être injectées par l’application.

MyGo `ui/router.go` : `BackButton`, `ForwardButton`, `historyButton`. Un click appelle Go(±1), contexte menu liste jusqu’à quinze destinations ; le Router est une dépendance MyGo non portée par ce widget.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
enum class HistoryDirection { Backward, Forward };
struct HistoryEntry {
    std::string key;
    std::string title;
    bool operator==(const HistoryEntry&) const=default;
};
HistoryButton(HistoryDirection direction,Binding<bool> can_navigate,
              std::function<void(int signed_steps)> navigate);
HistoryButton(HistoryDirection direction,State<bool>& can_navigate,
              std::function<void(int signed_steps)> navigate);
HistoryButton&& entries(Binding<std::vector<HistoryEntry>>) &&;
HistoryButton&& entries(State<std::vector<HistoryEntry>>&) &&;
HistoryButton&& maximum_menu_entries(std::size_t count) &&;
HistoryButton&& label(std::string text) &&;
HistoryButton&& style(HistoryButtonStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> can_back{true};
auto back = ui::HistoryButton{ui::HistoryDirection::Backward,can_back,
    [](int steps){(void)steps;}};
```

Default menu cap15, labels « Retour »/« Avance », entries nearest-first. signed_steps négatif backward/positif forward. Les entries résument les destinations dans cette direction uniquement ; keys non vides uniques, titles peuvent être égaux.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

can_navigate Binding externe ; bouton effectif enabled = can true AND disponibilité inherited AND callback présent. entries optional Binding, snapshot own pour menu. Le composant ne maintient pas l’historique ni current index : app fournit liste et gère toutes writes. Binding invalide interdit navigation, aucun callback synthétique. Titles vides affichent key comme fallback explicite.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Primary click/Enter/Space appelle navigate(±1) une fois au release. Context request ou keyboard context command ouvre menu portable si entries nonempty/can true. Choix key remapée dans entries actuelles, appelle direction*(index+1), pas ancien menu index. Échap annule press/ferme menu. Long press n’est pas imposé v1. Molette ignorée, pas raccourci global Alt-Left/Right capturé par ce widget ; app compose ses commandes.

Le contrat cible de ce contrôle traite ReadOnly comme une interdiction de navigation, car celle-ci déclenche une mutation applicative ; le focus et la lecture du label restent permis. Revalider can_navigate, enabled et read_only à release et au choix de menu.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Button intrinsèque icon chevron + padding, minimum hit target résolu style, optional label visible si style choisi. Menu anchored à bounds via service overlay, contraintes viewport/scroll menu existants. Pas de dimension dédiée par longueur de tout l’historique et aucune répartition de pages UI.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

HistoryButtonStyle basé recette button/icon : hover/pressed/disabled/focus et dimensions. Can false efface pressed state et ferme le menu, sans navigation. Entries update peut reflow menu mais ne demande pas layout du bouton si label/icon inchangé. Pas de theme slot History déjà existant supposé.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle Button, nom descriptif Retour/Avance, Focus/Activate quand éligible ; menu PopupMenu/MenuItems nommés destinations. Direction alone n’est pas la seule information visible/accessibility. Disable bloque actions de mutation ; ne pas inventer un rôle Router ou historique natif.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Navigation peut fermer/détruire owner via safe deferred checkpoint ; bouton désarme et libère capture avant callback. Entries change pendant menu : revalider key/can avant signed step, key absente no-op et fermeture. Callback throwing n’est pas réessayé ; popup handles terminalisés/cleanup RAII. Démontage n’appelle pas navigate.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[Button](button.md), [IconView](icon_view.md), [PopupMenu](popup_menu.md)/[ContextMenu](context_menu.md), services Overlay/Focus. Cas : début/fin historique, entries empty, cap zéro menu désactivé, duplicate keys rejected avant snapshot, index > INT_MAX reject menu action sans overflow, can expired et callback absent. Aucun Router ou global history registry.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/history_button.hpp` et `src/history_button.cpp`.

history_button.hpp contient direction/entry/style/builders ; history_button.cpp porte retained activation, snapshot mapping de menu et paint/layout. Backward/Forward sont variantes de ce couple, pas headers/cpp distincts. Aucun type Router ni API OS dans signatures.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `history_directions_steps` : main clicks ±1 et saut menu ±N.
- `history_can_gating` : disabled/read-only/expired bindings, aucun callback.
- `history_menu_cap` : nearest-first limité15, cap0, labels égaux keys distinctes.
- `history_menu_rebase` : changement entries remappe key avant action.
- `history_destination_removed` : stale key no-op avec menu fermé.
- `history_press_cancel` : release out/cancel et fermeture safe.
- `history_navigate_throw` : callback at-most-once, interaction suivante valide.
- `history_no_router` : exemple fonctionne avec callbacks injectés seuls.

Créer l’exemple public futur `examples/features/history_button.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
