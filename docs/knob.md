# Knob

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Contrôle rotatif continu de valeur float pour des interfaces quelconques. Un domaine musical peut l’utiliser ; l’automation et les gestes audio restent dans l’adapter applicatif.

Présent dans [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), `Knob(label, Binding<float>)` et surcharge State. `KnobComponent` public dans [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). Il reçoit une plage, un drag vertical et des flèches clavier.

MyGo n’a pas de Knob autonome dans les 54 familles ; `Slider` de `ui/widgets.go` fournit un comportement continu comparable mais une présentation différente. Préserver l’extension NativeUI.

## 2. API publique et composition

API existante exacte : `Knob(std::string, Binding<float>)`, `Knob(std::string, State<float>&)`, fluent rvalue `range(float minimum, float maximum)` et `spec() &&`. Aucun `on_change`, formatter ou style n’existe actuellement.

Exemple existant vérifié :

```cpp
ui::State<float> amount{0.5f};
auto control = ui::Knob{"Quantité", amount}
    .range(0.0f, 1.0f)
    .spec();
```

La surcharge State est convertie immédiatement en Binding ; après destruction de sa source, valid()==false, get() fournit la dernière valeur, set est ignoré et observe inactif. Aucune notification de destruction automatique ; les événements revalident valid(). Le constructeur public KnobComponent conserve label/Binding/plage ; pas de migration float vers double dans cette extraction.

## 3. État, propriété et notifications

Binding<float> possédé et valeur visuelle locale. Au montage, subscription par instance ; chaque écriture externe actualise la valeur clampée et invalide le rendu.

Source actuelle : value_ reçoit state_.get() sans clamp au constructeur ; les notifications observe clampent ensuite. L’arc normalise au rendu, mais le texte peut initialement montrer une valeur hors plage. Contrat enrichi : initialisation et updates externes utilisent la même valeur effective clampée, sans writeback ; les mutations utilisateur écrivent une valeur clampée.

Le drag conserve sa valeur initiale et le déplacement total. Contrat enrichi : une mise à jour externe pendant drag annule la gesture pour éviter une réécriture depuis ancien point de départ. Les observations confirmant l’écriture attendue du Knob ne comptent pas comme externes ; cette reconnaissance est scoped et restaurée même si set/observer lève, une valeur réentrante différente annule. Tester la distinction lors de l’enrichissement.

## 4. Interactions

PointerDown commence DragGesture et capture. Mouvement vertical vers le haut augmente la valeur ; sensibilité actuelle = plage/180 pixels logiques. PointerUp termine et libère.

Flèches gauche/bas décrémentent et droite/haut incrémentent, pas = 1 % de plage ; Shift = 0,2 %. Les autres touches/molette sont Ignored ; pas de reset double clic implicite.

ReadOnly consomme les événements mutateurs sans écriture et annule le drag en cours. PointerCancel/désactivation/démontage libèrent la capture et gardent la dernière valeur publiée, sans rollback ni notification ajoutée.

## 5. Mesure et layout

Mesure historique 176 × 182. Le parent peut contraindre ; contrôler clipping et géométrie du knob dans un petit espace sans changer sa taille demandée.

Le rendu actuel réserve titre, arcs et valeur à deux décimales. Garder cette présentation normale comme oracle ; aucune mesure de chaîne ne déclenche une mutation.

Coordonnées logiques ; le DPR appartient au renderer. La normalisation doit rester finie pour toute plage prise en charge, et ne jamais donner NaN à arc/line.

## 6. Présentation et invalidation

Couleurs et géométrie actuelles depuis colors : panel, border, accent, knob et knobInner. Focus remplace la bordure ; pas de KnobStyle existant.

Valeur externe/interne et focus = repaint. La plage et le label sont immuables après création. L’extraction ne promet pas une animation ou un effet matériel nouveau.

Plage maximum <= minimum : préserver maximum = minimum + 1 pour valeurs ordinaires. Contrat enrichi : bornes non finies rejetées par invalid_argument ; si minimum+1 float ne progresse pas, utiliser nextafter(minimum,+inf), rejet si résultat non fini. Valeur source non finie a pour vue minimum, jamais une géométrie invalide.

## 7. Accessibilité

Contrat cible : Slider, nom = label, numeric_value = valeur effective, value_range = plage et step 1 %, actions Increment/Decrement/SetValue/Focus.

ReadOnly conserve nom/valeur et retire les mutations sémantiques. Les actions suivent le même clamp que le clavier ; aucune voie privilégiée hors plage.

Knob actuel ne démontre pas ces overrides. Vérifier la publication backend-neutre ; ponts natifs T068 différés, sans revendiquer prise en charge native.

## 8. Cycle de vie et récupération

Subscriptions RAII annulées au démontage ; ajouter un unmount explicite si nécessaire sans dépendre seulement de la destruction. Déactivation et exception ne laissent pas DragGesture active.

Appel Binding::set potentiellement réentrant : préparer valeur et état geste, écrire, puis revalider le token de vie avant tout accès au composant. Une notification commencée n’est jamais rejouée après exception.

Destruction no-throw ; pas de callback métier de fin de geste improvisé. Aucun globals, locks ou audio calls ; deux Knob simultanés restent indépendants.

## 9. Dépendances et cas limites

Réutilise Binding/State, DragGesture et focus/capture existants. Aucun contrôleur de paramètre de plugin ajouté au toolkit.

Plage large avec subtraction float potentiellement overflow : calcul du span, delta et normalisation interne en double, conversion finale float après clamp, sans modifier l’API float. Valeur NaN/inf : présentation effective minimale, aucune correction du modèle.

Une State/Binding réentrante qui retire le nœud cesse toute gesture ; une nouvelle instance ne reprend pas l’ancien drag. Le reset/reopen doit pouvoir réobserver sans garder l’ancien invalidateur.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/knob.hpp` et `src/knob.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_basic.inc` et `widgets_builders.inc`. Préserver KnobComponent et les deux constructeurs du builder ; noyau de gesture/rendu dans knob.cpp, sans fichier wrapper vide.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `knob_legacy_delta` : drag 180 unités et flèches/Shift reproduisent les incréments.
- `knob_external_drag` : écriture externe annule le drag avant autre mouvement.
- `knob_readonly_cancel` : transition ReadOnly et PointerCancel libèrent capture sans écriture.
- `knob_wide_nonfinite` : aucune géométrie non finie pour entrées extrêmes.
- `knob_observer_throw` : après observer réentrant qui lève un nouveau drag reste possible.

Créer `examples/features/knob.cpp` et la cible `nativeui_example_knob`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
