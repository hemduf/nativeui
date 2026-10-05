# Checkbox

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer une option booléenne indépendante, avec marque et label cliquables comme une seule cible.

NativeUI : [widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc),
Checkbox, CheckboxStyle et noyau detail::CheckboxComponent. Observe le Binding pour classer
précisément layout/paint et utilise PressActivationState.

MyGo : `ui/widgets.go`, `Checkbox` ; `ui/base.go`, `CheckboxBase`. Le bool simple est déjà couvert ;
le choix mixed appartient à CheckboxGroup et ne remplace pas la signature bool de Checkbox.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
Checkbox(Binding<bool> state, std::string label);
Checkbox(State<bool>& state, std::string label);
Checkbox&& style(CheckboxStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<bool> notifications{false};
auto checkbox = ui::Checkbox(notifications, "Notifications").spec();
```

Respecter l’ordre actuel état puis label, contrairement à Toggle(label, état). Ne pas introduire une
seconde valeur interne qui pourrait diverger du Binding.

CheckboxStyle et CheckboxStylePatch existants restent disponibles depuis style.hpp et widgets.hpp ;
l’extraction n’impose pas un nouveau modèle d’options.

## 3. État, propriété et notifications

Binding<bool> possède la vérité du choix ; marque checked relue dans le Binding. La session
pointeur/Espace ne conserve qu’un armement, pas un snapshot de bool.

Au commit, copier le Binding puis publier !state.get(). Une écriture externe pendant l’appui est
donc prise en compte ; il n’y a pas de restauration d’une ancienne valeur au relâchement.

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

PointerDown arme et capture ; PointerUp intérieur bascule une fois ; sortie puis relâchement dehors
et PointerCancel ne publient aucune mutation. Le label fait partie de la cible.

Espace bascule au KeyUp après le premier KeyDown ; répétitions ne multiplient pas. Entrée n’est pas
un choix dans l’API actuelle : laisser l’événement suivre le routage parent.

Tab et disponibilité utilisent le focus commun ; ReadOnly reste focusable mais consomme les entrées
mutantes et annule une capture déjà armée. Disabled ne reçoit pas l’activation.

Molette sans effet ; aucun drag-reorder ou menu contextuel automatique. Le groupe de plusieurs cases
n’impose pas une exclusivité.

## 5. Mesure et layout

Mesure actuelle : max(minimum_width, texte + leading_padding + box_size + label_gap + 5), hauteur
control_height. Conserver cette géométrie pendant extraction.

Box centrée verticalement ; label placé après box_size et label_gap. Utiliser les limites logiques
du composant pour hit-test et clipping du texte.

Une case cochée ne grossit pas à moins qu’un patch checked change explicitement une métrique. Parent
étroit : garder indicateur lisible et clipper texte, sans affecter valeur.

## 6. Présentation et invalidation

Conserver CheckboxStyle : base/checked/hovered/pressed/focused/disabled/read_only et ordre de
résolution. Checkmark visible seulement si état true et métriques/couleur permettent peinture.

La classification d’invalidation tient compte d’un changement checked qui fait apparaître la marque
même si les styles sont identiques. Le patch checked qui change box_size doit aussi remesurer.

Le ring reste visible au clavier ; le style local ne mute pas Theme. Deux cases partageant un
Binding peuvent être cochées ensemble sans partager hover/capture.

## 7. Accessibilité

Cible : Checkbox, name=label, checked Checked/Unchecked, action Toggle et Focus si disponibles.
ReadOnly retire Toggle mais conserve checked.

detail::CheckboxComponent ne fournit pas actuellement d’override semantics dans le fichier étudié ;
publier ce contrat via les hooks Component est une condition de l’extraction complète.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

L’observation utilise déjà un objet d’invalidation détaché du composant avec flag active ; conserver
cette protection contre un observateur précédent retirant le nœud pendant un passage.

unmount désactive la classification puis retire l’abonnement. L’activation termine les opérations de
contexte avant state.set ; aucun accès this après publication.

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

Dépendances : State, TextService, ThemeBinding et PressActivationState ;
[checkbox_group](checkbox_group.md) compose des bindings indépendants sans modifier Checkbox.

Binding dont le State a expiré : respecter la sécurité de Binding existante, ne pas transformer le
contrôle en pointeur brut. Label vide admis avec nom sémantique par composition.

Valeur bool externe unique, aucun état mixed inventé. Le contenu de Form ne doit pas réinitialiser
la valeur au montage ou par focus.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/checkbox.hpp` et `src/checkbox.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Déplacer l’implémentation bool hors de widgets_checkbox_radio.inc ; préserver Checkbox et styles,
puis faire importer checkbox.hpp par les headers historiques.

detail::CheckboxComponent reste privé ; aucune instanciation template prédéfinie ni remplacement de
State<bool> requis.

Inscrire `src/checkbox.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`checkbox_release` : clic et Espace écrivent une fois au relâchement ; Enter laisse le bool
identique.

`checkbox_cancel_read_only` : annulation ou ReadOnly pendant appui ne publie pas ; capture libérée.

`checkbox_external_value` : valeur externe changée pendant appui sert de base au commit.

`checkbox_style_classifier` : marque, couleur et box_size entraînent les invalidations adaptées.

`checkbox_remove_listener` : observateur précédent retire nœud ; classificateur detached reste sûr.

`checkbox_observer_throw` : State récupère après exception ; clic ultérieur possible et aucune
relance.

Ajouter `examples/features/checkbox.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
