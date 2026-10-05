# Toolbar

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Barre horizontale de commandes avec navigation de groupe et overflow des contrôles ne tenant plus.
Les commandes restent les mêmes en barre et en menu.

NativeUI a Row, PopupMenu, Button et focus de groupe, mais aucune Toolbar publique dans
[widgets.hpp](../include/nativeui/widgets.hpp).

MyGo : `ui/toolbar.go`, `Toolbar`, `layoutToolbar`, `appendToolbarItems`. Les contrôles de fin sont
repliés puis décrits dans un menu. Cible : métadonnées possédées explicites, sans conserver ni
cliquer des pointeurs de nœuds cachés.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
struct ToolbarItem {
  std::string key;
  Spec content;
  std::optional<PopupMenuItem> overflow_item;
};
class Toolbar {
public:
  Toolbar(std::string label, std::vector<ToolbarItem> items);
  Toolbar&& style(ToolbarStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
std::vector<ui::ToolbarItem> items;
items.push_back({"save", ui::Button("Sauver", [] {}).spec(),
    ui::PopupMenuItem::action("Sauver", [] {})});
auto bar = ui::Toolbar("Document", std::move(items)).spec();
```

Les callbacks du contenu et de overflow_item doivent commander la même action applicative ; ne pas
tenter une activation synthétique d’un Node caché. Pour l’exemple minimal les actions sont vides, en
usage réel partager une fermeture possédée.

overflow_item absent marque un contrôle qui ne peut être replié ; un ToggleGroup fournit un
sous-menu children des options, checked en snapshot. Représenter un Spacer via content Spacer et
overflow_item absent avec largeur flexible.

ToolbarStyle cible : padding, gap, hauteur minimum, styles de contrôle Toolbar et libellé du bouton
“Plus”. Les snapshots d’items ne changent pas en place : remplacer la Spec à un checkpoint quand les
métadonnées applicatives changent.

## 3. État, propriété et notifications

Les keys non vides et uniques identifient les items au sein d’une génération ; labels ne servent
jamais d’identité. ToolbarItem.content et overflow_item constituent un snapshot immuable de
génération. Changer items/métadonnées demande remplacer Spec au checkpoint ; aucune mise à jour
implicite en place ni conservation de transient state entre générations n’est promise.

La barre conserve ses composants montés quand ils passent en overflow afin de garder State et
identité. Ils deviennent cachés et non interactifs ; captures/IME actives sont terminées avant
retrait visuel.

L’overflow contient des copies des métadonnées de commande. Une commande devenue obsolete est
bloquée par son jeton de propriétaire applicatif ; elle ne doit pas réactiver un contenu d’identité
recyclée.

## 4. Interactions

Tab entre sur un seul participant visible, ou Plus ; Gauche/Droite bouclent, Home/End
premier/dernier. Les flèches de TextInput ou d’un sous-groupe sont traitées par le descendant avant
navigation extérieure.

Clic/Entrée/Espace délégués aux contrôles ; le menu Plus suit PopupMenu. La molette et les commandes
sans propriétaire ne sont pas capturées par la barre.

Si le contrôle focusé est replié, terminer son interaction puis transférer le focus à Plus sans
écrire de valeur par défaut. En élargissant, ne pas voler focus au menu ouvert ; fermer ce menu s’il
n’existe plus d’overflow.

ReadOnly/Disabled sont hérités par les contrôles ; une action ponctuelle reste soumise au contrat
Button, les valeurs au contrat ToggleButton/ComboBox.

## 5. Mesure et layout

Mesurer les items intrinsèques, gap et padding ; si la somme dépasse la largeur, réserver d’abord
Plus puis replier depuis la fin les items overflowables jusqu’à ce que les visibles tiennent.

Préserver l’ordre des items conservés et des commandes overflow. Les groupes sont indivisibles pour
leur arrangement, et leur sous-menu décrit les enfants, sans couper un ToggleGroup au milieu.

Items non repliables restent présents : si leur largeur dépasse le viewport, clipper la barre et
permettre le parent scroll ; ne perdre aucune commande silencieusement. Les Spacers flex prennent
seulement l’espace restant.

Décisions d’overflow prises au layout, jamais en paint. Coordonnées logiques et styles de police
sont la source des mesures ; adapter après scale/font/resize.

## 6. Présentation et invalidation

Contrôles transparents au repos, face visible au hover/pressed ; le style Toolbar s’applique via
portée locale. Les bools checked persistent visuellement même dans le menu Plus.

Plus n’occupe pas de largeur quand tout tient. Une transition d’overflow invalide layout, focus et
structure sémantique ; couleurs seules font paint.

Aucune animation boucle globale ni remount pour chaque resize. Les invalidations d’items masqués
sont suspendues visuellement jusqu’à réapparition, tout en gardant données courantes.

## 7. Accessibilité

SemanticRole actuel ne contient pas Toolbar : utiliser Group nommé et garder actions des contrôles
visibles. Le menu Plus est Button/PopupMenu ; éléments repliés ne sont pas doublés dans l’arbre
accessible.

Pour checked ou sous-menu, utiliser les métadonnées de PopupMenuItem. Les boutons uniquement
iconiques ont un label de commande explicite dans content et overflow_item.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer un plan d’overflow complet puis publier ensemble géométries, visibilité et navigation.
Échec de mesure/fabrique : conserver le layout précédemment commis, pas un item en moitié de barre
et menu.

Le menu copie ses actions avant invocation et se ferme d’abord. Callback qui retire l’item/barre ou
lève ne produit pas une seconde action à partir de l’ancien nœud.

Rejet d’enqueue focus/overlay est un état récupérable avec commande durable au checkpoint ; ne pas
passer par des appels directs sur un participant caché.

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

Dépendances : [row](row.md), [spacer](spacer.md), [focus_scope](focus_scope.md),
[popup_menu](popup_menu.md), [toggle_group](toggle_group.md). Pas de Router ni de menu système.

Items vides : barre de taille minimale sans Plus. Keys dupliquées rejetées avant publication.
overflow_item sans callback reste visible dans le menu selon son enabled/actionable réel.

Une application mettant à jour enabled/checked doit fournir de nouvelles métadonnées au même
checkpoint que le contenu. Pas d’inspection ad hoc d’un State caché ni d’accès audio.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/toolbar.hpp` et `src/toolbar.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

ToolbarItem, ToolbarStyle et modèles de groupe/spacer restent dans le couple Toolbar ; le .cpp porte
mesure/repli, portée de style et focus, avec moteur menu partagé.

Aucun scan de types Pugl/Skia dans ToolbarItem ni conversion de Node* vers callbacks ; les keys et
métadonnées sont les seules identités de commande exportées.

Inscrire `src/toolbar.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`toolbar_overflow` : réduction largeur replie depuis la fin, réserve Plus et conserve l’ordre.

`toolbar_groups` : groupe indivisible devient un sous-menu ; checked et disabled visibles
correctement.

`toolbar_same_command` : action menu invoque exactement la même commande que le contrôle visible.

`toolbar_resize_focus` : repli du focus/drag annule proprement ; réélargissement sans remount ni vol
focus.

`toolbar_nonoverflowable` : contrôle sans metadata reste visible/clippé ; aucun item perdu.

`toolbar_stale_throw` : commande devenue périmée ou callback levant ne réactive aucun nœud retiré.

`toolbar_layout_failure` : exception de mesure ne publie aucune géométrie/visibilité partielle.

Ajouter `examples/features/toolbar.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
