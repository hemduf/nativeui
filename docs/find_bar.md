# FindBar

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Barre de recherche dans un document, avec champ query, count, précédent/suivant et fermeture.
L’application calcule les matches et applique le déplacement ; le widget ne parcourt aucun texte.

NativeUI possède CommandScope et TextInput mais pas de FindBar. Le nouveau composant assemble des
contrôles et observations, sans moteur de recherche ou index global.

MyGo : `ui/feedback.go`, `FindBar`. Open/query externes, count/current, navigation circulaire,
Enter/Shift+Enter, Cmd+G sur macOS et F3 ailleurs ; ouverture focus et sélection du query.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class FindBar {
public:
  FindBar(Binding<bool> open, Binding<std::string> query,
          Binding<std::size_t> matches, Binding<std::optional<std::size_t>> current);
  FindBar(State<bool>& open, State<std::string>& query,
          State<std::size_t>& matches, State<std::optional<std::size_t>>& current);
  FindBar&& label(std::string value) &&;
  FindBar&& on_navigate(std::function<void(std::size_t)> callback) &&;
  FindBar&& style(FindBarStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<bool> open{true};
ui::State<std::string> query{"gain"};
ui::State<std::size_t> matches{3};
ui::State<std::optional<std::size_t>> current{std::optional<std::size_t>{0}};
auto find = ui::FindBar(open, query, matches, current).label("Rechercher")
    .on_navigate([](std::size_t) {}).spec();
```

matches est lu/observé mais jamais écrit par le widget. current optionnel exprime absence de choix ;
remplacer l’index MyGo 0 artificiel en absence de matches par un contrat explicite.

FindBarStyle cible : SearchFieldStyle/TextInputStyle, ButtonStyle navigation/fermeture, TextStyle
status, gap/padding et largeur maximum du champ ; default label “Rechercher”.

on_navigate reçoit l’index cible après publication current, même s’il reste identique avec un seul
match ; l’application peut alors centrer la vue à nouveau. Aucun on_change de query additionnel.

## 3. État, propriété et notifications

open/query/current sont bindings mutables, matches source externe read-only par usage. Phases
opened/closed et ID de focus précédent sont locaux à la barre.

Query edit publie live ; l’application met à jour matches/current. Au rendu, current inconnu ou hors
plage n’est pas réécrit : utiliser un index effectif clamp pour status/navigation, absence quand
matches=0.

La première navigation en absence de current choisit 0 pour suivant et count-1 pour précédent ;
sinon modulo count. Matches count change annule une action armée si ancienne génération cible
devenue invalide.

Fermer publie open=false, conserve query/current ; rouvrir conserve ces valeurs et sélectionne query
pour remplacement. Le widget ne remet pas defaults à ouverture.

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

À passage false->true, demander focus de champ et sélectionner query. Enter/Shift+Enter vont
suivant/précédent ; boutons mêmes actions, circulaires aux extrémités.

Raccourcis de navigation : CommandScope reçoit nouvelles commandes portables FindNext/FindPrevious ;
ajouter les enumerators à la fin de Command et traduire Cmd+G/Shift+Cmd+G ou F3/Shift+F3 dans couche
input privée.

Key::F3 n’existe pas actuellement : ajouter en fin enum sans renuméroter l’existant, avec tests
mapping. Le composant ne teste pas des types OS ni n’enregistre un hook clavier process-wide.

Escape ferme barre depuis champ (après annulation composition si active), bouton Terminer pareil.
Tab parcourt champ, précédent, suivant, terminer ; directions désactivées count=0.

Sur fermeture, restaurer focus précédent s’il est valide et si le focus appartient encore à la barre
; sinon laisser focus applicatif. Molette ne navigue pas ; ReadOnly empêche query/current mutations,
la fermeture reste une action de visibilité.

## 5. Mesure et layout

Quand closed : aucun enfant peint ni arrêt focus, mesure nulle. Ouvert : Row champ flex, status de
largeur réservée, boutons navigation/fin ; hauteur contrôle + padding.

Largeur étroite privilégie champ et fin ; status peut clipper avant d’écraser zones d’action,
boutons gardent taille minimum. Pas d’overflow implicite ni popup.

Text status utilise chiffres tabulaires si service le permet, sans changer nombre de matches.
Coordonnées logiques et clipping suivent parent.

## 6. Présentation et invalidation

Fond surface et bordure de séparation, status muted. Query vide affiche status vide ; query non
vide/count0 “Aucun résultat” ; sinon “n sur count” avec n effectif+1.

Une valeur current invalide n’apparaît pas comme indice impossible ; marquer absence/valeur externe
invalide en description sans set implicite.

Query/count/current change paint ; transition open ou style métrique layout/structure. Pas de timer
ou boucle de recalcul de recherche automatique.

## 7. Accessibilité

Cible : Group nommé (Toolbar absent dans SemanticRole), champ TextInput, status Text, boutons
Previous/Next/Done avec actions réelles.

SemanticRole n’a pas Status ni live region ; fournir description/current text snapshot et prévoir
extension d’annonce separately si exigée par pont natif. Aucun résultat AT natif revendiqué.

Closed enlève ses descendants accessibles. Query, count et current exposés comme données immutable
backend-neutres, jamais callbacks de recherche depuis lecture native.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Une navigation prépare target à partir snapshot count/current puis publie current et appelle
on_navigate copié si propriétaire/génération encore vivants et Bindings valides. Callback commencé
ne sera jamais rejoué.

L’observateur current peut retirer barre ou remplacer count ; revérifier token/génération avant
on_navigate, annuler l’action obsolete plutôt que envoyer mauvais index.

open false ferme text input/composition et capture avant publication de descendants cachés.
Destruction n’appelle ni close action ni navigate ; Focus précédent est un handle sûr, pas Node*.

Échec demande focus/commande différée reste pending au checkpoint sans fallback sync ; observer
levant conserve valeurs déjà commises et dispatch réutilisable.

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

Dépendances : [text_input](text_input.md), [search_field](search_field.md), [button](button.md),
[command_scope](command_scope.md), Focus/State. Recherche/replace/options regex appartiennent à
l’application.

count=0 : aucune navigation/callback et current préservé ; count=1 : navigation peut callback même
index. Count variant pendant geste utilise génération pour annuler cible retirée.

Bindings invalid : dernier snapshot peut être affiché mais plus de mutation/callback ; si open
invalid, barre se cache au checkpoint. Query extrêmement long et counts de grande taille ne
débordent pas modulo arithmetic.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/find_bar.hpp` et `src/find_bar.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

FindBarStyle et tous modèles/status restent dans le couple ; find_bar.cpp possède phases,
observations, commandes, navigation et layout.

Les nouvelles commandes et touches sont ajoutées au modèle input partagé et normalisées en backend
privé ; leurs tests de compatibilité enums sont un prérequis de l’implémentation du composant.

Inscrire `src/find_bar.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`find_bar_opening` : open true demande focus/select query ; closed nul sans desc focusables.

`find_bar_navigate_wrap` : forward/back/modulo et absence current donnent indices attendus ; one
match callback possible.

`find_bar_count_change` : count0 ignore et retire directions ; cible supprimée pendant appui
annulée.

`find_bar_shortcuts` : Enter/Shift+Enter et commandes normalisées macOS/autres aboutissent même
action, enums stables.

`find_bar_closing` : Escape/Done conservent query ; restauration focus seulement si encore
propriétaire.

`find_bar_reentrant` : current observer remplace count/retire barre/ lève : aucune mauvaise
navigation ni retry.

`find_bar_invalid_state` : suppression d’un State garde dernier snapshot sûr et coupe
action/callback.

Ajouter `examples/features/find_bar.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
