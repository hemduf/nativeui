# CheckboxGroup

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Composer une case d’agrégation et des options indépendantes, avec état unchecked/checked/mixed
dérivé. Usage : sélectionner toutes les autorisations d’un groupe.

NativeUI fournit Checkbox et SemanticCheckedState::Mixed dans
[semantics.hpp](../include/nativeui/semantics.hpp), mais aucun groupe agrégé.

MyGo : `ui/feedback.go`, `CheckboxGroup`, `paintCheckbox` ; agrège les bools des cases construites
sous une case parent. Cible : collection explicite de bindings, pour éviter un registre implicite de
contexte.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
struct CheckboxGroupItem {
  std::string key;
  std::string label;
  Binding<bool> checked;
  bool enabled{true};
  bool read_only{false};
};
class CheckboxGroup {
public:
  CheckboxGroup(std::string label, std::vector<CheckboxGroupItem> items);
  CheckboxGroup&& style(CheckboxGroupStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<bool> mail{true};
ui::State<bool> calendar{false};
auto group = ui::CheckboxGroup("Notifications", {
    {"mail", "Courrier", mail.binding(), true, false},
    {"calendar", "Calendrier", calendar.binding(), true, false}}).spec();
```

CheckboxGroupStyle cible possède CheckboxStyle pour parent/enfants, indentation, gap et marque
mixed. Les CheckboxGroupItem et le bool agrégé restent des sous-types du groupe.

Ne pas proposer Binding<bool> au parent : son état tri-valué est dérivé. Les notifications
applicatives viennent des bindings enfants, sans second callback aggregate fictif.

## 3. État, propriété et notifications

Chaque item possède un Binding<bool> copié. Le vector d’items est un snapshot immuable pour la
génération montée, sans setter/provider d’items dans cette v1 ; keys uniques/non vides identifient
les enfants dans cette génération. Changer ordre/label/items exige remplacer la Spec au checkpoint,
avec annulation de la génération précédente ; seuls les bools restent observables live par RAII.

Agrégat dérivé sur tous les items : vide=Unchecked ; tous true=Checked ; certains true=Mixed ; sinon
Unchecked. Disabled/read_only restent comptés visuellement, puisqu’ils sont de vraies valeurs.

Activation parent : choisir false si agrégat Checked, sinon true, puis écrire seulement les enfants
enabled, non read_only et Binding valid dans l’ordre du snapshot. Un enfant non mutable peut laisser
le parent Mixed ; son dernier bool lisible reste compté.

L’écriture collective n’est pas atomique entre plusieurs State. Chaque set notifie immédiatement ;
en cas d’exception, les valeurs déjà publiées restent, les suivantes ne sont pas écrites et le
parent recalcule son agrégat au checkpoint. Aucun rollback ni retry de callbacks.

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

Case parent et enfants activent au relâchement pointeur ou Espace ; Enter suit le comportement
Checkbox. La case parent est focusable seulement lorsqu’au moins un enfant peut être modifié.

Tab visite parent puis enfants disponibles ; ce groupe ne remplace pas les cases par un unique arrêt
roving. Flèches n’effectuent pas d’opération collective.

ReadOnly hérité interdit toutes les écritures mais expose agrégat ; disabled enfant reste
visuellement présent. PointerCancel annule la demande parent avant le premier set.

Une écriture externe ou le retrait d’item pendant un set est traité sur un snapshot de bindings ;
vérifier la génération vivante avant chaque nouvelle écriture, arrêter si le groupe a été retiré.

## 5. Mesure et layout

Column : case parent puis colonne indentée d’enfants ; largeur maximum des lignes + indentation,
hauteur somme des mesures et gaps.

Indicateur mixed prend exactement la même boîte que checked ; pas de saut de layout pendant
agrégation. Les labels suivent Checkbox et les coordonnées logiques.

Contrainte étroite : clipper labels, préserver indicateurs ; aucune disparition automatique
d’autorisation par manque de place.

## 6. Présentation et invalidation

Parent utilise checkmark quand Checked, trait horizontal quand Mixed, rien quand Unchecked. La
marque suit checkbox colors et largeur de trait locale.

Modification bool demande repaint parent et enfant ; changement de texte/ordre/style métrique
demande layout. Une observation n’effectue jamais un set pour aligner la valeur agrégée.

Le parent peut rester Mixed après “tout cocher” si des enfants disabled restent false ; garder cette
présentation fidèle aux valeurs, plutôt qu’afficher une réussite artificielle.

## 7. Accessibilité

Cible : Group nommé label, case parent Checkbox checked Mixed/Checked/Unchecked et enfants Checkbox
nommés. Parent Toggle absent quand aucune valeur mutable.

La disponibilité parent est fondée sur ses actions, pas sur l’agrégat ; un groupe vide décrit une
liste sans options et ne présente pas un bouton actif.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Toute activation parent prépare un snapshot key/binding/mutabilité et un objectif unique avant le
premier set. Libérer capture avant notifications, ne retenir aucun Child Node.

Un observateur peut retirer un enfant, reconstruire le groupe ou lever. Les données déjà écrites
sont autoritatives ; recalculer l’agrégat à partir du nouveau modèle, sans terminer aveuglément
l’ancien lot.

Démonter retire tous les abonnements, y compris ceux d’items remplacés ; la destruction ne lance
jamais une action “tout décocher”.

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

Dépendances : [checkbox](checkbox.md), [column](column.md), State, disponibilité et sémantique
mixed. Aucun service collectif de notification globale à ajouter.

Keys dupliquées rejetées avant publication. Les labels dupliqués sont permis ; même Binding dans
deux items est permis, parcouru dans l’ordre, le second set identique ne notifie pas à nouveau selon
State.

Collection vide/all immutable/all invalid : pas d’écriture, aucune capture de mutation. Remplacer
les items exige nouvelle Spec au checkpoint ; aucune synchronisation d’items ou conservation de
transient focus entre générations n’est promise.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/checkbox_group.hpp` et `src/checkbox_group.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

CheckboxGroupItem et CheckboxGroupStyle restent avec le parent ; les cases enfants réutilisent le
noyau Checkbox, tandis que checkbox_group.cpp porte agrégation, lot et layout.

Inscrire `src/checkbox_group.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les
includes collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou
plugin dans l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`checkbox_group_aggregate` : vide/off/all/mixed produisent les états exacts sans aucune écriture au
mount.

`checkbox_group_mutable_only` : parent change seulement enfants enabled et modifiables ; mixed
persistant fidèle.

`checkbox_group_order_failure` : observateur milieu lève : premières valeurs restent, suivantes
inchangées, groupe récupère.

`checkbox_group_remove_during_bulk` : retrait/reconstruction pendant set arrête les anciennes
écritures.

`checkbox_group_keys` : réordre garde identité/focus ; keys dupliquées rejetées.

`checkbox_group_semantics` : parent Mixed et actions réellement disponibles ; enfants non doublés.

Ajouter `examples/features/checkbox_group.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
