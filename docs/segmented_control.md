# SegmentedControl<T>

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Choisir une valeur exclusive parmi des segments : mode Liste/Grille ou plage de vue. Le groupe
conserve une sélection applicative stable, pas un index imposé au montage.

NativeUI propose RadioGroup<T>/RadioButton<T> dans
[widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc), mais pas de
SegmentedControl visuel.

MyGo : `ui/toggle.go`, `SegmentedBase`, `Segment`, `Segmented`. MyGo borne son index lors de
construction ; cible NativeUI utilise valeurs T et ne réécrit jamais l’état externe simplement pour
rendre.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
template<class T>
struct SegmentOption {
  T value;
  std::string label;
  bool enabled{true};
};
template<class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class SegmentedControl {
public:
  SegmentedControl(std::string label, Binding<T> selected,
                   std::vector<SegmentOption<T>> options);
  SegmentedControl(std::string label, State<T>& selected,
                   std::vector<SegmentOption<T>> options);
  SegmentedControl&& style(SegmentedControlStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<int> mode{0};
auto mode_picker = ui::SegmentedControl<int>(
    "Vue", mode, {{0, "Liste", true}, {1, "Grille", true}}).spec();
```

SegmentedControlStyle cible : track padding/gap/radius/border, ButtonStyle segment et patch
selected. T n’est pas limité à int/string ; les adaptateurs typed value/equals/set/observe sont
effacés pour le noyau non template.

Les variantes avec icônes restent des options du même composant ; elles peuvent être ajoutées via
une fabrique de contenu par option, en gardant label pour le nom, sans composant Segment public
autonome.

## 3. État, propriété et notifications

Binding<T> est la sélection ; options T sont possédées et comparées par égalité. Une valeur hors
options n’est pas corrigée au montage : aucun segment selected, mais le premier disponible peut
recevoir focus.

Le vector options constitue un snapshot immuable de génération ; remplacer les options nécessite
nouvelle Spec au checkpoint, sans setter d’options en place dans cette v1. Une valeur sélectionnée
disabled reste visiblement selected mais ne peut être choisie par l’utilisateur. La sélection
externement modifiée déplace la cible d’entrée Tab, sans voler le focus déjà dans un autre contrôle.

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

Clic au relâchement sur un segment enabled sélectionne sa valeur ; sélectionner la valeur courante
ne republie pas. PointerCancel annule l’action.

Tab est un arrêt sur le segment sélectionné disponible, sinon dernier actif/ premier disponible.
Gauche/Droite ou Haut/Bas, Home/End déplacent focus et sélectionnent en même temps ; bouclage
flèches et saut disabled.

Espace sélectionne au relâchement et Entrée à la première pression ; répétitions d’activation
supprimées. ReadOnly laisse lire/focus mais flèches ne publient aucune sélection.

Molette ignore ; glisser hors du segment annule, sans transformer le contrôle en slider. Réduction
de largeur ne modifie pas le choix.

## 5. Mesure et layout

Track horizontal ; largeur naturelle somme des segments et paddings. Hauteur commune égale la plus
grande mesure enfant ; conserver largeur variable par label plutôt qu’imposer texte tronqué
uniforme.

Un espace étroit clippe visuellement selon parent ; chaque hit-test utilise le rectangle réellement
arrangé, aucun choix de segment invisible. Pas d’overflow implicite.

Sélection constante en taille à style identique ; un patch selected à métrique différente invalide
layout explicitement.

## 6. Présentation et invalidation

Selected a une face distincte au sein du track ; pressed momentané et selected persistante sont deux
états. Focus reste visible sur le segment actif.

Styles proviennent du thème et du style du composant ; aucune mutation du ButtonStyle de widgets
voisins. SegmentOption.label et icône utilisent les mêmes couleurs résolues.

Changement de sélection invalide les deux faces affectées, pas la collection entière si métriques
inchangées.

## 7. Accessibilité

Cible : Group nommé, enfants RadioButton avec selected/checked et action Select quand mutables.
SemanticRole actuel ne contient pas RadioGroup : ne pas annoncer ce rôle comme livré.

Un seul segment selected par égalité ; option désactivée reste exposée enabled=false. Valeur
inconnue laisse tous les enfants non selected.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Le noyau non template possède les segments retenus et les adaptateurs par valeur ; aucune référence
T à une ligne temporaire.

Commit sélection après terminaison de capture et état focus cohérent ; observer retirant le groupe
ou levant ne doit pas provoquer un second set ni un accès postérieur à un enfant détruit.

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

Dépendances : [radio_button](radio_button.md), [toggle_group](toggle_group.md) pour présentation,
focus commun, State/ThemeBinding.

Valeurs d’options dupliquées interdites : invalid_argument avant publication de Spec, sinon identité
de choix ambiguë. Labels dupliqués admis si les valeurs diffèrent.

Options vides/all disabled : aucun choix utilisateur ; valeur externe préservée. Après remplacement
des options, une valeur retirée reste dans le Binding et perd seulement sa face selected.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/segmented_control.hpp` et `src/segmented_control.cpp`. Le header expose
les déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter
un véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

SegmentOption<T>, constructeurs State/Binding et éventuels guides de déduction restent en header ;
selected/observe/select sont des adaptateurs templates. Le .cpp contient la machine segmentée non
template.

Le contrôleur RadioGroup<T> existant reste compatible ; SegmentedControl ne rebaptise ni Toggle ni
Switch<T>.

Inscrire `src/segmented_control.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les
includes collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou
plugin dans l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`segmented_control_selection` : clic et navigation sélectionnent une seule valeur ; choix identique
sans notification.

`segmented_control_unknown_disabled` : état inconnu ou disabled n’est pas réécrit par mount/paint.

`segmented_control_empty_duplicates` : vide/all disabled sans focus mutant ; valeurs dupliquées
rejetées.

`segmented_control_remove_option` : option retirée pendant appui ne reçoit pas un commit périmé.

`segmented_control_generic_type` : type utilisateur copiable/égalité compile avec noyau .cpp, sans
instanciation listée.

`segmented_control_observer_throw` : retrait/réentrance et exception d’observer laissent prochain
choix possible.

Ajouter `examples/features/segmented_control.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
