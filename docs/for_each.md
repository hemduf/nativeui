# ForEach<T>

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`ForEach<T>` construit une liste dynamique d’enfants dont l’identité suit la clé, mais ne définit pas un layout ni une virtualisation. Source : [dynamic.hpp](../include/nativeui/dynamic.hpp), `ForEach<T>`, `ForEachComponent<T>` ; [dynamic_key.hpp](../include/nativeui/detail/dynamic_key.hpp), `encode_dynamic_key`.

MyGo utilise les clés d’Element dans la composition immédiate ; aucun composant catalogue autonome équivalent. La cible conserve les contraintes NativeUI de clés et la récupération transactionnelle.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API actuelle :

```cpp
using Items = std::vector<T>;
template<class KeyFunction, class ChildFunction>
ForEach(Binding<Items> state, KeyFunction key, ChildFunction child);
template<class KeyFunction, class ChildFunction>
ForEach(State<Items>& state, KeyFunction key, ChildFunction child);
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<std::vector<int>> ids{{1,2,3}};
auto items = ui::ForEach<int>{ids, [](int id){return id;},
    [](int id){return ui::Label{std::to_string(id)};}};
```

Factory prend `const T&` et retourne Spec ou builder convertissable par make_spec. Clés actuelles : string/string_view-convertible, intégrales signées/non signées, enum ; autres types refusés à compilation. Conserver guides de déduction actuels.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Binding Items possédé ; fonctions adaptées en std::function possédées. Clé encodée copiée avec préfixes s:/i:/u:, pas adresse de T ni indice implicite. Identité survit au reorder. Même clé conserve la sous-arborescence retenue : les données affichées évolutives doivent être bindées, une factory n’est pas une promesse de replacement de child à chaque valeur.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Aucun focus propre ; wrappers générés gardent leurs interactions. Retrait d’une clé récupère focus/capture via Tree. Ajout/reorder ne génère pas de click. Validation du contenu enfant reste locale. Pas de raccourcis de sélection ou molette virtualisée.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Le host dynamique mesure actuellement le maximum des enfants sur chaque axe et les superpose sur bounds. Il ne faut pas le documenter comme Column automatique. Utiliser les modèles de composition/layout appropriés pour disposer les items. Les mutations de dataset déclenchent structure/layout au checkpoint.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

Paint vide ; paint des children géré par Tree. Same keys/order n’oblige pas à reconstruire des recipes ; mises à jour internes bindées invalident leur propre widget. Les factories ne doivent pas dessiner ou modifier le thème global.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Wrapper `None` et enfants dans l’ordre logique accepté. Clés dupliquées n’exposent pas deux semantic nodes avec même identité. Retrait/réinsertion d’une même clé après destruction reçoit un nouvel ID vivant, sans résurrection d’un proxy périmé.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Valider toutes les clés d’un snapshot avant commit. Duplicates : éviter publication initiale ambiguë, conserver la politique runtime de diagnostic/quarantaine et permettre une génération ultérieure valide. Une factory/key-function qui lève restaure guards, ne publie pas des enfants partiels et ne rejoue pas un callback commencé automatiquement.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Source dynamique, encode_dynamic_key et safe reconciliation. Cas : vide, duplicate encodé, type enum, changement de données à clé égale, reorder/removal sous callback, factory modifiant le dataset. Lire un snapshot stable par passe ; ne pas garder `const T&` après callback.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/for_each.hpp` et `src/for_each.cpp`.

for_each.hpp garde adaptation de clé/factory et liaison typée au Binding ; for_each.cpp porte le host/source dynamique non template, identity strings et reconciliation demandée. Pas de restriction aux int/string instanciés explicitement. dynamic.hpp et le comportement de conservation par clé restent compatibles.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `foreach_key_types` : signed/unsigned/string/enum encodés distinctement.
- `foreach_reorder_identity` : focus/état suivent clé, pas index.
- `foreach_equal_key_data` : child conservé, Binding interne mis à jour.
- `foreach_duplicate_recovery` : dataset invalide puis nouvelle génération valide.
- `foreach_factory_throw` : aucun demi-mount, reprise sans replay.
- `foreach_reentrant_replace` : snapshot cohérent même si factory modifie state.
- Préserver tests dynamic recovery et quarantine epoch existants.

Créer l’exemple public futur `examples/features/for_each.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
