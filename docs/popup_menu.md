# PopupMenu

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Ouvrir un menu ancré à un bouton ; une action peut être invoquée au pointeur ou au clavier. Le menu
est un overlay portable de la même UI.

NativeUI : [combo_popup.hpp](../include/nativeui/combo_popup.hpp), PopupMenu/PopupMenuItem,
MenuPopupComponent et commandes overlay. Sont présents actions, séparateurs, éléments disabled et
styles de bouton/menu.

MyGo : `ui/widgets.go`, `MenuButton` ; `ui/menu.go`, `Menu.Item`, `Separator`, `Submenu`,
`MenuItem.Checked`, `Shortcut`. Cible : checked, raccourci affiché et sous-menu, sans importer les
menus système de MyGo.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
using ItemsProvider = std::function<std::vector<PopupMenuItem>()>;
PopupMenu(std::string label, std::vector<PopupMenuItem> items);
PopupMenu(std::string label, ItemsProvider items_provider);
PopupMenu&& style(ComboBoxStyle value) &&;
PopupMenu&& item_style(MenuItemStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
auto menu = ui::PopupMenu("Actions", std::vector<ui::PopupMenuItem>{
    ui::PopupMenuItem::action("Ouvrir", [] {}),
    ui::PopupMenuItem::separator(),
    ui::PopupMenuItem::action("Supprimer", [] {}, false)}).spec();
```

PopupMenuItem actuel possède kind(Action/Separator), label, enabled et std::function<void()>
callback ; action() et separator() conservent leur comportement.

Extensions cibles possédées : key string optionnelle unique par fratrie, optional<bool> checked,
string shortcut_label et vector<PopupMenuItem> children. Un item children non vide est un sous-menu
sans callback direct ; les raccourcis affichés ne les enregistrent pas globalement.

Provider est rappelé à chaque ouverture, jamais depuis paint. Il produit un snapshot possédé pour
toute la session ; les changements seront visibles au prochain menu ouvert.

## 3. État, propriété et notifications

Le provider et chaque callback sont possédés. Runtime d’ancre porte l’identité retenue, handle
overlay et suppression de touche d’ouverture ; aucun pointeur brut de nœud n’est stocké dans les
actions.

La session garde un snapshot immuable des items, un surligné local et une pile de sous-menus.
Checked est une lecture visuelle du snapshot ; l’action applicative publie la nouvelle valeur pour
la prochaine ouverture.

Fermer puis appeler le callback est l’ordre de commit. L’ancre peut disparaître pendant l’action
sans prolonger le menu ni invoquer deux fois le même item.

## 4. Interactions

Ouverture : clic au relâchement, Entrée initiale, Espace au relâchement ; flèche Bas ouvre. La
touche ayant ouvert reste consommée jusqu’au KeyUp afin de ne pas activer le premier item.

Dans le panneau : Haut/Bas parcourent les actions actives, Home/End première/dernière ;
Entrée/Espace valident. Séparateurs, disabled et actions sans callback sont sautés.

Cible sous-menu : Droite ouvre children du surligné ; Gauche ferme le niveau courant ; Échap ferme
un niveau puis toute la session. Survol ouvre un sous-menu après 200 ms ; délai cancellable, jamais
un pointeur de Node.

Tab et clic extérieur ferment selon l’overlay existant. PointerCancel cesse l’armement sans choisir.
La molette défile un panneau trop haut ; focus retourne à l’ancre encore valide ou suit la politique
du runtime.

## 5. Mesure et layout

Largeur du bouton inchangée : label mesuré, minimum et padding du ComboBoxStyle. Le panneau mesure
les labels, colonne de checked, shortcut_label et indicateur children.

Ancre en coordonnées logiques ; placement sous l’ancre puis retournement/clamp dans le viewport par
le service overlay. Chaque sous-menu calcule son placement à droite ou gauche, sans dépasser le
bord.

Un grand menu a une hauteur bornée et viewport scrollable ; ne pas monter tous les sous-panneaux
fermés. Suppression d’une ancre invalide ferme la session.

## 6. Présentation et invalidation

Conserver ComboBoxStyle/MenuItemStyle. Extensions MenuItemStyle pour colonnes
checked/shortcut/chevron restent dans le modèle du menu ; aucun menu OS ou slot Theme fictif.

Surligné et focus ne réévaluent pas le provider. Variation de métriques d’item invalide
mesure/panneau ; variation de couleur seule invalidate paint.

Un item checked ne change pas de largeur entre true/false : réserver la colonne. Aucun timer de
sous-menu lorsque fermé, caché ou démonté.

## 7. Accessibilité

Cible : ancre SemanticRole::Button avec état expanded ; panneau
PopupMenu, éléments MenuItem, séparateurs None et checked applicable pour les items marqués.

Le surligné et la relation de sous-menu utilisent snapshots/identités ; annonce nom et
shortcut_label en description. combo_popup.hpp ne fournit actuellement pas d’override semantics :
publication à compléter.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer le snapshot et les panneaux avant publication du handle. Si provider ou montage lève,
l’ancre reste fermée, ou l’ancienne session cohérente ; aucun handle semi-ouvert.

Toute action démarre après fermeture logique et remise du focus au checkpoint prévu. Exception de
callback : session reste fermée ; aucune réouverture ni relance.

Rejet/exception d’enqueue overlay : laisser une commande durable pour le prochain checkpoint ou
rapporter refus sans publication ; ne pas exécuter synchroniquement une action qui exige fermeture
différée.

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

Dépendances : [combo_box](combo_box.md), service overlay retenu (OverlaySpec/OverlayHandle,
detail::OverlayService) et commandes overlay, Focus, ThemeBinding, styles de menu. ContextMenu
réutilise le même moteur de panneau.

Liste vide admise : open_popup actuel publie un panneau sans action sélectionnable ; Échap, Tab ou
clic extérieur le ferment. Préserver cette ouverture vide sans inventer un item/callback. Le
snapshot de session ne change pas parce que l’application modifie sa liste.

Children invalides, keys dupliquées ou profondeur supérieure à 16 sont rejetés avant ouverture avec
invalid_argument pour l’API C++ directe. Les callbacks et styles restent opaques au moteur de menu.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/popup_menu.hpp` et `src/popup_menu.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Conserver combo_popup.hpp comme header compatible important également ComboBox. PopupMenuItem et
variantes menu restent déclarés dans popup_menu.hpp.

Extraire les noyaux ancre/panneau non template de combo_popup.hpp ; partager avec ContextMenu par
une interface privée, sans switch central de widgets ni duplication du service overlay.

Inscrire `src/popup_menu.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`popup_menu_snapshot` : provider exécuté à l’ouverture ; mutation source ultérieure ne change pas la
session.

`popup_menu_navigation` : disabled/séparateurs sautés ; suppression touche d’ouverture ; nested
Droite/Gauche/Échap.

`popup_menu_placement` : long menu défile et sous-menu retourne près du bord à plusieurs scales.

`popup_menu_action_once` : fermeture commise avant action ; callback retire ancre ou lève sans
seconde action.

`popup_menu_open_failure` : provider, mount et enqueue en échec laissent handles/focus récupérables.

`popup_menu_legacy_api` : action, separator, style et item_style historiques compilent sans
changement.

Ajouter `examples/features/popup_menu.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
