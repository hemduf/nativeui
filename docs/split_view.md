# SplitView

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Deux panes ajustables par un séparateur, côte à côte ou empilés. Aucun builder SplitView dans NativeUI étudié ; composer Row/Column ne fournit pas le contrat d’input du splitter. Fondations : [layout.hpp](../include/nativeui/layout.hpp), [component_base.hpp](../include/nativeui/component_base.hpp).

Référence MyGo `ui/split.go` : `Split`, `SplitVertical`, `split`. MyGo maintient la taille du premier pane en DIP, minima 40 DIP et ligne de 1 DIP. La cible ajoute callbacks explicites et récupération d’annulation.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
enum class SplitOrientation { Horizontal, Vertical };
template<class First, class Second>
SplitView(Binding<double> first_extent, First&& first, Second&& second);
template<class First, class Second>
SplitView(State<double>& first_extent, First&& first, Second&& second);
SplitView&& orientation(SplitOrientation) &&;
SplitView&& minimum_panes(double first, double second) &&;
SplitView&& step(double value) &&;
SplitView&& on_change(std::function<void(double)> callback) &&;
SplitView&& on_commit(std::function<void(double)> callback) &&;
SplitView&& style(SplitViewStyle) &&;
Spec spec() &&;
```

Exemple futur :

```cpp
ui::State<double> sidebar_width{220.0};
auto split = ui::SplitView{sidebar_width, ui::Label{"Sources"}, ui::Label{"Éditeur"}}
    .minimum_panes(40.0,40.0).orientation(ui::SplitOrientation::Horizontal);
```

Defaults : Horizontal, minima 40/40, step 10 DIP, ligne 1 DIP, hit grip 6 DIP. Le style conserve ces métriques comme champs contrôlables, sans nouveau slot Theme supposé.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

L’application possède l’extent désiré du premier pane. La vue calcule un extent effectif borné par viewport sans réécrire le modèle au seul resize. Les gestes écrivent Binding puis on_change si valeur différente ; release appelle on_commit une fois si édition modifiée. L’origine de gesture et la génération externe servent à une annulation sûre ; un write externe pendant drag termine le drag sans rollback de la valeur externe.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Down dans grip capture le pointeur et focus le séparateur ; move ajuste l’axe choisi depuis origine.
- Up commit et release ; PointerCancel/Échap restaure l’origine si aucune valeur externe concurrente n’a remplacé la gesture.
- Flèches de l’orientation ajustent d’un step ; Shift multiplie par dix ; Home/End placent aux bornes.
- La molette ne modifie pas le splitter et bubble.
- Tab suit pane 1, séparateur, pane 2 ; disabled bloque input, read-only permet focus sans ajuster.
- Perte de visibilité ou desmontage annule capture sans callback applicatif de teardown.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Première dimension partagée = extent effectif, puis ligne, puis reste ; cross axis commun. Si minima impossibles, répartir proportionnellement l’espace disponible entre minima et ligne bornée, sans tailles négatives. Enfant overflow clippé à son pane. Le hit grip chevauche panes sans réserver 6 DIP de gap. Type double modèle converti en float validé au placement.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

SplitViewStyle définit ligne, grip, couleur hover/drag/focus et métriques. Line accent pendant hover/drag, ring sur grip focus-visible. Changement d’extent demande layout des panes ; couleur seulement paint séparateur. Aucun dessin backend dans header.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Le rôle Splitter n’existe pas actuellement : cible `Custom` avec nom « Séparateur », valeur/range, Focus/Increment/Decrement/SetValue quand éligibles. Un rôle spécialisé serait une extension distincte. Panes gardent leurs descendants et ordre logique.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer candidate finite/bornée avant setter. Reset capture/guards avant appeler on_commit ; callback throwing ne rejoue pas commit. Invalidateur stale après retrait est no-op via weak owner. Désabonner et annuler état de capture sans utilisateur pendant destruction.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend Row/Column, clipping, capture/focus, Binding et style. Refuser minima/step négatifs ou non finis par invalid_argument à construction, step strictement positif. Extent externe non fini : afficher premier minimum, sans réécriture externe automatique ; prochain extent valide récupère. Deux enfants exigés ; pas de pane management général en v1.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/split_view.hpp` et `src/split_view.cpp`.

split_view.hpp déclare orientation/style/public splitter Component et templates de conversion de deux enfants. split_view.cpp porte extent effectif, gesture transaction, measure/layout et paint. Aucun alias séparé SplitVertical, cette variante demeure dans le couple SplitView.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `split_orientations` : horizontal/vertical géométrie et flèches.
- `split_minimum_small_view` : espace insuffisant sans rect négatif.
- `split_live_commit_cancel` : writes/callbacks comptés, Échap restaure.
- `split_external_during_drag` : externe gagne, pas de rollback stale.
- `split_resize_no_model_write` : bounding effectif ne réécrit pas extent.
- `split_removed_under_capture` : capture et callbacks stale nettoyés.
- `split_commit_throw` : commit at-most-once et gesture suivante possible.

Créer l’exemple public futur `examples/features/split_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
