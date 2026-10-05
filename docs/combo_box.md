# ComboBox<T>

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Sélectionner une valeur exclusive dans une liste d’options. Le contrôle ne permet pas de taper ni de
filtrer ; EditableComboBox est le composant dédié à cette fonction.

NativeUI : [combo_popup.hpp](../include/nativeui/combo_popup.hpp), ComboBoxOption<T>, ComboBox<T> et
noyaux d’ancre/popup templatisés. Sélection, provider et snapshot de session sont déjà présents.

MyGo : `ui/widgets.go`, `Select` ; `ui/base.go`, `SelectBase[T]`, `SelectParts`. Cette
correspondance avec Select évite de prétendre que la fonction MyGo Combobox éditable est déjà
portée.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
template<class T>
struct ComboBoxOption { T value; std::string label; bool enabled{true}; };
template<class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class ComboBox {
public:
  using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>() >;
  ComboBox(Binding<T> selection, std::vector<ComboBoxOption<T>> options);
  ComboBox(State<T>& selection, std::vector<ComboBoxOption<T>> options);
  ComboBox(Binding<T> selection, OptionsProvider provider);
  ComboBox(State<T>& selection, OptionsProvider provider);
  ComboBox&& placeholder(std::string value) &&;
  ComboBox&& style(ComboBoxStyle value) &&;
  ComboBox&& item_style(MenuItemStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<int> quality{1};
auto quality_box = ui::ComboBox<int>(quality,
    std::vector<ui::ComboBoxOption<int>>{{1, "Normal", true}, {2, "Élevé", true}})
    .placeholder("Aucune qualité").spec();
```

Conserver exactement contraintes, surcharges et types de style. Le provider actuel est appelé une
fois par le constructeur pour initial_options, puis lors ouverture ; aucun appel pendant paint.

Les adapters typed T restent en header ; le futur .cpp reçoit indices opaques et closures
copy/get/set/equals/observe nécessaires, sans imposer un variant de types autorisés.

## 3. État, propriété et notifications

Binding<T> possède la sélection. Options du bouton et snapshot de popup sont possédés. Le label
affiché est trouvé dans display_options ; valeur inconnue affiche placeholder sans écrire.

Le popup initialement surligne la sélection enabled si présente, sinon la première enabled. Déplacer
le surligné ne publie pas la sélection ; commit uniquement lors choix.

Une ouverture rafraîchit le snapshot du provider et met déjà display_options à jour avant commande
overlay, conformément à la source ; annuler ne remet pas les anciens labels et ne change pas la
sélection.

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

Clic au relâchement, Entrée première pression, Espace relâché ou Bas ouvrent. La touche d’ouverture
reste supprimée jusqu’au KeyUp pour éviter un choix accidentel.

Popup : Haut/Bas circulent sur enabled, Home/End premier/dernier ; Entrée/Espace valident. Clic item
enabled choisit au relâchement ; séparateur inexistant dans ComboBoxOption.

Échap, Tab et clic extérieur ferment ; focus retourne à l’ancre ou suit le focus runtime selon
raison. ReadOnly ferme/blocage du popup et empêche sélection ; Disabled retire interaction.

Molette du bouton ne change pas la sélection. Aucun typeahead implicite ni édition TextInput : ces
comportements nécessitent EditableComboBox.

## 5. Mesure et layout

Bouton mesure label courant + horizontal_padding deux fois, minimum_width et control_height du
ComboBoxStyle ; aucune largeur maximum globale calculée par provider en paint.

Popup mesure options du snapshot en coordonnées logiques ; service overlay retenu
(OverlaySpec/OverlayHandle, detail::OverlayService) ancre sous le bouton et borne le viewport. Les
très longues listes demandent un panneau scrollable lors enrichissement futur.

Conserver géométrie et nombre de composants actuels pendant extraction ; ne pas proclamer
virtualisation livrée. Changement de sélection observe et invalide layout/paint car label peut
changer de largeur.

## 6. Présentation et invalidation

ComboBoxStyle pour l’ancre, MenuItemStyle pour lignes. Surligné local et selected affiché sont deux
notions ; choix externe n’est pas une simulation de clic.

Style de ligne modifiant row_height demande remesure du panneau ; survol couleur seule demande
paint. Les options disabled ont une apparence et une action distinctes.

Provider nul produit liste vide. La police et les caractères suivent TextService, avec réparation
UTF-8 commune.

## 7. Accessibilité

Cible : ComboBox, name/label accessible fourni par composition, text_value=label courant, expanded ;
popup et items sélectionnables nommés.

Le fichier actuel ne contient pas d’override semantics dans les noyaux ComboBox. Les IDs de session
sont backend-neutres ; options T ne doivent pas fuir sous forme de pointeurs d’application vers un
pont natif.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer snapshot/session avant publier commande overlay ; échec provider/construction doit laisser
l’ancre fermée et réutilisable. suppress_until_key_up est restauré si aucun handle validé.

Fermer logiquement le popup puis copier Binding/valeur avant set. Observer retire l’ancre : vérifier
token et ne pas effectuer un accès this après publication.

Démontage désactive runtime et subscriptions, supprime commandes pending et ferme overlay. Commande
tardive devient safe no-op ; rejet de queue ne force pas une ouverture synchrone.

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

Dépendances : State, OverlayCommandSource, OverlayAnchorPolicy, styles et ThemeBinding ;
[popup_menu](popup_menu.md) partage l’infrastructure panneau non template.

Liste vide ou all disabled : ouvrir un panneau sans élément sélectionnable selon comportement actuel
; Échap/Tab le ferment. Le Binding n’est jamais remplacé par première option.

La source ne rejette pas des valeurs T dupliquées : préserver ce comportement à extraction, premier
label correspondant affiché ; ne pas introduire silencieusement une validation nouvelle.

Retrait d’option après ouverture ne change pas le snapshot de session. Une action externe retirant
ancre ferme la session ; aucune identité d’index d’un nouveau snapshot ne peut recevoir le choix
ancien.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/combo_box.hpp` et `src/combo_box.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Conserver combo_popup.hpp comme façade historique. ComboBoxOption<T>/ComboBox<T> et signatures
templates restent dans combo_box.hpp ; effacer uniquement le noyau retained/popup vers
combo_box.cpp.

Pas de liste explicite d’instantiations int/string : les closures typed couvrent les types
utilisateurs. La synchronisation des display_options et la session sont prises en charge par le
noyau propriétaire.

Inscrire `src/combo_box.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`combo_box_unknown` : valeur inconnue affiche placeholder sans set au mount ou paint.

`combo_box_provider` : appel initial et ouverture vérifiés ; aucune exécution depuis paint.

`combo_box_choice_cancel` : navigation sans write ; validation une write ; Échap/clic extérieur
aucune.

`combo_box_disabled_empty` : skip disabled et panneau vide fermable sans sélection inventée.

`combo_box_typed_core` : T utilisateur avec copy/equals compile vers noyau .cpp ; signatures
historiques conservées.

`combo_box_failure_remove` : provider/enqueue/observer lèvent ou retirent ancre ; session et key
suppression récupèrent.

Ajouter `examples/features/combo_box.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
