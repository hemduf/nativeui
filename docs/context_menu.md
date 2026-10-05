# ContextMenu

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Ajouter à un contenu retenu un menu contextuel portable. Le contenu garde sa mesure et ses
interactions normales jusqu’à la demande explicite de menu.

NativeUI fournit [combo_popup.hpp](../include/nativeui/combo_popup.hpp) et
[overlay.hpp](../include/nativeui/overlay.hpp), mais pas de décorateur ContextMenu.

MyGo : `ui/menu.go`, méthode `Element.ContextMenu`, puis `menuPress`, `menuRelease`, `menuKey`. Elle
compose des menus plateforme ; cible NativeUI réutilise les panneaux de PopupMenu en overlay.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class ContextMenu {
public:
  ContextMenu(Spec child, PopupMenu::ItemsProvider items);
  ContextMenu(Spec child, std::vector<PopupMenuItem> items);
  ContextMenu&& item_style(MenuItemStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
auto text = ui::Label("Document").spec();
auto contextual = ui::ContextMenu(
    std::move(text),
    std::vector<ui::PopupMenuItem>{ui::PopupMenuItem::action("Copier", [] {})})
    .spec();
```

Les items, checked, shortcut_label et children suivent exactement [PopupMenu](popup_menu.md). Le
provider n’est appelé que lorsqu’une demande contextuelle est acceptée.

Le décorateur est un conteneur à un enfant ; il ne synthétise pas un nouveau bouton visuel. Le nom
accessible de l’enfant est conservé.

## 3. État, propriété et notifications

Posséder child Spec, provider et style ; session séparée par décorateur et par UI. Le point
d’ouverture est un Point logique copié, jamais un événement natif conservé.

Un menu ouvert capture le snapshot courant ; reconstruire la source des items ne retargete pas une
action déjà visible. Démontage du contenu ferme le menu de cette ancre.

L’identité utilisée pour fermer/restaurer le focus est celle du décorateur et du descendant
précédemment ciblé ; l’absence de descendant invalide fait revenir à la politique normale de Focus.

## 4. Interactions

Demande normalisée existante : InputType::ContextMenu transporte position logique/modifiers ; le
routage choisit la cible hit-test puis remonte si Ignored. Il termine une capture active avant
livraison sans déplacer le focus clavier ni armer un nouveau PointerDown. Le décorateur consomme
cette demande, pas un clic primaire synthétique.

Clavier cible : ajouter Key::Menu et Key::F10 en fin enum Key, sans renuméroter les valeurs
actuelles ; normaliser Menu/Shift+F10 en InputType::ContextMenu à la position du rectangle focusé
via le runtime. Ces touches et leur traduction plateforme ne sont pas livrées actuellement. F10
simple ne déclenche pas la demande.

Ne pas déclencher d’action principale enfant à partir du même clic secondaire ; consommer seulement
la demande acceptée. Clic principal, sélection/glissement, molette et Tab suivent l’enfant tant que
le menu est fermé.

Navigation et annulation du panneau suivent PopupMenu. Une demande refusée par le décorateur est
Ignored ; la capture précédente a néanmoins déjà été annulée par le routage normalisé existant. Une
demande acceptée avec liste vide ouvre le panneau vide fermable comme PopupMenu.

## 5. Mesure et layout

Mesure du décorateur égale celle de l’enfant, sans padding obligatoire. Son enfant est arrangé dans
les mêmes limites ; clipping et transformations du parent s’appliquent au point d’ouverture.

Le menu est placé à l’emplacement de la demande ou au bord du descendant clavier ;
clamp/retournement utilisent service overlay retenu (OverlaySpec/OverlayHandle,
detail::OverlayService). L’overlay ne participe pas à la mesure du document.

Resize et scroll de l’enfant peuvent invalider la géométrie d’ancre ; recalculer via identité sûre,
ou fermer si la cible disparaît.

## 6. Présentation et invalidation

Le décorateur n’ajoute ni bordure ni survol à l’enfant. MenuItemStyle affecte uniquement le panneau
; la présentation du contenu reste issue de ses composants.

Ouverture/fermeture invalident overlay paint ; ne reconstruire le contenu que si ses données
changent. Garder des états de focus cohérents pendant le menu modal de commandes.

Les sous-menus et timer 200 ms sont ceux de PopupMenu ; aucune seconde animation ou seconde pile de
menus.

## 7. Accessibilité

Conserver la sémantique de l’enfant ; le décorateur expose Group/None selon son rôle de composition,
et la disponibilité du menu dans description ou action de commande.

Le modèle SemanticAction actuel n’a pas ShowContextMenu ; ne pas prétendre Activate équivalent à
l’action principale enfant. Une extension ShowContextMenu peut être ajoutée séparément et routée
vers le même noyau.

Le panneau ouvert publie PopupMenu/MenuItem comme la spécification de PopupMenu, sans exposer de
références natives.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Les commandes d’ouverture sont préparées avant publication. Aucun menu n’est lancé après démontage,
même si une demande différée est déjà en file.

La fermeture commet le handle terminal avant l’action. Échec de provider ou d’enqueue : rester
fermé, garder l’enfant utilisable et annuler la commande non acceptée ; aucune ouverture synchrone
de secours.

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

Dépendances : [popup_menu](popup_menu.md), Overlay, Focus, input backend-neutre. La traduction du
geste secondaire existant et des nouvelles touches clavier doit être couverte au niveau input, pas
dans chaque décorateur.

Décorateurs imbriqués : le plus proche décorateur monté/enabled avec provider accepte et devient
propriétaire, même si snapshot vide ou inactif ; un seul menu s’ouvre. Un décorateur indisponible ou
provider nul ignore, permettant au parent de recevoir la demande.

Contenu vide, retiré ou caché : aucune demande ; coordonnées hors viewport bornées par overlay. La
liste MyGo des commandes système d’édition n’est pas ajoutée automatiquement.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/context_menu.hpp` et `src/context_menu.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Réutiliser un noyau privé de popup_menu.cpp pour la session et les panneaux ; context_menu.cpp porte
le décorateur, son routage et sa géométrie d’ancre. Les deux fichiers ont un comportement réel
distinct.

Inscrire `src/context_menu.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`context_menu_secondary` : un clic secondaire ouvre au point logique sans activer le contenu.

`context_menu_keyboard` : commande normalisée Menu/Shift+F10 ouvre près du focus ; F10 simple ne
fait rien.

`context_menu_nested` : un seul décorateur accepte ; parent peut traiter une demande refusée.

`context_menu_layout` : mesure/clip identiques au contenu avant et après ouverture.

`context_menu_stale_request` : demande différée après retrait devient inopérante ; échec
provider/queue récupère.

`context_menu_action_throw` : menu fermé avant callback même si ancre supprimée ou exception.

Ajouter `examples/features/context_menu.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
