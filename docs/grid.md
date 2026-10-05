# Grid

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Grid` existe avec pistes Fixed/Auto/Flex et placement séquentiel ligne par ligne. Sources : [layout_model.inc](../include/nativeui/detail/layout_model.inc), `Track`, `GridTracks` ; [layout_components.inc](../include/nativeui/detail/layout_components.inc), `GridComponent` ; [layout_builders.inc](../include/nativeui/detail/layout_builders.inc).

MyGo `ui/grid.go` : `Grid`, `ColumnStart`, `RowStart`, `ColumnSpan`, `RowSpan`, `gridPlace`, `sizeTracks`. Les écarts à combler sont le placement explicite et les cellules fusionnées ; le layout reste distinct de [GridView](grid_view.md).

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante à conserver :

```cpp
template<class... Children> Grid(GridTracks tracks, Children&&... children);
Grid&& gap(float value) &&;
Grid&& column_gap(float value) &&;
Grid&& row_gap(float value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto grid = ui::Grid{
    ui::GridTracks{.columns={ui::Track::fixed(100.0f), ui::Track::flex(1.0f)},
                   .rows={ui::Track::auto_size()}},
    ui::Label{"Nom"}, ui::Label{"Valeur"}}.gap(8.0f);
```

Extension cible : `GridCell{std::size_t row, column, row_span=1, column_span=1}` et `template<class Child> Grid&& cell(GridCell, Child&&) &&`. Indices zéro-based ; contenu constructeur continue en auto-placement. `Grid(GridTracks)` accepte zéro enfant puis `.cell`.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Pistes, placements et Spec sont possédés. Aucun binding de sélection ni callback. Une cellule ne référence jamais directement un nœud Tree. Valider les placements avant consommation ; l’ordre de Spec reste l’ordre logique même si les positions explicites diffèrent.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun focus/capture propre. Tab suit la composition ; le pointeur utilise les bounds enfants. Le layout ne propose pas de sélection de cellules ni de navigation par flèches, comportements de TableView/GridView. Les gestures et validations restent dans les contrôles enfants.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Fixed garde l’extent assaini ; Auto suit l’intrinsèque ; Flex distribue l’espace libre.
- Colonnes vides deviennent une Auto ; compléter les lignes requises par Auto.
- Revendiquer les cellules explicites avant auto-placement ; auto-placement row-major dans les premières cases libres sans densification rétroactive.
- Span zéro, overflow size_t, recouvrement explicite ou extent non représentable : `invalid_argument` avant publication.
- Contribution intrinsèque d’un span répartie sur Auto/Flex après soustraction des gaps, sans modifier Fixed.
- Déficit : réduction jusqu’aux minima puis débordement explicite, sans clip automatique.
- Passes de mesure bornées et publication unique ; pas de boucle de convergence non déterministe.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Aucun paint. Pistes/spans modifiés demandent layout ; la couleur enfant demande paint. Les styles descendants influencent les pistes via leurs métriques. Ne pas ajouter lignes de tableau ni sélection au Grid.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Rôle `None` ; enfants dans l’ordre de composition. La position 2D ne crée pas une sémantique Table. Les spans ne créent aucun descendant accessible fantôme ni double annonce.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Préparer occupation, pistes et allocations avant publier placements. Une erreur conserve la dernière génération cohérente. Une mesure qui lève ne laisse pas une piste définitivement marquée « mesurée » ; une passe suivante peut récupérer.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Allocation de pistes/contraintes existantes. Tester vide, rows déclarées insuffisantes, spans au-delà des pistes initiales, grands indices, poids zéro, fenêtre réduite. Le nombre d’enfants est distinct du nombre de cellules occupées.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/grid.hpp` et `src/grid.cpp`.

Préserver `GridComponent`, `Track`, `TrackType`, `GridTracks` par déclaration unique/réexports adaptés ; ne pas dupliquer les modèles entre headers. `grid.cpp` porte placement et sizing des spans. `layout.hpp`, anciennes signatures/gaps float et auto-placement sans spans restent compatibles.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `grid_legacy_tracks` : conserver les scénarios `grid_layout_tests`.
- `grid_span_intrinsic` : span sur Fixed/Auto/Flex avec minima.
- `grid_auto_occupied` : ignorer les cases explicitement occupées.
- `grid_overlap_overflow` : refuser validation sans demi-commit.
- `grid_empty_resize` : empty/zero bounds puis resize positif.
- `grid_span_fault` : allocation/mesure qui lève puis génération valide.

Créer l’exemple public futur `examples/features/grid.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
