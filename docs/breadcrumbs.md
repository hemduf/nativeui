# Breadcrumbs

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Breadcrumbs affiche un chemin d’ancêtres navigables suivi de la destination actuelle. NativeUI ne possède pas ce widget ; les actions injectées utilisent la fondation button/popup, sans Router.

MyGo `ui/feedback.go` : `Breadcrumbs`. Les ancêtres sont des liens, la dernière composante est du texte non cliquable et labels longs s’ellipsent. La cible emploie des clés stables plutôt que l’indice chosen MyGo.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
struct BreadcrumbItem {
    std::string key;
    std::string label;
    bool enabled=true;
    bool operator==(const BreadcrumbItem&) const=default;
};
Breadcrumbs(Binding<std::vector<BreadcrumbItem>> path);
Breadcrumbs(State<std::vector<BreadcrumbItem>>& path);
Breadcrumbs&& on_navigate(std::function<void(const std::string&)>) &&;
Breadcrumbs&& label(std::string accessible_name) &&;
Breadcrumbs&& style(BreadcrumbsStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<ui::BreadcrumbItem>> path{{
    {"root","Disque"},{"home","Ada"},{"docs","Documents"}}};
auto crumbs = ui::Breadcrumbs{path}.label("Chemin")
    .on_navigate([](const std::string&){});
```

Callback obligatoire pour ancêtres activables ; sans callback, présentation seule. Pas de sélection additionnelle : la dernière clé est la destination, l’application remplace path après navigation. Modèle item et separators restent dans le couple.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Path Binding own safe reference, snapshot owned par génération. Keys non vides uniques. Dernier item non navigable même enabled ; ancêtres enabled et callback présents activables. Callback n’écrit pas path lui-même ; app décide navigation et update. Expiration binding conserve dernier path, refuse toute action de navigation à partir de donnée potentiellement périmée.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Click release/Enter/Space activent ancestor une seule fois, down-out/cancel disarm. Tab traverse ancêtres visibles enabled, dernier texte hors Tab. Échap ferme overflow popup ou annule press ; molette ne change pas destination. Path changé entre down/up : revalider key existe et reste ancestor enabled, sinon no-op. Right click n’ouvre aucun menu système automatiquement.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Row chevrons entre items, zéro divider autour du path. Fit par intrinsèques ; ellipsis intra-label d’abord. Si somme minima ne tient pas, garder premier et dernier et remplacer groupe intermédiaire par popup « … » ; petit viewport garde dernier + overflow si premier ne tient pas. Items cachés disponibles dans popup par key, pas objets séparés en doublon sémantique. Bounds/labels finites, layout sans chargement de path externe.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

BreadcrumbsStyle : font, padding, min item width, chevron/gap et focus/hover colors. Destination légèrement accentuée, enabled/disabled distinction textuelle compatible thème. Changed path/layout invalidates metrics, hover seulement pixels. Les icônes décoratives separators ne sont pas interactives.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Group nommé via label ; ancêtres cible Custom/Button tant que rôle Link absent, destination Text. Overflow PopupMenu expose les ancêtres cachés ; éviter de publier l’item caché à la fois dans barre et menu visible. name complet conserve label même ellipsis. Aucune relation URL native ni navigateur invoqué implicitement.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer snapshot path avant publication. Initial duplicate/empty key invalid_argument ; update invalide garde dernière génération acceptée et diagnostic. Popup open conserve keys owned, revalide dataset à choix ; retrait de key pendant popup ferme/removes stale entry sans callback. Callback throwing disarme press avant propagation et ne rejoue pas navigation.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[Button](button.md), [PopupMenu](popup_menu.md), [Popover](popover.md) services overlay au besoin, layout row/text shaping. Cas : path vide, singleton, toutes actions disabled, wide unicode label, shrinking bounds et callback qui remplace path. Pas de I/O fichier ou URL résolue par le composant.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/breadcrumbs.hpp` et `src/breadcrumbs.cpp`.

breadcrumbs.hpp déclare BreadcrumbItem/style/builder ; breadcrumbs.cpp porte fitting/overflow, retained items, activation et paint/layout. Ni fichier par item ni type Router exposé. Popup items sont modèles possédés et les overlays suivent le service existant.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `breadcrumbs_last_not_action` : singleton/dernier non focusable.
- `breadcrumbs_key_navigation` : ancêtre callback key exacte, pas indice.
- `breadcrumbs_overflow` : path très long, racine/destination priorités et menu.
- `breadcrumbs_changed_during_press` : key devenue leaf/retirée = no-op.
- `breadcrumbs_menu_stale` : dataset change avant choix, aucune key périmée.
- `breadcrumbs_semantic_label` : label complet malgré ellipsis.
- `breadcrumbs_callback_throw` : press libéré, prochaine action possible.

Créer l’exemple public futur `examples/features/breadcrumbs.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
