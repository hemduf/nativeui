# Stepper

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Contrôle compact à deux flèches incrémentant/décrémentant un nombre ; notamment à côté de
NumberInput. Aucune zone de saisie de texte dans Stepper lui-même.

NativeUI propose sliders float et domaines privés, mais aucun Stepper. Réutiliser
[state.hpp](../include/nativeui/state.hpp), l’input et la disponibilité ; créer un domaine double
privé pour les nouveaux contrôles numériques.

MyGo : `ui/indicators.go`, `Stepper`. Flèches, Home/End et maintien répété (400 ms initial, 80 ms
répétition) sont le comportement à porter.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class Stepper {
public:
  explicit Stepper(Binding<double> value);
  explicit Stepper(State<double>& value);
  Stepper&& label(std::string value) &&;
  Stepper&& range(double minimum, double maximum) &&;
  Stepper&& step(double value) &&;
  Stepper&& style(StepperStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<double> copies{1.0};
auto arrows = ui::Stepper(copies).label("Copies").range(1.0, 99.0).step(1.0).spec();
```

Defaults : range[0,100], step=1, flèches verticales ; min<=max finite et step>0 finite. StepperStyle
contient largeur/hauteur, flèches/bordure/fonds pour
normal/hovered/pressed/focused/disabled/read_only.

Pas de timer configuré par callback applicatif : délai 400 ms et cadence 80 ms constants internes
avec horloge injectée en tests. label nomme le contrôle sans occuper une troisième cellule.

## 3. État, propriété et notifications

Binding<double> est l’état. Valeur externe non finite affichée/effective=minimum, hors plage clamp
pour affordances sans write.

Interaction augmente/diminue la valeur effective puis snap sur grille ancrée minimum et clamp.
Min==max est un contrôle constant sans action ; set identique ne notifie pas.

Chaque tick répété relit la valeur courante afin de respecter une écriture externe pendant maintien.
Les flèches désactivées à une borne ne demandent pas de répétition.

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

PointerDown sur flèche active fait un incrément immédiat, capture et arme repetition. Après 400 ms,
incréments toutes les 80 ms tant que pointeur reste dans la même flèche.

Quitter la flèche suspend repetition ; retour redémarre le délai sans nouvel incrément immédiat.
PointerUp/Cancel, focus perdu, hidden/disabled/read_only arrêtent le timer et capture.

Haut/Droite=plus, Bas/Gauche=moins, Home=min, End=max ; répétition clavier suit les KeyDown
normalisés, sans doubler la répétition pointeur. Échap annule maintien sans rollback.

Tab donne un arrêt sur le contrôle, pas un arrêt par flèche ; molette ignorée. ReadOnly permet
focus/lecture mais supprime toutes mutations.

## 5. Mesure et layout

Deux cellules verticales de même hauteur, séparateur fin et dimension compacte explicitement
déterminée par StepperStyle. Le parent NumberInput aligne le contrôle à la hauteur du champ.

Hit-test des deux moitiés suit les limites arrangées, en coordonnées logiques. Hauteur nulle ou
largeur nulle empêche armement ; zone séparateur est attribuée à moitié supérieure pour un résultat
déterministe.

Valeur ne change pas la mesure ; seule géométrie/flèches/style métrique demande layout.

## 6. Présentation et invalidation

Flèches près des bornes ont disabled visuel local ; contrôle lui-même reste focusable si une
direction est possible. Focus ring autour des deux flèches.

Au maintien, pressed concerne seulement la flèche active ; value change invalidate paint pour
réévaluer disponibilité des directions.

Aucun frame request après la borne atteinte, fermeture ou démontage ; le runtime de ticks/animation
est par instance.

## 7. Accessibilité

SemanticRole n’a pas Stepper : utiliser Custom avec numeric_value, value_range et actions
Increment/Decrement/SetValue, nom label.

Les flèches ne sont pas deux arrêts Tab, mais leurs actions sont accessibles depuis le contrôle. Un
rôle SpinButton est une extension du schéma séparée, pas un rôle disponible aujourd’hui.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Avant chaque set répétitif, préparer la nouvelle valeur et vérifier génération propriétaire.
Callback retirant le Stepper stoppe immédiatement le calendrier ; aucune itération suivante sur un
nœud détruit.

Après exception d’observer, arrêter le maintien, rétablir capture/drift de timer et laisser la
valeur déjà commise. Ne rattraper ni rejouer les ticks passés ; prochain appui repart normalement.

Horloge en retard : produire au plus un incrément par Tick livré, puis fixer prochaine échéance
depuis maintenant ; éviter une rafale applicative non bornée après suspension.

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

Dépendances : Binding, commandes input/horloge, disponibilité et [number_input](number_input.md)
pour composition. Domaine double privé partagé avec NumberInput, sans modifier SliderDomain float.

Domaine/step invalides : invalid_argument avant mount. Step supérieur au span autorisé, bornes
atteignables ; éviter overflow value+step par clamp d’intermédiaires et contrôles finite.

Min==max, valeur NaN et all unavailable ne créent jamais un timer permanent. Deux instances ne
partagent aucune échéance ni flèche active.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/stepper.hpp` et `src/stepper.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Le header expose Stepper/StepperStyle ; le .cpp implémente cellules, domaine, horloge, répétition et
peinture. Le moteur numérique commun est privé, pas un fichier parent supplémentaire de composant.

Inscrire `src/stepper.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`stepper_domain` : min/max/step validés ; clamp/snap et domaine constant sans write.

`stepper_repeat_clock` : Down immédiatement, aucun avant 400 ms, ensuite cadence 80 ms avec horloge
contrôlée.

`stepper_pause_cancel` : sortie/retour redémarre délai ; Up/Cancel/hidden arrêtent tout tick.

`stepper_external_held` : écriture externe pendant maintien est base du prochain incrément.

`stepper_late_tick` : suspension longue ne crée pas rafale de notifications.

`stepper_throw_remove` : observer retire/ lève : capture/timer arrêtés et prochaine interaction
récupère.

Ajouter `examples/features/stepper.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
