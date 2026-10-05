# Slider

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer une valeur continue ou quantifiée sur un axe horizontal/vertical. Afficher facultativement
une valeur formatée, sans transport d’automation plugin.

NativeUI : [widgets_slider.inc](../include/nativeui/detail/widgets_slider.inc), Slider,
SliderVisualState et noyau ; [slider_value.hpp](../include/nativeui/detail/slider_value.hpp) définit
domaine et normalisation. Style partagé dans
[slider_style.hpp](../include/nativeui/slider_style.hpp).

MyGo : `ui/widgets.go`, `Slider` ; `ui/base.go`, `SliderBase`. Le domaine MyGo est float64 ;
NativeUI float constitue une API existante à conserver.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
explicit Slider(Binding<float> state);
explicit Slider(State<float>& state);
Slider&& range(float minimum, float maximum) &&;
Slider&& step(float value) &&;
Slider&& orientation(SliderOrientation value) &&;
Slider&& formatter(Formatter value) &&;
Slider&& style(SliderStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<float> amount{0.5f};
auto slider = ui::Slider(amount).range(0.0f, 1.0f).step(0.01f)
    .orientation(ui::SliderOrientation::Horizontal).spec();
```

Formatter conserve std::function<std::string(float)> du noyau ; builder default : range [0,1],
step=0, horizontal. step=0 signifie mouvement continu.

Constructeurs gardent State<float>/Binding<float>. Le portage d’un algorithme MyGo double ne
justifie aucune migration implicite du type public.

## 3. État, propriété et notifications

Binding<float> est autoritatif ; les valeurs externes ne sont jamais snap/clamp réécrites au
mount/paint. Pour géométrie seulement : non finite devient minimum, hors plage clamp.

Interaction utilisateur : quantifier minimum + round((value-minimum)/step)*step si step>0 puis clamp
; intermédiaires double pour éviter débordement du span float.

Le glissement publie live à Down/Move/Up seulement si valeur change. Une écriture externe pendant
drag est relue lors prochaine cible ; PointerCancel ne restaure pas l’ancien snapshot.

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

PointerDown choisit la cible sur l’axe et capture ; Move suit en dehors des limites avec valeur
bornée ; Up finalise la cible et relâche ; PointerCancel arrête sans rollback.

Gauche/Bas diminuent, Droite/Haut augmentent ; Home=min, End=max. step>0 = incrément step ; continu
= span/100, Shift = span/1000.

Orientation verticale : maximum en haut et minimum en bas selon SliderTrackAxis existant. ReadOnly
consomme les mutations et arrête une capture sans set ; Disabled ne reçoit pas l’édition.

Molette et Échap n’éditent pas actuellement le slider ; préserver leur ignorance. Tab et focus
suivent runtime, un seul contrôle focusable.

## 5. Mesure et layout

Les métriques existantes sont conservées : longueur de contrôle, épaisseur track, insets du thumb et
espace formatter définissent le même axe pour peinture et mapping.

Le formatter réserve une bande d’affichage à l’horizontal et à la verticale ; sa présence doit être
identique dans slider_track_axis pour pointer et paint.

Si axe disponible nul, mapping conserve une fraction sûre sans division par zéro. Toutes positions
sont logiques ; scale n’altère pas les valeurs ni le seuil de quantification.

## 6. Présentation et invalidation

SliderStyle et SliderVisualState Normal/Hover/Pressed/Focused/Disabled/ReadOnly restent publics. La
face et le ring sont résolus par Theme et patches existants.

Binding change invalide paint ; style track/largeur/minimum/formatter métrique invalide layout. Le
formatter est une callback de rendu : ne l’appeler que sous contrat UI, sans mutation de modèle.

Un formatter levant doit laisser la frame précédente valide et les save/clip équilibrés. Aucun timer
ni animation auto ajouté à extraction.

## 7. Accessibilité

Cible : Slider, numeric_value effectif, value_range {minimum,maximum,step}, actions
Increment/Decrement/SetValue et Focus quand mutable.

La source actuelle du widget ne publie pas semantics ; ajouter le snapshot cible conserve les règles
de clamp sans write. Formatter text peut compléter text_value, mais ne doit pas être appelé depuis
lecture native immutable.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Déterminer target et terminer capture/context avant publier State ; callback observer retirant le
slider ne permet aucun accès postérieur au noyau.

Abonnement Binding retiré au démontage ; capture annulée et focus local effacé. Exception d’observer
ou formatter restaure les drapeaux sans réémettre le dernier set.

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

Dépendances : SliderDomain immuable, slider_track_axis, ThemeBinding/State et TextService formatter.
[range_slider](range_slider.md) réutilise le domaine.

minimum/max doivent être finite et min<max ; step finite>=0. invalid_argument levé lors création du
noyau avant publication ; préserver timing et message contractuel actuel si testé.

Step supérieur au span admis, borne maximum encore accessible ; valeurs NaN/inf externes rendues
sans corruption d’état. Domaine dégénéré rejeté plutôt qu’un slider vide trompeur.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/slider.hpp` et `src/slider.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Déplacer noyau non template hors widgets_slider.inc. Préserver SliderVisualState, SliderOrientation,
SliderStyle, Formatter et fonctions publiques déjà visibles ; slider_style.hpp reste compatible.

Le helper SliderDomain reste privé et partagé avec RangeSlider ; pas de dépendance du header public
au runtime Skia.

Inscrire `src/slider.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`slider_domain` : min/max/step invalides rejetés, step=0 continu et large steps corrects.

`slider_external` : mount/paint de NaN/inf/hors plage ne réécrit pas State.

`slider_axis` : horizontal/vertical et formatter donnent la même position au paint et hit-test.

`slider_keys` : flèches/Home/End et Shift continus produisent les incréments exacts.

`slider_cancel` : cancel garde dernière valeur live ; ReadOnly arrête capture sans write.

`slider_throw_remove` : observer retire/ lève et formatter lève ; next input/frame récupèrent.

Ajouter `examples/features/slider.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
