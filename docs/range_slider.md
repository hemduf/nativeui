# RangeSlider

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer un intervalle low/high dans un domaine : coupe de fréquence ou plage. Une publication unique
du RangeValue préserve low<=high pour un geste.

NativeUI : [widgets_range_slider.inc](../include/nativeui/detail/widgets_range_slider.inc),
RangeValue {float low, float high}, RangeSlider et thumb actif. Le domaine est celui de
[slider_value.hpp](../include/nativeui/detail/slider_value.hpp).

MyGo : `ui/indicators.go`, `RangeSlider` ; MyGo emploie deux pointeurs float64. NativeUI possède
déjà un binding agrégé ; conserver cette différence qui évite deux notifications séparées. Cible :
compléter accès clavier aux deux handles.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
struct RangeValue { float low{}; float high{1.0f}; };
explicit RangeSlider(Binding<RangeValue> state);
explicit RangeSlider(State<RangeValue>& state);
RangeSlider&& range(float minimum, float maximum) &&;
RangeSlider&& step(float value) &&;
RangeSlider&& orientation(SliderOrientation value) &&;
RangeSlider&& style(SliderStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<ui::RangeValue> band{ui::RangeValue{0.2f, 0.8f}};
auto range = ui::RangeSlider(band).range(0.0f, 1.0f).step(0.05f).spec();
```

RangeValue::operator== existant reste inchangé. La variante clavier ajoute Entrée pour changer le
handle actif sans écrire le Binding ; elle n’ajoute ni deuxième composant public ni changement des
constructeurs.

Defaults conservés : range[0,1], step=0, orientation horizontal ; les numerics float publics restent
stable.

## 3. État, propriété et notifications

Binding<RangeValue> possède les deux bornes ; poignée active est un état local. Low/high externes
non finite prennent respectivement minimum/maximum, puis clamp et swap pour géométrie uniquement,
sans set.

Une interaction normalize la poignée choisie ; lower ne dépasse pas high effectif, upper ne descend
pas sous low effectif. set publie un RangeValue entier seulement si différent.

Choix pointeur par distance au target continu avant snap ; égalité choisit poignée active
précédente, sinon lower. Ne pas utiliser grille step pour décider l’identité de poignée.

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

PointerDown choisit la poignée la plus proche et publie live ; Move/Up suivent cette poignée avec
capture. Les poignées ne se croisent pas : elles se bloquent contre l’autre borne.

Clavier existant : flèches et Home/End changent la valeur du handle actif, sans changer son identité (lower par défaut) ; Home
lower=min/upper=low, End lower=high/upper=max. Incréments comme Slider (step ou span/100, Shift
span/1000).

Extension cible : Entrée bascule le handle actif, filtre autorepeat jusqu’au KeyUp, sans modifier la
valeur ; annonce le handle actif. Tab demeure un arrêt sur le RangeSlider, puis sort.

Cancel termine drag sans rollback de valeur live. ReadOnly bloque les setters mais permet choix du
handle/focus ; molette ignorée. Disabled annule interaction sans inventer un intervalle initial.

## 5. Mesure et layout

Même SliderTrackAxis et insets pour paint/hit-test ; orientation verticale inverse la fraction comme
Slider. Segment sélectionné peint entre low/high effectifs.

Les deux thumbs gardent taille fixe et peuvent se superposer si low==high. La source actuelle peint lower puis upper et dessine le focus ring sur les deux ; cible enrichie : peindre le handle actif en dernier et réserver son indication active pour rester lisible. Le prochain clic à égalité garde l’identité active.

Contrainte d’axe nul gérée sans division zéro. Les bornes de clipping et calculs pointer restent en
coordonnées logiques lors resize et scale.

## 6. Présentation et invalidation

Style partagé SliderStyle, avec visual_state pour le contrôle et accent du handle actif/focus.
RangeValue change repaint seulement à métriques identiques.

Selection de handle via Entrée repaint le ring/indicateur du thumb actif, pas layout ; la taille
n’est pas différente entre poignées.

Aucun double Binding ni callbacks audio ; la vue reflète toujours un snapshot pair accepté.

## 7. Accessibilité

Cible : Group avec deux enfants RangeSliderHandle nommés “Minimum”/“Maximum”, numeric_value, range
ajusté par l’autre borne et actions Increment/Decrement/SetValue.

Les enfants sémantiques portent identités stables pendant la vie du RangeSlider ; action
Focus/Select choisit le handle. La source actuelle ne publie pas ces semantics ni deux enfants :
enrichissement à réaliser.

La virtualisation spéciale ListView n’est pas réutilisée pour deux handles : créer deux sous-parties retenues internes dans le même couple range_slider.hpp/cpp, avec nœuds sémantiques simples backend-neutres et sans pointeurs vers thumbs. Aucune extension de VirtualSemanticChildren n’est nécessaire pour ce contrôle.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer la paire et la fin de capture avant State.set ; observateur retire le widget : aucune
relecture de poignée/composant après notification.

Après exception d’observer, paire publiée reste autoritative et poignée active reste valide ou reset
si démonté ; aucune restauration partielle de low seulement.

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

Dépendances : [slider](slider.md), SliderDomain, slider_track_axis, State/ThemeBinding ; complément
sémantique dans services communs.

Range min<max finite, step>=0 finite identique Slider. Pair externe inversé/NaN/hors plage : rendu
sûr sans corriger automatiquement l’application.

Nouvelle écriture externe pendant drag est relue comme paire courante ; la poignée choisie demeure
identique. Si l’autre borne se déplace, prochaine valeur est bornée contre elle, sans gesture caché.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/range_slider.hpp` et `src/range_slider.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Préserver RangeValue, RangeSlider, SliderStyle/Orientation et includes historiques ; déclarations
dans range_slider.hpp, noyau non template/peinture/input dans range_slider.cpp.

L’ajout clavier et les enfants sémantiques ne remodèlent pas le State public et ne créent pas un
.cpp par handle.

Inscrire `src/range_slider.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`range_slider_pair` : toute écriture garde low<=high ; une seule notification pair.

`range_slider_nearest` : distance continue choisit poignée ; ties gardent active avant snap.

`range_slider_keyboard_handles` : Entrée change active, flèches/Home/End éditent bornes correctes ;
Tab sort.

`range_slider_external_invalid` : swap/clamp/render de valeurs externes sans set au mount/paint.

`range_slider_cross_cancel` : handles ne croisent pas ; Cancel ne rollback pas la paire live.

`range_slider_semantic_ids` : deux IDs stables et actions ajustées à l’autre borne.

`range_slider_remove_throw` : observateur réentrant/ levant ne laisse capture ni handle périmé.

Ajouter `examples/features/range_slider.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
