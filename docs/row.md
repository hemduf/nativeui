# Row

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Row` organise les enfants sur l’axe horizontal, avec allocation flex et alignement transversal. Il existe dans [layout_builders.inc](../include/nativeui/detail/layout_builders.inc) ; `RowComponent` dans [layout_components.inc](../include/nativeui/detail/layout_components.inc) est public et peint zéro pixel.

MyGo `ui/layout.go`, fonctions `flexLayout`, `resolveFlexible`, `justifyOffsets`, fournit un layout flex plus général. Le présent composant conserve son modèle sans retour automatique à la ligne ; ce portage n’introduit pas de flex-wrap implicite.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API existante à conserver :

```cpp
template<class... Children> explicit Row(Children&&... children);
Row&& gap(float value) &&;
Row&& align(Align value) &&;
Row&& justify(Justify value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
auto row = ui::Row{ui::Label{"Nom"}, ui::Label{"Valeur"}}
    .gap(8.0f).align(ui::Align::Center).justify(ui::Justify::Start);
```

Les valeurs par défaut sont `gap=18`, `Align::Start`, `Justify::Start`. Préserver `RowComponent(float, Align, Justify)` et les enums publics.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

- Le builder possède la liste ordonnée des `Spec` ; le runtime possède les composants montés.
- Aucun binding, état de sélection ni callback propre à `Row`.
- L’identité des enfants est gérée par le runtime ; une modification de contenu invalide le layout du parent.
- Les facteurs `Flex` appartiennent aux enfants, pas à une table mutable du conteneur.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Aucun arrêt de focus et aucune activation pour le conteneur.
- Le pointeur et le clavier ciblent les descendants dans l’ordre normal du runtime.
- Tab suit l’ordre de composition ; l’alignement visuel ne change pas l’ordre de lecture.
- Molette, capture, validation et annulation sont déléguées aux enfants.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Taille préférée : somme des largeurs, intervalles entre enfants, hauteur maximale.
- Minimum : même formule avec les minima des enfants.
- Mesure sans borne sur l’axe X ; la hauteur reste contrainte par le parent.
- Placement par `allocate_main_axis` : grow répartit l’excédent ; shrink retire l’espace sans franchir les minima.
- `Stretch` agit sur la hauteur ; `SpaceBetween` distribue uniquement l’espace libre positif.
- Gap négatif ou non fini devient zéro, comme aujourd’hui. Une ligne vide mesure zéro.
- Pas de clip automatique ; un déficit au-delà des minima déborde et se gère par [Clip](clip.md) ou [ScrollView](scroll_view.md).

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

- Le conteneur ne dessine ni fond, ni bordure, ni focus ring.
- Changer l’alignement ou l’écart au moment d’une reconstruction ne change que placement/métriques.
- La couleur et la typographie restent celles des descendants et de [StyleScope](style_scope.md).
- Les styles de widget ne modifient pas les valeurs explicites de layout.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Le rôle cible demeure `None` : aplatir le wrapper en conservant l’ordre de ses descendants. Aucun nom, valeur, action ou état sélectionné propre à la ligne. Une ligne sémantiquement nommée doit être enveloppée dans un groupe explicitement prévu par l’application.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Une erreur de mesure ou de montage d’un enfant ne publie pas un placement partiel. Garder la transaction de layout existante ; après récupération, reconstruire les positions depuis les métriques acceptées, sans réutiliser de référence vers un enfant retiré.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend de `ChildMetrics`, `Constraints`, `Align`, `Justify` et du noyau flex. Tester enfants collapsed, minimum supérieur à la largeur disponible, largeur zéro et redimensionnement. Un enfant retiré pendant son callback est réconcilié par le runtime, sans parcours manuel après suppression.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/row.hpp` et `src/row.cpp`.

Préserver l’include historique `layout.hpp`, les constructeurs variadiques, les signatures `float` et `RowComponent`. Extraire le layout depuis `layout_components.inc` ; le constructeur template ne fait que convertir et posséder les `Spec`. L’algorithme de placement reste non template dans `row.cpp`.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `row_intrinsic_and_empty` : 0, 1 et 3 enfants, gap appliqué exactement N−1 fois.
- `row_flex_minimum` : grow/shrink répartis ; minima jamais franchis.
- `row_alignment_justify` : Start/Center/End/Stretch et SpaceBetween, y compris déficit.
- `row_nonfinite_gap` : NaN/inf/négatif ramenés à zéro.
- `row_removed_during_layout` : exception/réconciliation puis resize valide.
- Réutiliser les contrats des tests existants `layout_flex_tests`, `layout_alignment_tests` et `layout_constraints_tests`.

Créer l’exemple public futur `examples/features/row.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
