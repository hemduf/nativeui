# Button

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Déclencher une action ponctuelle : sauvegarde, confirmation, commande de barre d’outils. L’état
pressé ne constitue pas une valeur persistante.

NativeUI fournit Button, ButtonStyle et ButtonVisualState dans
[widgets_button.inc](../include/nativeui/detail/widgets_button.inc). La machine commune
[widgets_activation.inc](../include/nativeui/detail/widgets_activation.inc) termine l’interaction
avant le callback.

MyGo : `ui/widgets.go`, fonctions `Button`, `PrimaryButton`, `styleButton`. Sa variante principale
utilise la couleur accent et accepte des enfants ; NativeUI dessine actuellement un label. Cible :
variante principale et contenu composé, dans le même composant.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
Button(std::string label, ActivateCallback on_activate);
Button&& style(ButtonStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
auto save = ui::Button("Enregistrer", [] {}).spec();
```

Extensions cibles : `enum class ButtonVariant { Standard, Primary, Toolbar };`, `Button&&
variant(ButtonVariant) &&` et `Button&& content(Spec) &&`. Un contenu remplace le label visuel ; le
label reste le nom accessible.

ActivateCallback reste std::function<void()>. Une action absente laisse un bouton affichable et
activable sans effet ; aucune dépendance à une commande audio ou un service natif.

Les enfants composés du contenu n’ajoutent pas de cible d’activation : le bouton est un seul
contrôle ; un enfant focusable est rejeté à la validation du contenu.

## 3. État, propriété et notifications

Le label, style, contenu et callback sont possédés par la Spec puis par le composant. Pas de
Binding<bool> pour un bouton momentané.

Survol, capture et appui clavier sont locaux à l’instance. La variante principale ne choisit pas
automatiquement une action par défaut dans toute la fenêtre.

Une action peut modifier l’état applicatif ou retirer le sous-arbre. Le widget ne se relit plus
après son callback ; le rafraîchissement appartient aux bindings des composants affectés.

## 4. Interactions

Pointeur : armement à PointerDown, capture ; déplacement hors des limites retire le pressé ;
PointerUp à l’intérieur active une fois. PointerCancel, disparition ou perte de focus annulent
l’armement sans action.

Espace arme au premier KeyDown et active au KeyUp ; les répétitions ne multiplient pas l’action.
Entrée active au premier KeyDown, puis reste supprimée jusqu’au KeyUp, conformément à l’existant.

La molette et le glissement externe ne modifient rien. Disabled bloque l’activation et le focus
selon la disponibilité héritée ; ReadOnly conserve la sémantique d’action existante, car aucune
valeur n’est éditée.

Le mode Toolbar est une présentation locale ; la navigation de groupe appartient à Toolbar, qui
appelle le même callback.

## 5. Mesure et layout

Mesure actuelle : largeur maximale entre minimum_width et mesure du texte avec deux
horizontal_padding ; hauteur control_height. Préserver ces résultats à style identique.

Le contenu cible fournit sa mesure intrinsèque, entourée du padding du bouton ; le parent distribue
les contraintes. Un espace inférieur au minimum clippe le contenu, sans dessiner hors de la zone
allouée.

Toutes les dimensions sont logiques ; le scale framebuffer ne change ni hit-testing ni seuil
d’armement. Redimensionner pendant la capture utilise les limites courantes au relâchement.

## 6. Présentation et invalidation

Conserver ButtonStyle base/hovered/pressed/focused/disabled/read_only et l’ordre de résolution
actuel. Primary utilise accent en fond et couleur lisible issue du thème ; Toolbar est transparent
au repos.

Focus visible et pressé sont indépendants ; le ring ne disparaît pas sous un patch pressed. Les
métriques de police, padding et minimum demandent layout ; les seules couleurs demandent paint.

Un contenu personnalisé utilise les services de peinture communs et la disponibilité héritée ;
aucune animation permanente ni boucle Tick n’est ajoutée.

## 7. Accessibilité

Cible : SemanticRole::Button, nom tiré du label, action Activate quand enabled ; le bouton momentané
n’a pas d’état checked.

ButtonComponent ne publie pas actuellement un override semantics dans sa source ; cet enrichissement
doit être testé, y compris avec un contenu iconique dont le label visuel est remplacé.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

La fermeture du menu overflow ou d’un Dialog qui appelle Button doit avoir commis son état terminal
avant l’action. Annulation et destruction ne déclenchent jamais Activate.

Si l’action lève après avoir modifié un State, le bouton reste désarmé et le prochain clic
fonctionne ; ne tenter ni rollback de l’application ni relance automatique.

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

Dépendances : Component, ThemeBinding, TextService, styles et machine PressActivationState.
Réutiliser cette dernière pour Link/ToggleButton ; ne pas créer un second focus manager.

Label vide admis pour contenu iconique, mais exiger un nom sémantique non vide dans l’exemple
accessible. Texte très long, UTF-8 invalide et police remplacée suivent les règles de TextService.

Si content(Spec) échoue au montage, aucun enfant partiel ne reste publié ; conserver le bouton
précédent jusqu’au checkpoint de réconciliation.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/button.hpp` et `src/button.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Extraire le noyau actuellement inline de widgets_button.inc ; ButtonVisualState et ActivateCallback
demeurent disponibles depuis les includes historiques.

Les nouveaux builders variant/content complètent les signatures sans changer Button(label, callback)
ni les patches ButtonStyle. La variante PrimaryButton de MyGo ne crée aucun fichier C++
supplémentaire.

Inscrire `src/button.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`button_activation` : un clic intérieur et une paire Espace produisent exactement une action ; Enter
répété est filtré.

`button_cancel` : sortie, PointerCancel et désactivation ne déclenchent rien ; nouvelle activation
réussit.

`button_layout_style` : couleur seule invalide paint, police/padding invalident layout ; contenu
reste clippé.

`button_reentrant_throw` : action retire son bouton ou lève puis prochain clic sur un autre bouton
fonctionne.

`button_primary_semantics` : Primary conserve nom, Activate et contraste ; contenu iconique reste un
seul contrôle.

Ajouter `examples/features/button.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
