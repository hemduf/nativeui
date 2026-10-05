# Fieldset

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Fieldset regroupe des fields sous une légende et une disponibilité héritée. NativeUI possède les wrappers Enabled/ReadOnly mais aucun Fieldset. Fondations : [component_state.hpp](../include/nativeui/component_state.hpp).

MyGo `ui/form.go` : `Fieldset`. La légende nomme le Group et les labels internes participent au Form extérieur. NativeUI reprend ce regroupement sans fusionner Field/Fieldset.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
template<class... Children> Fieldset(std::string legend, Children&&... fields);
Fieldset&& enabled(Binding<bool>) &&;
Fieldset&& enabled(State<bool>&) &&;
Fieldset&& read_only(Binding<bool>) &&;
Fieldset&& read_only(State<bool>&) &&;
Fieldset&& description(std::string) &&;
Fieldset&& style(FieldsetStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<bool> allowed{true};
ui::State<std::string> address{""};
auto shipping = ui::Fieldset{"Livraison",
    ui::Field{"Adresse",ui::TextInput{"",address}}}.enabled(allowed);
```

Defaults enabled true/read_only false, légende possédée. Variantes de bordure et espacement dans FieldsetStyle, pas un composant « section de form » séparé.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Le groupe possède legend/description/children ; états d’availability optionnels Binding. Sans binding, héritage normal. Les valeurs contrôles restent applicatives, aucune map globale de noms ou état « formulaire courant ». Les Field imbriqués contribuent au contexte Form le plus proche, sauf Form interne qui démarre son propre contexte.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Légende n’est pas un bouton ni arrêt Tab. Tab/pointeur vont aux descendants. False enabled bloque actions selon héritage sans effacer valeurs ; read-only garde navigation/lecture. Pas d’action groupée toggle à l’activation de légende. Échap/Enter restent les contrôles/Form.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Légende au-dessus de la colonne des enfants, gaps/padding explicites. En Form, ne pas commencer une nouvelle colonne de labels : garder alignement courant. Légende longue wrap ; empty legend laisse une description/groupe éventuel mais aucun texte vide annoncé. Overflow requiert ScrollView externe.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

FieldsetStyle : legend typography, spacing, option border/padding. Ne pas appliquer une opacité uniforme pour disabled ; controls résolvent leur recette. Repaint/layout classés selon changement réel de legend/style/availability. Aucune animation permanente.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Group avec name=legend et description possédée. Les noms spécifiques des champs sont conservés, pas remplacés par legend. Disabled/read-only effectifs se répercutent sur descendants. Group sans legend n’invente pas un label depuis le premier Field.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Abonnements availability et contexte Form RAII propre à instance. Retrait réentrant pendant mesure doit supprimer contribution au contexte sans garder référence de label. Destruction n’appelle pas de validation ou callback d’enabled ; cleanup no-throw.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[Field](field.md), [Form](form.md), [Enabled](enabled.md), [ReadOnly](read_only.md). Cas : groupe vide, légende vide, fieldset imbriqué, nested Form, state availability expiré, retirer sous focus/capture. Aucun ordre de validation automatique ni transfert audio.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/fieldset.hpp` et `src/fieldset.cpp`.

fieldset.hpp déclare builder/style et adaptateurs enfants/State ; fieldset.cpp contient Group retained, lexical participation Form, mesure/layout/availability/paint legend. Couple distinct de field.cpp et form.cpp ; aucun déplacement des styles d’enfants vers ce modèle.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `fieldset_semantic_group` : legend name, children names conservés.
- `fieldset_form_column` : alignement avec fields extérieurs.
- `fieldset_nested_form` : nouveau contexte isolé quand Form imbriqué.
- `fieldset_availability` : enabled/read-only restrictifs sans effacer données.
- `fieldset_empty_legend` : mesure/readout accessibles cohérents.
- `fieldset_removal_fault` : contexte/subscriptions récupérés après exception.

Créer l’exemple public futur `examples/features/fieldset.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
