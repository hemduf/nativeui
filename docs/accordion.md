# Accordion

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Sections de disclosure regroupées, avec navigation entre headers et politique d’ouverture. Pas d’Accordion NativeUI ; cible fondée sur [Collapsible](collapsible.md).

MyGo `ui/collapsible.go` : `Accordion`, `AccordionItem`, `accordionKeys`. Les sections sont indépendantes dans MyGo ; l’application ferme les autres pour une ouverture exclusive. NativeUI explicite cette policy dans le builder.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée :

```cpp
enum class AccordionMode { Multiple, Single };
Accordion(Binding<std::vector<std::string>> open_keys);
Accordion(State<std::vector<std::string>>& open_keys);
template<class Child>
Accordion&& section(std::string key, std::string title, Child&& content,
                     bool enabled=true) &&;
Accordion&& mode(AccordionMode) &&;
Accordion&& content_policy(DisclosureContentPolicy) &&;
Accordion&& on_change(std::function<void(const std::vector<std::string>&)>) &&;
Accordion&& style(AccordionStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<std::string>> open_keys{{"general"}};
auto panels = ui::Accordion{open_keys}.mode(ui::AccordionMode::Single)
    .section("general","Général",ui::Label{"Options"})
    .section("advanced","Avancé",ui::Label{"Réglages"});
```

Defaults Multiple, Retain. Section clés string non vides uniques ; items restent modèles internes du même couple.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Open keys ordered selon section order, duplicates removed. Multiple permet plusieurs sections ; Single ouverture remplace la seule clé et fermeture permet zéro. Snapshot externe contenant plusieurs clés en Single : première section correspondante affichée, pas de rewrite silencieux. Nouvelle gesture publie un vecteur canonique. Unknown keys ignorées visuellement.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Up/Down déplacent focus entre headers enabled ; Home/End premiers/derniers. Enter/Space bascule section focusée. Tab accède aux contenus ouverts ; un seul arrêt roving pour l’ensemble des headers. Pointeur header copie Collapsible. Close d’un contenu focusé retourne au header, ouverture exclusive récupère focus de l’ancien contenu sans double notification.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Colonne de headers/panels avec bordure groupée et séparateurs. Chaque section utilise son header et dimensions du contenu ouvert. Fermées hors allocation contenu. En single, publier l’ouverture/fermeture ensemble pour éviter une frame vide intermédiaire. Overflow de l’accordéon n’installe pas de scroll implicite.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

AccordionStyle complète CollapsibleStyle : container radius/border, separator et padding. Focus ring inset pour ne pas être clippé par bordure. Invalidation de section n’oblige pas repeindre les sections sans changement. Animation indépendante mais controller d’ouverture commun ; reduced motion identique à Collapsible.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Group avec nom éventuel, headers Custom/Button et état expanded. Expanded keys reflètent exactement la policy effective. Les descendants des panels closed ne sont pas des cibles accessibles. Un rôle Accordion serait une extension séparée ; pas de promesse native déjà livrée.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Valider clés avant consommer Spec. Changement Single prépare vecteur complet, commit unique au Binding puis callback unique. Callback réentrant modifiant la liste ouverte gagne à la prochaine reconciliation. Section retirée sous capture/focus invalide son identity et libère animations ; ne pas accéder à un ancien index.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Collapsible, Binding<vector<string>>, navigation focus/roving et style. Sections vides/title long, toutes disabled, keys unknown, duplicates de section invalid_argument, binding expired et reentrance. State externe vit assez longtemps ; aucune liste process-global des headers.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/accordion.hpp` et `src/accordion.cpp`.

accordion.hpp contient Section interne publique au besoin, policy/style et template section ; accordion.cpp porte mode, headers roving, normalisation, layout/input/paint. Réutiliser le noyau Collapsible sans créer un fichier par AccordionItem.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `accordion_multiple_single` : policies et zéro section ouverte.
- `accordion_external_noncanonical` : affichage déterministe sans writes cachés.
- `accordion_header_roving` : Up/Down/Home/End, skip disabled.
- `accordion_exclusive_focus` : fermer l’ancien panel sous focus.
- `accordion_keys_validation` : duplicates refusées avant commit.
- `accordion_reentrant_change` : callback at-most-once et état final stable.
- `accordion_section_teardown` : timers/captures propres à l’instance.

Créer l’exemple public futur `examples/features/accordion.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
