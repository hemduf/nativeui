# ListView<Key>

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

## 1. Objectif et état actuel

`ListView<T>` existe en sélection unique avec items retenus, activation et ListViewStyle dans [widgets_list_tabs.inc](../include/nativeui/detail/widgets_list_tabs.inc). Le chemin [VirtualListState](../include/nativeui/virtual_list.hpp) virtualise des lignes à hauteur fixe avec overscan, exceptions focus/capture et métadonnées immutables.

MyGo `ui/list.go`, `List`, `ListState`, `layoutList`, `listHeights`, `navigate`, `reorder`, `pinHeader` : hauteurs variables, ancrage par clé, sélection multiple, typeahead, follow-end, sections fixes et réorganisation. Ce sont des extensions cibles ; elles ne sont pas déjà présentes dans NativeUI.

Version étudiée : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Les descriptions de sources indiquent le présent ; les prescriptions suivantes constituent le contrat cible.

## 2. API publique et composition

API actuelle à conserver :

```cpp
explicit ListView(Binding<std::optional<T>> selection);
explicit ListView(State<std::optional<T>>& selection);
explicit ListView(VirtualListState<T>& state);
template<class Child> ListView&& item(T key, Child&& child, bool enabled=true) &&;
ListView&& on_activate(std::function<void(const T&)> callback) &&;
ListView&& style(ListViewStyle value) &&;
Spec spec() &&;
```

Exemple existant vérifié :

```cpp
ui::State<std::optional<int>> selected{std::nullopt};
auto list = ui::ListView<int>{selected}.item(1,ui::Label{"Premier"})
    .item(2,ui::Label{"Second"}).on_activate([](const int&){});
```

Modèles communs **cibles**, déclarations uniques dans `collection_model.hpp` et noyau dans `collection_model.cpp` :

```cpp
enum class SelectionMode { Single, Multiple };
template<class Key> struct SelectionSnapshot {
    std::vector<Key> selected;
    std::optional<Key> active;
    std::optional<Key> anchor;
    bool operator==(const SelectionSnapshot&) const=default;
};
template<class Key> struct CollectionItem {
    Key key;
    std::string label;
    bool enabled=true;
    bool section_header=false;
    bool operator==(const CollectionItem&) const=default;
};
template<class Key> class Selection {
public:
    explicit Selection(Binding<SelectionSnapshot<Key>> value);
    explicit Selection(State<SelectionSnapshot<Key>>& value);
    Binding<SelectionSnapshot<Key>> binding() const;
    SelectionSnapshot<Key> snapshot() const;
    bool valid() const noexcept;
    bool set(SelectionSnapshot<Key> value);
};
struct ListRowHeights { double estimate=24.0; bool variable=true; };
```

Extensions cibles : `ListView(Selection<Key>&)`, `.selection_mode(SelectionMode)` default Single, `.on_selection_change(std::function<void(const SelectionSnapshot<Key>&)>)`, `.on_reorder(std::function<void(const std::vector<Key>&,std::optional<Key> before)>)`, `.follow_end(bool=true)`, `.typeahead(bool=true)`. VirtualListState ajoute constructeur `(Selection<Key>&, ListRowHeights, RowFactory, std::size_t overscan=2)` et opérations `scroll_to_end()`, `visible_range()` last exclusive, `at_end()`. La factory reçoit Item possédé pour l’appel ; nouvelle métadonnée section_header ajoutée sans casser le constructeur actuel Item.

Pour le chemin non virtualisé, ajout cible distinct :

```cpp
template<class Child>
ListView&& item(CollectionItem<Key> metadata, Child&& child) &&;
```

Cette surcharge possède key/label/enabled/section_header puis le Spec enfant. La surcharge historique `.item(Key, Child, bool)` reste inchangée et n’infère jamais un label depuis le rendu ou la clé : ses lignes sans métadonnées textuelles sont exclues du typeahead. Les lignes virtualisées utilisent leur `Item.name` existant. Le moteur de recherche ne construit ni inspecte les composants enfants pour obtenir ces labels.

