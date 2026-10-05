# ReadOnly

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`ReadOnly` garde les valeurs lisibles et, suivant le contrôle, focus/copie/navigation, en bloquant leur mutation. Source : [component_state.hpp](../include/nativeui/component_state.hpp), `ReadOnly`, `ReadOnlyComponent`.

MyGo exprime cette propriété par états d’Element ; ce wrapper NativeUI n’a pas d’équivalent autonome de catalogue. Le code actuel State-only doit être extrait et enrichi avec Binding.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API actuelle :

```cpp
template<class Child> ReadOnly(State<bool>& state, Child&& child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<bool> locked{true};
ui::State<std::string> text{"Valeur"};
auto field = ui::ReadOnly{locked, ui::TextInput{"Valeur", text}};
```

Cible : même constructeur prenant Binding<bool>. Les handlers de widget restent responsables de définir leurs actions non mutantes ; le wrapper ne remplace aucune valeur ni callback.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

ReadOnly effectif est restrictif : un ancêtre true impose la lecture seule. Ne pas figer le Binding de valeur enfant : les modifications applicatives externes restent visibles. Aucun callback de changement de mode ajouté ; observer l’état d’entrée suffit.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Texte : lecture, sélection, copie et focus permis ; insertion/coupe/collage mutante refusés.
- Contrôles de valeur : navigation/focus permis si leur contrat le prévoit, ajustement refusé.
- Les actions sans valeur mutable suivent le contrat propre du widget ; ne pas assimiler tous les buttons à disabled.
- Transition au milieu d’une gesture : récupération du gesture de mutation sans nouveau write.
- Wrapper sans focus/capture, ni traitement d’Échap ou de molette direct.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Mesure/minimum/placement transparents ; aucune suppression d’espace. Si recette visuelle read-only change métriques, l’enfant demande layout ; le wrapper ne suppose pas que toutes les recettes ont même dimension.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Aucun paint. Conserver lisibilité distincte de disabled ; ne pas réduire automatiquement l’opacité. Les descendants publient l’apparence read-only résolue. Changement externe de valeur demande le paint/layout approprié sans callback de geste utilisateur.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None` ; descendants lisibles avec `read_only=true`. Refuser SetValue/Increment/Decrement et autres mutations annoncées ; Focus/lecture/navigation restent disponibles quand éligibles. Le bridge ne doit jamais contourner l’état via écriture directe.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Remplacement réentrant du mode pendant editing conserve les données commitées ; ne pas remettre une ancienne valeur par rollback tardif. Un binding expiré ne produit aucun accès stale. Restaurer les guards d’édition après exception et laisser le runtime récupérer focus/capture.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Disponibilité héritée et implémentations enfants ; [Enabled](enabled.md) demeure une propriété distincte. Cas : verrouillage pendant drag slider, text draft, callback réentrant, modifications externes quand verrouillé, nested scopes. IME préedit non disponible n’est pas ajouté ici.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/read_only.hpp` et `src/read_only.cpp`.

read_only.hpp/cpp extraient le noyau non template de lecture seule, component_state.hpp reste façade. Garder ReadOnly State& et ajouter Binding sans transformer les modèles de valeur des widgets ni leurs surcharges historiques.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `readonly_text_copy` : sélection/copie fonctionnent, paste ne modifie pas.
- `readonly_value_gesture` : slider focusable mais write refusé.
- `readonly_external_update` : nouvelle valeur externe visible.
- `readonly_inheritance` : parent true non annulable.
- `readonly_mid_drag` : lock sous capture, aucun write après verrouillage.
- `readonly_throw_recovery` : exception puis prochaine interaction valide.

Créer l’exemple public futur `examples/features/read_only.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
