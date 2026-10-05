# EditableText

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Afficher un texte puis l’éditer sur demande, comme un nom de fichier. Le draft est privé jusqu’à
acceptation ; Escape restaure l’affichage de dernière valeur applicative.

NativeUI fournit Label/TextInput et collections retenues, mais aucun EditableText. Intégration aux
lignes utilise clés stables et commandes explicites, sans retrouver un Node par label.

MyGo : `ui/editable.go`, `EditableText`, `editState` et délai renameDelay=500 ms. Standalone double
clic/Enter ; dans liste, demande F2/Enter ou clic lent sur ligne choisie, Enter/blur commit et
Escape annule.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class EditableTextController {
public:
  void begin();
  void accept();
  void cancel();
  bool editing() const noexcept;
};
class EditableText {
public:
  using Validator = std::function<std::optional<std::string>(std::string_view)>;
  EditableText(std::string label, Binding<std::string> value);
  EditableText(std::string label, State<std::string>& value);
  EditableText&& controller(std::shared_ptr<EditableTextController> value) &&;
  EditableText&& select_stem(bool value = true) &&;
  EditableText&& validator(Validator value) &&;
  EditableText&& style(EditableTextStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<std::string> file{"Preset.oreto"};
auto rename = ui::EditableText("Nom du preset", file).select_stem().spec();
```

Controller facultatif possédé via shared_ptr, lié à une seule instance montée ; réemploi simultané
rejeté. Sans controller, le widget en possède un privé. begin/accept/cancel sont des demandes UI
différées au checkpoint sûr ; editing indique phase commise.

Defaults : select_stem=false (texte générique), validator absent, label utilisé comme nom
accessible. select_stem(true) sélectionne avant dernier point non initial, comme MyGo pour fichiers.

Validator retourne nullopt si valide, sinon message possédé. EditableTextStyle contient TextStyle
lecture, TextInputStyle édition, et message invalid. Aucun composant rename distinct.

## 3. État, propriété et notifications

Binding<string> est texte validé ; draft, editing, just_started et error sont locaux. begin snapshot
current value et demande focus/sélection ; frappe ne set pas la valeur.

Accept valide draft et publie seulement s’il diffère ; Cancel abandonne draft et montre valeur
courante sans set. Une valeur externe pendant editing gagne : remplacer draft/baseline et conserver
ou réappliquer sélection stem.

Enter validation invalid maintient édition/error ; blur invalid annule draft et revient lecture sans
voler le focus. Blur valid commit mais ne restaure pas focus à l’éditeur ; validation n’est jamais
appelée en destruction.

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

Standalone : double clic ou Enter passe en édition ; simple clic donne focus. En édition Enter
accept, Escape cancel, Tab/blur accept valide puis poursuit focus.

Dans collection : controller begin depuis commande sur clé choisie ; intégration permet clic lent
500 ms sur texte déjà choisi, annulé par deuxième clic/doubleclic, sélection différente, scroll ou
retrait.

F2 est une extension input ciblée : Key ne le contient pas actuellement ; l’ajouter en fin enum et
traduire plateforme, sans changer valeurs préexistantes. La collection appelle controller begin avec
son propre ID de ligne.

Au départ select_stem choisit UTF-8 avant dernier “.” si non initial ; fichier sans suffixe
sélectionne tout. Édition clipboard/selection suit TextInput, mais Enter/Escape/blur politique
locale remplace snapshot restore.

ReadOnly bloque begin/accept mutation tout en montrant texte ; Disabled bloque focus. PointerCancel
annule déclenchement lent. Molette n’édite pas et annule timer de rename si scroll réel du parent.

## 5. Mesure et layout

En lecture, taille du texte sous contraintes parent ; en édition, éditeur remplace visuellement le
texte dans la même aire avec bordure/padding compensés, sans déplacer soudain la ligne.

Long draft utilise scroll horizontal TextInput ; la largeur de la cellule collection prime sur
longueur du nouveau nom. Hauteur minimum commune aux deux phases.

Erreur se place dans une zone réservée/description, sans agrandir arbitrairement toutes les lignes
d’une table. Les coordonnées logiques de la clé éditée sont recalculées au checkpoint.

## 6. Présentation et invalidation

Lecture TextStyle ; édition border accent et caret/selection. Ring visible sur texte focusé en
lecture, puis sur éditeur en édition, jamais deux focus visibles.

Passage phase invalide layout si la hauteur effective diffère, sinon paint/structure ; nouveau nom
accepté remesure texte. Timer lent arrête demande frames dès annulation.

Validator et actions ne sont pas appelés dans paint ; error snapshot possédé. Le parent ne remount
pas toute la table sur chaque frappe du draft.

## 7. Accessibilité

Cible lecture : Text, focusable et action Activate “Modifier” quand mutable ; phase édition
TextInput et text_value draft. Le nom reste le label, pas uniquement le nom de fichier.

Annonce de phase/error via snapshot description backend-neutre. Le controller n’est pas un nœud
sémantique ; aucun pointeur de collection conservé par un pont.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Controller garde faible token de propriétaire et identité monotone ; commande après démontage
devient no-op, ne garde pas le widget vivant.

Avant validator, prendre draft/callback snapshot sans this emprunté survivant à appel réentrant ; si
generation change ou widget retiré, le résultat est ignoré. Exception de validator garde draft
editing, flags restaurés, prochaine accept possible.

Après validation réussie, phase lecture/focus policy sont cohérentes avant Binding.set ; observer
peut retirer la ligne. Destruction ne commit pas le draft et ne rejoue aucune validation.

Rejet d’enqueue du controller conserve une demande durable pour le checkpoint du propriétaire, sans
exécution synchrone risquée. Controller réattaché à un remplaçant ne reprend pas demandes anciennes.

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

Dépendances : [label](label.md), [text_input](text_input.md), Focus/Dispatcher et collection keyed
([list_view](list_view.md), [table_view](table_view.md)). Intégration n’importe pas Router MyGo.

Les demandes du controller nécessitent le Dispatcher du propriétaire actif pour exécuter
begin/accept/cancel. Sans Dispatcher, elles restent pending et peuvent être postées lors d’une
activation suivante qui fournit cette capacité ; aucun fallback synchrone n’est permis. Un post
rejeté conserve la demande jusqu’au prochain checkpoint retenu, qui réessaie seulement l’enqueue.
La désactivation annule les demandes acceptées de l’ancienne activation ; elles ne peuvent pas
s’exécuter sur un nouveau propriétaire Dispatcher.

Binding invalid coupe toutes demandes et reste lecture dernier snapshot ; nom vide autorisé si
validator le permet. Points initiaux “.profile”, plusieurs extensions et UTF-8 ne doivent pas casser
sélection stem.

Ligne retirée/réordonnée pendant timer : rechercher key stable et génération, annuler si différente.
Doubleclic reste action submit de la collection quand intégrée : ne lancer ni rename ni deux
callbacks.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/editable_text.hpp` et `src/editable_text.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

EditableTextController, Validator et EditableTextStyle sont décrits/déclarés dans le même header ;
editable_text.cpp possède phases, timer, commandes, validation et noyaux lecture/édition.

Les requêtes du controller passent une interface privée backend-neutre ; l’extension Key F2 respecte
ABI des enumerators existants.

Inscrire `src/editable_text.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les
includes collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou
plugin dans l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`editable_text_phases` : begin draft sans set ; Enter/blur valide commit une fois ; Escape conserve
modèle actuel.

`editable_text_validator` : Return invalid reste editing, blur invalid annule ; validator réentrant/
levant récupère.

`editable_text_stem` : suffixe, aucun point, point initial, multiple points et UTF-8 donnent offsets
exacts.

`editable_text_external` : écriture externe pendant edit gagne et devient nouveau texte.

`editable_text_row_delay` : 500 ms controlled ; doubleclick/reorder/removal/scroll annulent rename
périmé.

`editable_text_controller` : commande stale no-op et enqueue rejeté ne passe pas sync ; un
controller une instance.

`editable_text_destroy_draft` : démontage/destroy aucun commit/validation, deux instances isolées.

Ajouter `examples/features/editable_text.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
