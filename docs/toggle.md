# Toggle

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Interrupteur booléen à curseur visuel ; activer/désactiver une option. Ce nom conserve sa
signification actuelle NativeUI.

NativeUI : [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc), ToggleComponent public
; [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), Toggle(label,
State/Binding). Style dans [toggle_style.hpp](../include/nativeui/toggle_style.hpp).

MyGo : `ui/widgets.go`, `Switch` ; `ui/base.go`, `SwitchBase`. Son `Toggle` désigne un bouton pressé
porté séparément comme ToggleButton ; Switch<T> NativeUI reste une composition conditionnelle.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
Toggle(std::string label, Binding<bool> state);
Toggle(std::string label, State<bool>& state);
Toggle&& style(ToggleStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<bool> enabled{true};
auto control = ui::Toggle("Actif", enabled).spec();
```

Conserver l’ordre label puis état ainsi que ToggleComponent public et ToggleStyle. Aucun alias
Switch vers Toggle, aucun changement du type bool.

Valeur notifiée par Binding ; pas d’API audio start/end gesture ni callback d’automation dans le
contrôle.

## 3. État, propriété et notifications

Binding<bool> est l’unique état persistant. Le curseur représente state.get() ; les bools
space_pressed_/enter_pressed_ filtrent les répétitions et ne sont pas la valeur.

Les valeurs externes sont observées et rafraîchissent le rendu. Une écriture externe lors d’un appui
reste autoritative ; aucun commit final au PointerUp n’écrase sa modification.

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

Comportement actuel à conserver : PointerDown bascule immédiatement puis capture ; PointerUp termine
l’appui sans second set. PointerCancel ne restaure pas la valeur déjà publiée.

Space et Enter basculent au premier KeyDown ; auto-repeat supprimé jusqu’au KeyUp. Perte de
focus/désactivation remet les drapeaux de touche à zéro.

ReadOnly bloque les pointeurs mutants et touches ; si appliqué pendant capture, terminer l’armement
sans publier. Disabled suit disponibilité héritée. Molette ignorée.

Le curseur ne se déplace pas proportionnellement au drag : c’est un bool, pas un slider. Toute
nouvelle interaction “drag pour choisir” serait une extension séparée, pas une conséquence de
l’extraction.

## 5. Mesure et layout

Track, thumb et label utilisent ToggleStyle et mesure de texte, selon le noyau existant. La position
on/off déplace seulement le thumb, pas la largeur du contrôle.

Garder padding, gap et minimum de la source ; les limites logiques servent au hit-test même à scale
élevé. Un label long se clippe sous contrainte du parent.

Resize pendant appui ne repasse pas state.set. Changement métrique de track/thumb/police implique
layout, changement de position/couleur implique paint.

## 6. Présentation et invalidation

Conserver style base/checked/hovered/pressed/focused/disabled/read_only et patches actuels. Afficher
position checked et distinction focus ; ne pas remplacer l’interrupteur par une face de bouton.

La source ne promet pas d’animation temporelle automatique du curseur ; l’extraction ne crée aucun
timer. Une animation future doit respecter réduit-motion et pause hors disponibilité.

Le classificateur de transition compare métriques et présentation, y compris patches dependent on
checked. Theme reste par instance/portée.

## 7. Accessibilité

Cible : Toggle, checked Checked/Unchecked, action Toggle quand mutable ; nom label. Le bool est lu
après commit instantané, sans attendre PointerUp.

ToggleComponent ne publie pas actuellement semantics dans le fichier étudié. Ajouter l’override lors
extraction sans déclarer un pont natif livré.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Tout contexte/capture doit être dans un état cohérent avant la publication instantanée à
PointerDown. Copier le Binding si nécessaire pour ne pas relire ce composant après set retirant son
sous-arbre.

Une exception d’observateur conserve la valeur commise ; drapeaux clavier/capture rétablis pour
prochaine interaction. Le démontage ne remet jamais l’option false.

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

Dépendances : ThemeBinding, State, PressActivationState et TextService.
[toggle_button](toggle_button.md) partage la logique bool mais a un timing de commit différent
explicite.

Deux interrupteurs liés à un même bool affichent la même valeur mais leurs captures/focus restent
distincts. State expiré suit Binding, aucune référence brute.

Aucun service système, préférence OS, automation ou traitement DSP n’est déclenché directement par
le widget. L’observateur applicatif réalise l’effet.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/toggle.hpp` et `src/toggle.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Déplacer ToggleComponent public et le builder depuis les .inc ; le header conserve leurs
déclarations et signatures publiques. Le .cpp accueille observer/input/layout/paint et
classification de style.

toggle_style.hpp reste compatible ; les anciens includes widgets.hpp/nativeui.hpp continuent à
exporter Toggle. Ne jamais renommer ce composant en Switch.

Inscrire `src/toggle.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`toggle_down_commit` : PointerDown écrit une fois ; PointerUp/Cancel n’annulent ni ne réécrivent.

`toggle_key_repeat` : Space/Enter au premier KeyDown uniquement ; KeyUp autorise la suivante.

`toggle_read_only` : transition ReadOnly pendant capture termine geste et préserve valeur.

`toggle_external_change` : valeur externe entre Down et Up n’est pas écrasée au relâchement.

`toggle_style` : checked modifie thumb et invalidation exacte ; géométrie préservée à extraction.

`toggle_publish_remove_throw` : observateur retire/ lève après commit ; captures et prochain Toggle
récupèrent.

Ajouter `examples/features/toggle.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
