# ColorPicker

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Sélecteur sRGB avec carré saturation/valeur, teinte, alpha, champ hexadécimal, preview et palette nommée. Le modèle public reste ui::Color ; HSV est uniquement une représentation UI interne.

Absent de NativeUI ; [geometry.hpp](../include/nativeui/geometry.hpp) définit Color, Painter dispose de gradients, Slider/TextInput et Binding existent.

MyGo : `ui/colorpicker.go`, `ColorPicker`, `toHSVA`, `hsva.color`, `hexOf`, `pickerState`, `channelSlider`, `checkers`. Conservation de teinte sur gris locaux, synchronisation externe, alpha/hex et swatches. MyGo utilise uint8 ; cible garde les composantes flottantes de ui::Color.

## 2. API publique et composition

API cible proposée :

```cpp
struct ColorSwatch { std::string id; std::string name; Color value; };
class ColorPicker {
public:
    ColorPicker(std::string label, Binding<Color> value);
    ColorPicker(std::string label, State<Color>& value);
    ColorPicker&& alpha_enabled(bool value = true) &&;
    ColorPicker&& swatches(std::vector<ColorSwatch> value) &&;
    ColorPicker&& on_change(std::function<void(Color)> callback) &&;
    ColorPicker&& style(ColorPickerStyle value) &&;
    Spec spec() &&;
};
```

Défauts : alpha activé, swatches vide pour ne pas imposer une palette, carré SV puis teinte/alpha/hex, largeur préférée 280 unités logiques. ColorPickerStyle couvre carré, thumb, checker, textes, gaps/padding et swatch size.

Exemple cible proposé : `ui::ColorPicker{"Accent", accent}.alpha_enabled().spec()` avec State<Color> détenu par l’application. Tous les nouveaux nombres d’options/style sont double ; Color reste son type existant.

## 3. État, propriété et notifications

Binding<Color> externe et surcharge State convertie immédiatement. Source détruite : valid=false, get dernière couleur, set ignoré/observe inactif sans notification automatique ; revalider avant edit et ne pas notifier on_change. HSV double privé, dernier Color effectif, brouillon hex et capture appartiennent à l’instance. sRGB non linéaire des canaux [0,1] ; alpha [0,1] linéaire.

HSV est calculé sur ces canaux sRGB, pas après conversion gamma. Une transition utilisateur vers gris conserve la dernière teinte utile ; une couleur externe grise conserve aussi cette teinte de l’instance sans écrire Color.

Chaque mutation utilisateur modifie Color effectif puis on_change une fois si distinct ; observations de l’écriture attendue reconnues par scope récupérable ne réinitialisent pas la teinte. Externe différente met à jour HSV/preview, annule gesture et remplace brouillon sans callback. Aucune quantization8 bits sauf saisie/format hex.

## 4. Interactions

Carré SV : clic/drag capturé, x=S, y=1-V, clamp aux bounds. Flèches droite/gauche ±0,01 S et haut/bas ±0,01 V ; Shift divise le pas par 10. Teinte/alpha utilisent le comportement clavier des sliders.

Swatch activé par clic/Enter/Space ; champ hex accepte exactement #RRGGBB ou #RRGGBBAA. Commit Enter/perte focus seulement si parse valide ; syntaxe invalide reste locale et ne modifie pas Color ; Escape restaure dernier hex valide.

PointerCancel garde dernière couleur publiée et libère capture. Externe pendant drag annule drag avant synchronisation. ReadOnly retire mutations mais conserve lecture/navigation ; aucune molette implicite sur carré.

## 5. Mesure et layout

Mesure : carré SV ratio 1:1, deux lignes de sliders si alpha, preview/hex et swatches en grille à colonnes bornées. Le parent contraint largeur ; min taille garantit zones utilisables ou clipping sans overlap.

Calcul SV avec largeur/hauteur strictement positives ; zéro interdit mutation plutôt que diviser. Rectangles swatches et pointer hit suivent la grille calculée.

Coordonnées logiques ; le checker alpha est clipé à la preview et au slider alpha. Pas de dimension framebuffer exposée ni d’accès écran.

## 6. Présentation et invalidation

ColorPickerStyle nouveau pour surfaces, labels et focus ; couleur sélectionnée ne devient pas une variable globale de thème. Checker clair/foncé rend alpha visible et garde une description textuelle.

Changement couleur repaint des surfaces dépendantes, update texte/semantics ; hue modifie gradients sans relayout. Swatches/style métrique changent layout/structure.

Hex formatage = lowercase, #rrggbb si alpha exactement 1, sinon #rrggbbaa ; rondeau byte = round(clamp(channel)*255). Format hex ne doit pas réécrire le modèle ni perdre ses fractions float.

## 7. Accessibilité

Contrat cible : Group nommé ; carré Custom décrit S et V, sliders Slider pour H/alpha avec ranges ; swatches Button nommés, champ TextInput nommé « Hexadécimal ». Pas de Role::ColorPicker existant.

Fournir valeurs descriptives en pourcentage et hex, pas seulement couleur visuelle. Actions sémantiques mutatrices partagent validation/clamp et respectent ReadOnly.

Ponts T068 différés. Champ hex utilise committed text ; IME preedit/candidate rectangles restent dépendance DESIGN17.4 et ne sont pas revendiqués.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement RAII, capture de SV/sliders détenue par le système input. Chaque update utilise une snapshot de la couleur source, sans références à ses canaux conservées.

Préparer conversion/parse avant écriture ; réentrance Binding/on_change peut masquer/démonter, donc ne toucher contexte ou instance après callback sans token valide.

Exceptions restauration capture/guards puis propagation C++ ; callback commencé jamais rejoué. Destruction no-throw annule edits/capture et abonnements sans on_change. Pas de service couleur OS ni état global mutable.

## 9. Dépendances et cas limites

Dépend de [Slider](slider.md), [TextInput](text_input.md), Button, Painter/gradients et Binding. La palette est fournie par valeurs possédées et ids stables.

External Color non fini : afficher invalide avec couleur effective de repli {0,0,0,1} sans writeback ; externe hors [0,1] clamp uniquement pour vue. Nouveau geste écrit une couleur finie normalisée.

alpha_enabled=false masque l’alpha et conserve le canal source lors des changements HSV/#RRGGBB ; #RRGGBBAA est refusé dans ce mode. IDs swatch vides/dupliqués = invalid_argument avant publication ; retrait de swatch pressé annule son action.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/color_picker.hpp` et `src/color_picker.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Color/Painter/Binding existants. ColorSwatch et ColorPickerStyle restent dans le header du parent ; HSV et parser privés dans color_picker.cpp, aucune API Skia ou système.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `color_picker_srgb_hsv` : primaires et aller-retour sRGB/HSV respectent la tolérance float.
- `color_picker_gray_hue` : perte saturation puis restauration garde teinte par instance.
- `color_picker_hex_validation` : formes exactes, erreurs et arrondi hex sans writeback.
- `color_picker_alpha_disabled` : canal alpha conserve sa précision et refuse hex alpha.
- `color_picker_external_drag` : update externe annule capture sans réécriture stale.
- `color_picker_remove_swatch` : palette modifiée pendant pression ne choisit pas un autre swatch.
- `color_picker_throw_recover` : callback qui lève ne rejoue pas et prochaine couleur reste possible.

Créer `examples/features/color_picker.cpp` et la cible `nativeui_example_color_picker`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
