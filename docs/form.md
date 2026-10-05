# Form

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Form organise Field/Fieldset avec labels alignés et commandes de validation explicites. Aucun Form NativeUI actuel. Fondations : [layout.hpp](../include/nativeui/layout.hpp), [command.hpp](../include/nativeui/command.hpp).

MyGo `ui/form.go` : `Form`, `alignLabels`, `alignBaselines`. Labels à droite dans une colonne commune, premier baseline du contrôle. La cible reprend la composition et ne déduit pas un modèle métier global des descendants.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
enum class FormLayout { Aligned, Stacked, Responsive };
template<class... Children> explicit Form(Children&&... fields);
Form&& layout(FormLayout) &&;
Form&& stacked_below(double logical_width) &&;
Form&& on_submit(std::function<void()> callback) &&;
Form&& on_cancel(std::function<void()> callback) &&;
Form&& style(FormStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::string> name{"Ada"};
auto form = ui::Form{ui::Field{"Nom",ui::TextInput{"",name}}}
    .layout(ui::FormLayout::Responsive).stacked_below(360.0);
```

Defaults Aligned, responsive threshold 360 DIP quand Responsive choisi. Foundations cibles : `ChildMetrics::first_baseline` optional<float> et `Component::first_baseline(Size measured_size) const` default nullopt ; aucune baseline actuellement présente dans ChildMetrics.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Form ne possède pas les valeurs des champs et ne réécrit pas leurs bindings. Il possède enfants, styles et callbacks. Le contexte d’alignement lexical est propre au Form retenu, obtenu par service scoped du Tree, sans static « current form ». Fieldset participe au même contexte ; un Form imbriqué ouvre un autre contexte.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Tab suit les contrôles ; label click relève du Field. Command submit explicite appelle on_submit ; Enter dans TextArea reste insertion, pas submit implicite. Échap appelle on_cancel seulement si descendants n’ont pas consommé l’événement/commande. Aucun validation callback sur paint ou mesure. Form sans callback laisse les commandes bubble.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- En Aligned, largeur de colonne labels = max preferred label de ses Field participants.
- Aligner label au first_baseline du contrôle s’il est disponible ; sinon centrer avec sa première ligne/hauteur selon Field.
- En Stacked, label au-dessus, largeur labels non partagée.
- Responsive choisit mode selon largeur allouée, sans modifier la valeur du modèle ou remonter les contrôles.
- Labels vides ne créent pas un padding de texte mais conservent l’alignement requis.
- Mesure en passes bornées : collecte label metrics, résolution de width, mesure des contrôles puis placement.
- Erreurs/descriptions sont sous le contrôle et contribuent à la hauteur ; champs hidden/collapsed respectent disponibilité.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

FormStyle contient row gap/column gap et label alignment, pas les recettes visuelles des contrôles. Label width change provoque layout de participants ; couleur d’un Field isolé seulement paint. Le basculement Responsive nettoie anciens pixels sans perdre focus/selection texte.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Form cible Group optionnellement nommé ; Field établit nom/description des contrôles sans remplacer leurs noms explicites. Les relations riches labelled-by/errors nécessitent des extensions neutres futures ; la v1 peut composer des chaînes possédées name/description. Aucun mapping natif annoncé ici.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Enregistrement de Field auprès du contexte avec handle monotone/weak et RAII. À mesure, snapshot de participants ; retrait réentrant n’expose pas une référence invalidée. Une erreur de baseline/mesure conserve l’ancien layout et permet une nouvelle génération. Les callbacks submit/cancel n’ont pas de retry automatique.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Dépend [Field](field.md), [Fieldset](fieldset.md), layout baseline cible et CommandScope. Aucun stockage/validation métier automatique ni I/O réseau. Threshold doit être fini/non négatif ; champs non Field acceptés comme enfants de colonne sans participation label. Form vide légal.

Ajouter `Command::Submit` et `Command::Cancel` en fin de l’enum portable, avec tests de routage et mappings explicites fournis par l’application/adapter. Ils sont absents de l’API actuelle ; Enter n’est pas mappé automatiquement à Submit. Le champ multiline garde son insertion newline.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/form.hpp` et `src/form.cpp`.

form.hpp expose FormLayout/style/templates enfants ; form.cpp porte contexte scoped, collection de métriques et layout responsive/submit routing. La fondation baseline dans Component est un changement d’interface explicite à réaliser avant l’alignement ; ne pas la masquer comme déjà existante.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `form_label_column` : labels partagent la largeur maximale, nested Form isolé.
- `form_baseline_fallback` : baseline réel ou nullopt sans placement incohérent.
- `form_responsive_focus` : resize commute sans remount ni perte de selection.
- `form_submit_textarea` : Enter multiline non capturé par submit.
- `form_fieldset_alignment` : Fieldset participe au contexte courant.
- `form_participant_remove` : handle stale sûr, prochain layout correct.
- `form_measure_fault` : erreur puis recovery sans registry globale.

Créer l’exemple public futur `examples/features/form.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
