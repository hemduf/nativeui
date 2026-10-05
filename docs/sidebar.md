# Sidebar<Key>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

Sidebar est une navigation mono-sélection en sections repliables, icônes et accessoires de ligne. NativeUI ListView n’exprime pas encore les sections de navigation ; ce composant s’appuie sur liste/disclosure plutôt que Router.

MyGo `ui/sidebar.go` : `Sidebar`, `SidebarSection`, `SidebarItem`, `keys`. Un arrêt de Tab, sélection par ID, typeahead, sections sous titre et scroll lorsque contenu dépasse.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API cible proposée, membres de Sidebar<Key> :

```cpp
explicit Sidebar(Binding<std::optional<Key>> selection);
explicit Sidebar(State<std::optional<Key>>& selection);
Sidebar&& section(std::string id,std::string title,Binding<bool> open) &&;
Sidebar&& section(std::string id,std::string title) &&;
template<class Accessory>
Sidebar&& item(Key key,std::string label,Accessory&& accessory,bool enabled=true) &&;
Sidebar&& item(Key key,std::string label,bool enabled=true) &&;
Sidebar&& on_navigate(std::function<void(const Key&)>) &&;
Sidebar&& style(SidebarStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::optional<std::string>> mailbox{std::string{"inbox"}};
ui::State<bool> open{true};
auto nav = ui::Sidebar<std::string>{mailbox}
    .section("mail","Boîtes",open.binding())
    .item("inbox","Réception").item("sent","Envoyés");
```

State<bool>& surcharge section. Sans open binding section toujours ouverte. item s’attache à la dernière section déclarée ; items avant sections forment un groupe racine sans header. Section/item keys uniques dans leurs domaines ; accessory Spec peut contenir IconView/Badge, sans fonction métier interactive par défaut.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Selection mono binding authoritative ; section open bindings own safe refs, sans section controller global. Items/sections builder static v1, reconstruction app pour changer ensemble ; keys préservent identités via retained recipe là où possible. Navigate callback après changement choisi, key copie détenue ; key externe inconnue ou section collapsed n’impose aucune selection différente. Binding invalide garde texte sélectionné lisible mais gesture mutation refusée.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

Up/Down/Home/End sur items visibles enabled ; typeahead labels buffer 700 ms. Click item sélectionne et appelle on_navigate une fois si nouvelle sélection ; Enter sur item déjà choisi active on_navigate sans nouveau write. Header click toggle si binding open ; headers focusables via roving list et Left/Right ferment/ouvrent. Un arrêt Tab principal et contrôles accessoires non focusables ; accessory explicit interactive doit conserver ses propres actions sans selection click synthétique. Section close sous active/focus récupère header/voisin, pas une navigation métier automatique.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Colonne dans ScrollView, header/rows variables selon style, indent d’un niveau sous section. Width fournie par parent (aucune largeur fixe globale), preferred default220 DIP, min120 DIP via style. Labels single-line ellipsis ; badges/accessory gardent intrinsèque dans limite du row. Overflow vertical scroll, horizontal clip. Un header collapsed masque items et leur contribution à la hauteur.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

SidebarStyle : surface, padding, section typography, row active/selected/hover, indent/gap. Selected accent distinct de header hover. Badge visuel ne change pas le label automatiquement ; description/accessibility pourrait donner compteur explicitement. Section arrows peuvent animer localement 150ms reduced motion, arrêt hidden/unmount.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Tree rôle MyGo n’existe pas dans NativeUI : Group/Custom et expanded sur headers, ListItem ou Custom sur destinations avec Select/Activate/Focus éligibles. Chaque section reste grouping logique ; labels owned et selection key snapshots cohérents. Native source-list/tree mapping futur est une extension séparée.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Résoudre key à release et copier callback avant user code. Navigation peut retirer Sidebar ou reconstruire sections au checkpoint ; ne pas continuer sur un index stale. Open binding expiré devient non modifiable, dernier open lisible jusqu’à prochaine lecture sûre. Teardown libère observers/timers/focus references, jamais callbacks métier.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[ListView](list_view.md), [Collapsible](collapsible.md), [ScrollView](scroll_view.md), IconView/Badge optionnels. Cas : aucun item, all disabled, unknown key, duplicate item/section key reject initial, section sans open, long title, accessory wide et retrait pendant navigation. Aucun Router imposé ; on_navigate injecté par app.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/sidebar.hpp` et `src/sidebar.cpp`.

sidebar.hpp contient modèles de section/item dans le couple, templates keys/accessories et style. sidebar.cpp porte roving/typeahead, disclosure state, layout/paint et activation. Aucun fichier autonome SidebarItem ou SidebarSection ; clés own type erased et noyau non template.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `sidebar_sections_root` : attach item à section courante ou groupe racine.
- `sidebar_selection_navigation` : callback changement/Enter conforme.
- `sidebar_typeahead_roving` : labels, skip disabled, un arrêt Tab.
- `sidebar_close_active` : focus récupéré sans navigation métier automatique.
- `sidebar_unknown_duplicate` : unknown externe inert, duplicates refused.
- `sidebar_accessory_layout` : wide badge/icon et ellipsis.
- `sidebar_reentrant_navigation` : suppression au callback sans UAF.
- `sidebar_open_binding_dead` : prochain accès safe, aucun write synthétique.

Créer l’exemple public futur `examples/features/sidebar.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