Le constructeur actuel fixed height/State optional reste inchangé ; `replace(std::vector<Item>)` et bool de validation sont préservés. Les nouvelles collections partagent ces modèles via [widgets.md#modeles-partages](widgets.md#modeles-partages), sans prétendre qu’ils existent aujourd’hui.

`Selection::set` retourne true uniquement si le Binding valide reçoit un snapshot effectif différent ; false pour no-op ou invalidité. Il supprime les doubles clés de selected en conservant leur première occurrence, sans connaissance du dataset. active/anchor peuvent désigner une clé non selected (navigation Primary) ou absente du dataset ; le contrôleur ne peut pas les valider contre une collection. Le widget résout l’ordre dataset et l’éligibilité au moment du geste. Une notification setter qui lève peut suivre le commit autoritaire : restauration des guards puis propagation, sans replay. Le contrôleur ne prétend pas annuler le commit après callback commencé.

Toutes les signatures appartiennent au namespace `ui`. Le builder est consommé par `Spec spec() &&` ; les enfants deviennent des `Spec` possédés. Les déclarations proposées ne prétendent pas constituer une API déjà livrée. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe. Les blocs de signatures sont des fragments des membres du type décrit, pas des programmes complets ; le `Key` ou `T` correspond au paramètre du template de ce type lorsqu’il existe.

## 3. État, propriété et notifications

- Selection est un contrôleur UI de Binding copié ; le widget ne garde pas Selection* après spec.
- selected contient clés uniques, ordonnées selon ordre logique dataset ; active est curseur clavier, anchor origine d’extension.
- Afficher seulement les clés encore présentes/enabled/non-header ; unknown keys externes n’écrivent pas automatiquement le modèle. En mode Single, si selected externe contient plusieurs clés, rendre seulement la première clé éligible selon l’ordre du dataset, sans writeback ; le prochain geste canonise une sélection unique.
- Une gesture publie un snapshot canonique en un set, puis callback utilisateur seulement si changement.
- Événement externe ne déclenche pas on_selection_change de gesture ; Binding invalide interdit writes/callbacks et garde dernier état lisible.
- Clés nouvelles copiables et equality-comparable, sans exigence hash ; l’adaptateur non template assigne des tokens monotones possédés. Conserver les types supportés et l’encodage actuels du chemin fixe.
- La préparation d’une génération distingue copie de métadonnées et résolution des clés. Les clés encodables historiques (chaînes, entiers, enums) permettent un index trié O(N log N). Un type utilisateur arbitraire comparable seulement par égalité utilise un fallback de recherche linéaire : déduplication, rematching et résolution des parents peuvent coûter O(N²). Aucun hash n’est exigé et aucune promesse de préparation O(N) n’est faite pour ce fallback. Après résolution, les opérations de scroll utilisent tokens/indices préparés et gardent leurs bornes ci-dessous.
- Dataset/semantic snapshot validé avant génération ; reorder maintient tokens des clés présentes, retrait puis réinsertion crée un nouveau token.
- Aucun callback ne garde Item&, Key& ou Node* après sa pile.

Le composant, son état et ses notifications sont confinés au thread UI/main-thread. Les véritables emprunts directs historiques de `State<T>&` doivent rester vivants ; les constructeurs qui délèguent à `state.binding()` conservent le control block sûr, pas le State. Après destruction de State, `valid()` devient faux, `get()` garde la dernière valeur, `set()` est ignoré et `observe()` inactif ; il n’y a pas de notification de destruction automatique. Les objets capturés par les modèles applicatifs gardent leur propre exigence de durée de vie.

## 4. Interactions

- Click simple sélectionne un item ; modificateur Primary (Cmd macOS/Ctrl ailleurs) toggle ; Shift étend depuis anchor, Primary+Shift ajoute une plage.
- Up/Down/Home/End et PageUp/PageDown sautent disabled/headers ; Primary+arrows déplace active sans changer selected ; Primary+Space toggle l’active.
- Primary+A sélectionne tous les items éligibles en mode Multiple ; Single conserve la logique historique.
- Typeahead sur texte commité compare labels avec repli ASCII sans casse et UTF-8 non ASCII exact, buffer 700 ms, répétition d’une lettre cycle après active ; Échap efface buffer. Ce défaut cible diffère du casefold Unicode MyGo et ne suppose pas de backend Unicode absent.
- Enter/double-click active au plus une fois via on_activate ; Space historique du mono-chemin garde son activation compatible, Primary+Space multiple a priorité.
- Reorder opt-in après seuil 4 DIP : capture, aperçu et autoscroll par instance ; up appelle on_reorder avec clés dans leur ordre actuel et before key ou nullopt pour fin, l’application modifie dataset.
- Échap/PointerCancel annulent reorder sans modifier le dataset ; suppression d’une clé draggable annule l’opération.
- Lire état current à release ; read-only permet navigation mais refuse writes/reorder. Enfants interactifs gardent priorité sur selection si leur event est consommé.

Le routage respecte disponibilité héritée, clipping et capture du runtime. Aucun raccourci global ni accès aux paramètres audio ne doit être ajouté pour ce composant.

## 5. Mesure et layout

- Fixed actuel : hauteur float positive finie, content height validée avant replace ; les mêmes formules arithmétiques restent disponibles.
- Variable cible : double cumuls et cache de hauteur par clé + largeur de mesure ; estimation strictement positive avant mesure réelle.
- Conserver ancre `{key,inset double}` pour que des mesures/insertion au-dessus ne déplacent pas la première ligne visible.
- Si ancre supprimée, choisir prochain survivant dans l’ancien ordre, puis précédent, sinon début/fin selon follow policy.
- Matérialiser viewport + overscan 2 de chaque côté et lignes exceptions focus/capture ; pas O(N) Components.
- Index prefix-sums maintenu par dataset/hauteur ; scroll ordinaire O(log N + V), pas full scan de clés à chaque frame.
- Hauteur zéro/non finie renvoyée par factory : minimum de 1 DIP pour borne de progression et diagnostic ; max 4096 matérialisations par passe pour éviter boucle sans espace.
- Largeur changée invalide heights mesurées mais garde l’ancre ; convergence de mesure bornée, remplir viewport avant publication sans frame trouée.
- Follow-end : actif seulement si utilisateur était à fin (tolérance 1 DIP), nouveau contenu garde fin ; scroll away suspend, retour fin reprend.
- Section header sticky détaché visuellement sans second item logique ; prochain header pousse le précédent, le hit/clip restent cohérents.

Les tailles et positions sont des coordonnées logiques NativeUI. Le backend effectue la conversion de facteur d’échelle une seule fois ; le composant ne manipule aucune coordonnée écran native.

## 6. Présentation et invalidation

ListViewStyle existant conserve ses champs et default resolver. Extensions de style (sticky header, insertion indicator) explicites dans ce même type ou options versionnées sans nouveaux slots Theme supposés. Selected/active/focus sont distincts ; ne pas colorer disabled comme choisi par un snapshot invalide. Offset seul demande paint/materialisation nécessaire, pas recopie O(N) des métadonnées. Cache owned par instance.

L’invalidation distingue changement de pixels et changement de métriques. Un état effectif identique est un no-op ; le composant ne force pas un repaint de toute la fenêtre lorsque ses limites suffisent.

## 7. Accessibilité

Le contrat normatif cible ListView/ListItem est [accessibility.md](accessibility.md) ; les headers actuels donnent metadata virtuelle mais les hooks complets/rôles natifs ne sont pas livrés. Garder l’invariant `VirtualSemanticChildren::item_at` sans factory ni mutation. Variable height/multiselection nécessitent une extension **nouvelle** du snapshot : shared immutable geometry/index et selected tokens, sans changer silencieusement la géométrie fixed existante. Lectures d’une ancienne génération restent cohérentes, removed tokens defunct.

Le contrat utilise les hooks et snapshots backend-neutres de [semantics.hpp](../include/nativeui/semantics.hpp) et [accessibility.md](accessibility.md). Les ponts natifs relèvent de T068, différé ; cette spécification ne valide ni VoiceOver, ni UIA, ni AT-SPI. Tout rôle absent de l’enum actuel nécessite une extension distincte ; jusque-là employer `None`, `Group` ou `Custom` selon le cas.

## 8. Cycle de vie et récupération

Replace prépare clés/metadata/heights/token mapping avant commit ; validation false laisse l’ancienne génération intacte. Factory/mount failure n’avance pas des tokens visibles ou indices de capture avant commit. Après commit, la refresh nécessaire est durablement dirty si elle lève. Un reorder callback peut modifier/remplacer le dataset ; garder keys/generation owned et revalider avant accès suivant. Arrêter typeahead timer/autoscroll en caché/unmount ; rien ne migre vers une autre instance.

Les abonnements, invalidateurs et captures sont propres à chaque instance et libérés par RAII. Un callback commencé qui lève n’est jamais rejoué automatiquement ; les invariants sont restaurés avant propagation C++. Le démontage est no-throw et ne déclenche pas de callback applicatif de destruction. La destruction du propriétaire UI depuis une pile de callback doit être différée au checkpoint sûr existant.

## 9. Dépendances et cas limites

[ScrollView](scroll_view.md), dynamic reconciliation, ListViewStyle, Semantic snapshots et modèles communs cibles. Item keys duplicate : reject avant commit ; fixed height invalid ctor invalid_argument, geometry overflow replace false. Dataset vide = no active, aucune activation, offset zero. Scroll key absente/index hors plage = false sans changement. Focus row hors viewport est conservée temporairement selon exception rows ; limite/recovery quand retirée est explicite.

Les touches `Key::PageUp` et `Key::PageDown` sont des additions cibles en fin de l’enum portable actuel, avec traduction plateforme et tests. Le source actuel ne les définit pas. Typeahead utilise les InputEvent de texte commité ; aucun support natif complet IME n’est supposé.

Réutiliser les services `Tree`, focus, disponibilités et invalidation ; ne pas en créer de copies locales concurrentes. Les limites d’IME restent celles de [DESIGN.md §17.4](../DESIGN.md) : le transport natif du texte commité est disponible ; le transport complet de préédition/IME et des rectangles de candidats est différé. Ne pas assimiler cette limite plateforme à une impossibilité de tester les modèles textuels en headless.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/list_view.hpp` et `src/list_view.cpp`.

list_view.hpp/cpp portent builder, state/controller declarations et noyau retained non template de liste. virtual_list.hpp reste include compatible et VirtualListState public avec ses aliases Item/Runtime/MetadataSnapshot historiques préservés ou adaptateurs compatibles. Les templates Key/RowFactory adaptent owned keys/Spec vers type erasure ; pas d’instanciations int/string seulement. Extraction retire les implémentations de widgets_list_tabs.inc et detail virtual list là où remplacées, sans casser includes list_tabs_style.hpp ni widgets.hpp.

Le header contient déclarations et seuls adaptateurs templates nécessaires. Le `.cpp` doit porter un véritable noyau retenu, de mesure/layout et, si applicable, d’entrée/paint ; aucun fichier vide ni switch central de widgets. Ajouter ce `.cpp` à `NativeUI::Core` lors de l’implémentation. Aucun type Pugl, Skia, OS, plugin ou automation n’entre dans les signatures publiques.

## 11. Tests et critères d’acceptation

- `list_fixed_legacy` : API actuelle, fixed geometry, overscan, Space/Enter et duplicate .item.
- `list_variable_anchor` : mesure hauteur/insertion/reorder au-dessus sans saut visuel.
- `list_variable_resize` : width change remeasure avec same key/inset.
- `list_multiple_ranges` : modifiers, active/anchor, skip disabled/headers, Primary+A.
- `list_reorder_keys` : before nullopt fin, callback unique, cancel/removal sous drag.
- `list_follow_end` : append/height growth à fin, scroll away, retour fin.
- `list_sticky_header` : push-off et une seule identity logique.
- `list_semantic_generation` : metadata pointer stable hors dataset change ; tokens non réutilisés.
- `list_snapshot_no_factory` : lectures sémantiques jamais app callbacks.
- `list_replace_faults` : validation/allocation/factory/mount failures et refresh après commit.
- `list_zero_height_bound` : pas de matérialisation infinie.
- `list_invalid_binding` : aucun write/gesture callback après invalidation.
- `list_eager_typeahead_metadata` : seules les lignes avec metadata.label ou Item.name participent ; la surcharge historique reste compilable et sans inspection du Spec.
- `list_single_external_multiple` : première clé éligible du dataset visible, aucune correction externe automatique.
- `list_generic_equality_fallback` : clé utilisateur non hashable admise, coût de préparation explicitement distinct du scroll.
- `list_million_rows` : clés entières encodables, Comp counts O(V+overscan+exceptions), scroll sans full scan.

Créer l’exemple public futur `examples/features/list_view.cpp` ; `--self-test` exécute les assertions propres à cette page puis quitte sans interaction manuelle. Compléter par rendu headless des états pertinents, destruction/remontage, deux instances simultanées et injection d’exception aux frontières applicatives.

Ces tests sont à implémenter avec le composant. La rédaction présente vérifie sources, signatures et liens ; elle ne rapporte aucune exécution de ces tests. Acceptation : tous les cas nommés passent, aucune régression des API historiques, aucune dépendance globale mutable et aucun warning NativeUI.
