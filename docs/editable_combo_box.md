# EditableComboBox

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Choisir une chaîne dans une liste fermée en tapant un filtre. Le texte draft n’est pas la sélection
persistante tant qu’une option valide n’a pas été choisie.

NativeUI ComboBox<T> correspond à Select, sans éditeur ; TextInput/overlays fournissent les
fondations. Aucun EditableComboBox public.

MyGo : `ui/combobox.go`, `Combobox`, `ComboboxBase`, `comboboxBase`, `matching`. Filtre, chevron
d’ouverture, choix et retour à selected à blur sont le contrat source repris.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class EditableComboBox {
public:
  using OptionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view candidate, std::string_view query)>;
  EditableComboBox(std::string label, Binding<std::string> selected,
                   std::vector<std::string> options);
  EditableComboBox(std::string label, State<std::string>& selected,
                   std::vector<std::string> options);
  EditableComboBox(std::string label, Binding<std::string> selected, OptionsProvider options);
  EditableComboBox(std::string label, State<std::string>& selected, OptionsProvider options);
  EditableComboBox&& filter(Filter value) &&;
  EditableComboBox&& placeholder(std::string value) &&;
  EditableComboBox&& style(EditableComboBoxStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<std::string> font{"Inter"};
auto selector = ui::EditableComboBox("Police", font,
    std::vector<std::string>{"Inter", "Georgia", "Menlo"}).spec();
```

Style cible : TextInputStyle, ComboBoxStyle pour cadre/chevron, MenuItemStyle popup. v1 est string
selon MyGo ; sélectionner un objet typed reste ComboBox<T>.

Filtre par défaut : trim ASCII du query, comparaison ASCII case-insensitive, UTF-8 non ASCII
préservé exact ; préfixes d’abord puis substring, ordre source stable. Différence explicite avec
strings.ToLower Unicode de MyGo ; filter injecté permet politique Unicode applicative.

Filter personnalisé décide inclusion, garde ordre provider ; aucune promesse ICU ou filtrage
asynchrone. Provider appelé à ouverture puis après chaque edit commis qui doit rafraîchir la liste,
jamais paint.

## 3. État, propriété et notifications

Binding<string> est sélection ; draft, typed/filtering, highlighted et session overlay locaux.
Afficher selection au repos, même si la chaîne a été retirée de la liste.

Frappe modifie seulement draft et suggestions. Le premier match est surligné ; navigation ne publie
pas. Choix d’un match remplace selected, formate draft et sélectionne tout le texte.

Blur ou Escape abandonne le draft non choisi et affiche sélection actuelle sans set. Écriture
externe selected pendant edit remplace draft et ferme session, avec priorité à l’application.

Snapshot de suggestions est possédé et génération identifié. Un résultat provider pour une ancienne
génération ne peut pas être commis sur la requête nouvelle.

Les surcharges State<T>& sont converties en Binding et ne gardent pas un emprunt brut. Après
destruction du State, Binding::valid() devient false, get() conserve la dernière valeur lisible,
set() est ignoré et observe() reste inactive. Aucune notification implicite de destruction :
vérifier valid à chaque dispatch/checkpoint pour couper mutation et callbacks utilisateur du modèle
disparu. Les modèles applicatifs capturés par une fermeture ne sont pas prolongés par Binding.

Une observation externe invalide la présentation sans simuler de geste utilisateur. Notifications
State synchrones : snapshot stable, ajouts au passage suivant, retraits sautés et écritures
récursives coalescées. Après exception, la valeur publiée demeure, les notifications du passage
restant sont interrompues et le dispatch doit être réutilisable.

## 4. Interactions

Chevon/clic ouvre liste complète si aucun filtre typed ; frappe ouvre liste filtrée. Bas/Haut
ouvrent puis naviguent sans bouclage en dépassant les extrémités ; Home/End restent navigation texte
sauf commandes popup explicites.

Enter avec surligné choisit ; Enter sans match ne crée pas une valeur libre. Clic item au
relâchement choisit. Escape ferme/restore ; Tab ferme/restore et poursuit focus.

L’éditeur garde focus pendant navigation popup ; chevron et lignes ne deviennent pas des arrêts Tab
supplémentaires. PointerCancel cesse armement item sans choix.

ReadOnly empêche édition/ouverture mutante et garde lecture ; disabled bloque. Molette défile
seulement panneau si ouvert ; aucune sélection spontanée sur champ fermé.

Composition Update ne filtre ni sélectionne ; Commit déclenche une seule génération query et
déduplication committed text. Pont IME natif avancé distinct.

## 5. Mesure et layout

Editor flex et chevron fixe dans un cadre commun ; taille préférée stable de champ, indépendamment
du nombre de matches. Long draft scroll horizontal.

Popup ancré à tout le cadre, borné viewport via OverlaySpec/OverlayHandle et service overlay retenu.
Hauteur plafonnée à 8 lignes puis scrolling ; pas de virtualisation claim pour v1.

Les lignes ont hauteur fixe issue MenuItemStyle et width au moins cadre, élargie pour texte dans
viewport. Empty results : panneau “Aucun résultat” non sélectionnable.

## 6. Présentation et invalidation

États focus/hover/read_only de champ plus highlighted du popup. Le selected applicatif n’est pas mis
à jour pour preview ; aucun design source imprimant un choix déjà validé alors que seul un filtre
est saisi.

Query change recalcule filtre au checkpoint UI puis paint/layout popup ; couleur de highlighted
paint only. Les handlers de texte restent dans noyau partagé.

Chevrons/primitives ne requièrent aucun shader/ressource spécifique public. Pas d’appel
provider/filter depuis semantics native immutable.

## 7. Accessibilité

Cible : ComboBox avec text_value draft, expanded et nom label ; options ListItem,
highlighted/selected distingués.

La sélection applicative apparaît en description/value au repos ; ReadOnly supprime
Expand/Select/SetValue. Le message vide est Text sans action.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer nouvelle liste filtrée avant publier génération ; si provider/filter lève, fermer le
panneau, conserver draft, retirer toute activation de l’ancien snapshot et restaurer flags avant
propagation. La prochaine ouverture pourra réessayer ; aucune option ancienne ne reste choisissable.

Commit ferme le popup puis prépare Binding et string possédés ; set peut retirer le champ. Aucune
relecture de draft/this après publication.

Rejet ouverture différée : rester fermé avec draft conservé, continuer édition ; un handle périmé
devient no-op. Clipboard et callbacks filter portent tokens sûrs.

Tout état et routage restent confinés au thread UI/main. Les abonnements et captures sont libérés
par instance ; aucun registre mutable global ne transporte les interactions.

Les callbacks sont possédés et copiés avant appel. Restaurer captures, drapeaux et identité avant de
publier une valeur ou appeler l’application. Un callback commencé qui lève ne sera jamais rejoué ;
les exceptions C++ directes peuvent repartir après restauration des invariants.

La suppression d’un sous-arbre suit la réconciliation sûre. La destruction du propriétaire UI/window
depuis un callback doit passer par un point sûr différé ; aucune sécurité de destruction synchrone
du propriétaire n’est promise.

Destruction et démontage sont no-throw. Les invalidateurs différés portent un jeton faible de
propriétaire et une identité monotone ; après retrait ils deviennent inopérants, sans retenir un
Node ou contexte emprunté.

## 9. Dépendances et cas limites

Dépendances : [text_input](text_input.md), [combo_box](combo_box.md), [popup_menu](popup_menu.md),
Overlay et State. Noyau suggestions privé partagé avec Autocomplete/TokenField.

Options dupliquées exactes dédupliquées au premier passage pour identité text stable ; chaîne vide
admise comme option explicite, distincte de placeholder.

Sélection inconnue ou options vides ne changent pas Binding ; aucun auto-select mount. Option
retirée entre générations annule son appui avant nouveau snapshot ; filter invalide ne fournit pas
un nouveau choix.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/editable_combo_box.hpp` et `src/editable_combo_box.cpp`. Le header expose
les déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter
un véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Les alias Filter/OptionsProvider et EditableComboBoxStyle restent dans le couple. Le .cpp porte
draft, filtrage, snapshot/génération, input, layout et popup via noyau partagé.

Ne pas ajouter ces fonctionnalités à ComboBox<T> en modifiant silencieusement son sens ; les anciens
includes et l’API sélection seule restent intacts.

Inscrire `src/editable_combo_box.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les
includes collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou
plugin dans l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`editable_combo_box_filter` : préfixes avant substring, ASCII case fold/Unicode exact et filter
injecté documentés.

`editable_combo_box_draft` : édition ne set pas selected ; Enter avec match choisit, sans match
reste sans choix.

`editable_combo_box_restore_external` : blur/Escape restore selected actuelle ; écriture externe
gagne et ferme popup.

`editable_combo_box_duplicates_empty` : dédup options et empty state non selectable ; aucune valeur
par défaut.

`editable_combo_box_composition` : Update ne filtre pas, Commit une génération ; pas double commit.

`editable_combo_box_stale_failure` : provider/filter/enqueue lèvent ou option retirée pendant appui
: pas choix périmé.

`editable_combo_box_popup_focus` : focus reste éditeur, resize/scroll popup stable et Tab sort.

Ajouter `examples/features/editable_combo_box.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
