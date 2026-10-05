# Tabs<T>

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`Tabs<T>` existe avec onglets key/label/panel, sélection Binding, style et navigation. Source : [widgets_list_tabs.inc](../include/nativeui/detail/widgets_list_tabs.inc), `Tabs`, `TabsRuntime`, `TabsComponent`, `TabPanelComponent`.

MyGo `ui/tabs.go`, `Tabs` et son usage de `TabsBase`, compose header et application panels séparément. NativeUI possède déjà les panels ; leur état monté est conservé même quand Collapsed. Le portage ici est l’extraction compatible, pas une nouvelle API d’onglets close/reorder.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API actuelle à préserver :

```cpp
explicit Tabs(Binding<T> selection);
explicit Tabs(State<T>& selection);
template<class Child>
Tabs&& tab(T key,std::string label,Child&& panel,bool enabled=true) &&;
Tabs&& style(TabsStyle value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<int> page{0};
auto tabs = ui::Tabs<int>{page}.tab(0,"Général",ui::Label{"Réglages"})
    .tab(1,"Avancé",ui::Label{"Autres options"});
```

Keys égales rejetées par invalid_argument à `.tab`. Conserver deduction guides implicites et T utilisateur equality-comparable, sans hash requis. Aucun callbacks on_change actuellement ; les modèles applicatifs observent leur State.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

Binding possédé, labels/keys/enabled/Spec copiés ou déplacés dans runtime. Selected key externe détermine panel visible même si onglet disabled ; disabled interdit navigation utilisateur vers ce tab mais ne force pas un nouveau modèle. Unknown selected key = aucun panel visible, pas de correction/writes par défaut. Les panels sont montés et la disponibilité les collapse selon selected key.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Left/Right cycle entre onglets enabled avec wrap, Home/End premier/dernier.
- Focus entrée utilise sélection enabled ou premier enabled comme active sans écrire automatiquement le Binding.
- Click down capture et arme index ; up sur même onglet choisit, release hors onglet annule.
- PointerCancel relâche capture ; hover non-selected enabled seulement.
- Pas de molette/shortcut close/reorder/Enter ajoutés à l’extraction.
- Navigation autoactive : arrows écrivent directement sélection, pas mode manuel confirmation.
- Cible sécurité : Binding invalide refuse writes ; disponibilité/read-only est revalidée au moment du select.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

Header de hauteur résolue TabsStyle, tabs répartis en largeurs égales `bounds.w/count`. Panels sous header+panel_gap et reçoivent les mêmes bounds, les non sélectionnés collapsed ne contribuent pas. Preferred width au moins 120 DIP, minimum width zéro ; header borné par height disponible. Aucun scroll header automatique ou natural tab widths ajouté. Empty tabs ne divise pas par zéro.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

TabsStyle/list_tabs_style.hpp conservé avec résolveurs et VisualState. Selected underline/disabled text/focus/hover suivent recipe actuelle. Style metric header_height/panel_gap demande layout, couleurs demandent paint. Panel subtree conserve son state local puisqu’il reste monté ; animation non ajoutée.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Contrat normatif Tabs/Tab/TabPanel et relation key pair dans accessibility.md. Les hooks complets/native bridges ne doivent pas être annoncés comme déjà livrés : implémentation cible étend les snapshots selon rôle fermé existant, nodes stables, inactive panels absents. Focus requests passent par Tree, pas par action key synthétique.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Le runtime partagé reste own, chaque panel observer RAII. Choisir un nouvel onglet pendant input peut cacher un panel sous capture : availability recovery libère au checkpoint. Setter observer throwing ne laisse pas pressed/capture flag actif ; callback commencé non rejoué. Unmounted subscription inactive ; state invalid get dernière valeur, aucun observer de destruction automatique.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

Binding<T>, TabsStyle, retained availability/focus, [Visibility](visibility.md). Cas : empty, all disabled, unknown/external disabled selected key, petits bounds, labels longs, key égal duplicate et equality throwing. Ne pas importer Router MyGo pour choisir un panel.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/tabs.hpp` et `src/tabs.cpp`.

tabs.hpp déclare templates key/panel adapters et types publics existants ; tabs.cpp porte noyau non template headers/panels, input/paint/layout avec adaptateurs equality/selection. widgets.hpp et list_tabs_style.hpp restent includes compatibles ; extraire Tabs depuis widgets_list_tabs.inc sans toucher ListView duplicate implementations.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `tabs_legacy_keyboard` : wrap enabled et Home/End.
- `tabs_panel_retention` : mêmes identities/state montés après switch.
- `tabs_disabled_external_key` : panel affiché sans interaction enabled.
- `tabs_unknown_empty` : aucun panel/default write/divide by zero.
- `tabs_pointer_cancel` : release hors tab, cancel et capture recovered.
- `tabs_style_invalidation` : metrics vs colors classés.
- `tabs_setter_fault` : next input fonctionne après observer throw.
- Réutiliser tests T036 list/tabs pour compatibility baseline.

Créer l’exemple public futur `examples/features/tabs.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
