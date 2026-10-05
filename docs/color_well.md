# ColorWell

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Échantillon de couleur compact ouvrant un ColorPicker portable ancré. Valeur publique sRGB ui::Color ; aucune boîte système de couleurs ni service global.

Absent de NativeUI ; Color, Button et [Overlay](../include/nativeui/overlay.hpp) existent. [ColorPicker](color_picker.md) et [Popover](popover.md) sont les dépendances cibles.

MyGo : `ui/colorpicker.go`, `ColorWell`, preview sur checker, valeur hex, Enter/Space/clic ouvrant et Escape/outside fermant. La couleur du picker est publiée pendant l’ouverture.

## 2. API publique et composition

API cible proposée :

```cpp
class ColorWell {
public:
    ColorWell(std::string label, Binding<Color> value);
    ColorWell(std::string label, State<Color>& value);
    ColorWell&& alpha_enabled(bool value = true) &&;
    ColorWell&& swatches(std::vector<ColorSwatch> value) &&;
    ColorWell&& on_change(std::function<void(Color)> callback) &&;
    ColorWell&& style(ColorWellStyle value) &&;
    Spec spec() &&;
};
```

Défauts : alpha activé, palette vide, popup ColorPicker largeur naturelle 280 DIP et preview 36 × 20 DIP plus padding. ColorWellStyle couvre preview/chrome/focus/size, pas les préférences globales du picker.

Exemple cible proposé : `ui::ColorWell{"Couleur de piste", trackColor}.spec()`. ColorSwatch est défini dans color_picker.hpp et réutilisé, pas redéfini.

## 3. État, propriété et notifications

Binding<Color> unique partagé entre preview et ColorPicker ; open/handle privés. Surcharge State convertie en Binding. Source détruite : valid false, get dernière valeur, set ignoré et observe inactif sans notification automatique ; revalider avant ouverture/edit et ne pas émettre on_change pour un set ignoré.

Mutations du picker notifiées une fois par ColorWell.on_change ; aucune seconde subscription qui réémet chaque update. Externe repaint preview/picker mais ne notifie pas on_change.

Valeurs publiées en continu ; fermer popup ne rétablit pas la couleur initiale. L’annulation Escape concerne l’ouverture/le brouillon hex, pas un transaction rollback de Color.

## 4. Interactions

Clic primaire terminé, Enter et Space basculent popup une fois par pression. Focus va au premier contrôle du picker ; fermeture normale rend focus à l’ancre encore disponible.

Escape/clic extérieur ferment. PointerCancel annule trigger pressé ; glissement/molette sur preview ignorés. Glissement dans picker suit ses règles de capture.

ReadOnly/Disabled empêchent ouverture mutatrice ; une transition ReadOnly/Disabled/Hidden pendant popup ferme et annule capture. Aucun double clic de reset ou drag/drop couleur ajouté implicitement.

## 5. Mesure et layout

Preview rectangulaire dans chrome avec dimensions style, ratio conservé. Clipping arrondi inclut le checker et le remplissage ; bounds nuls ne donnent aucun input.

Popup ancré par NodeId, Auto et clamp du service Overlay, taille naturelle du picker indépendante de preview. Le parent déplace/scroll l’ancre sans coordonnées OS.

Viewport plus petit que picker : borne/clip son contenu selon Popover/Calendar conventions sans agrandir la fenêtre ni déborder des interactions.

## 6. Présentation et invalidation

ColorWellStyle : bordure, focus, padding/radius ; preview dessine checker puis Color normalisé de vue. L’alpha ne réduit pas la couleur de la bordure/chrome.

Update couleur = repaint/semantics ; style métrique = layout ; open/close = structure Overlay. Aucun flash de couleur par défaut avant la première lecture Binding.

Une couleur invalide externe est signalée textuellement et rendue en repli du picker ; source préservée. Pas d’animation obligatoire ni nouvelle palette de thème.

## 7. Accessibilité

Contrat cible : Button nommé label, description/valeur hex de la couleur, expanded, Activate/Focus. SemanticRole::ColorWell absent ; ne pas le prétendre livré.

Le contenu ouvert porte le nom du champ et les noms de canaux ; les actions sémantiques empruntent les règles du picker. Preview ne génère pas une deuxième Image inutile.

Ponts T068 différés. La seule saisie texte est le champ hex du picker : committed text actuel, preedit natif futur DESIGN17.4.

## 8. Cycle de vie et récupération

UI/main-thread, subscription RAII, callback popup protégé par token/génération. Le démontage retire uniquement son overlay et ne ferme pas celui d’un autre ColorWell.

Échec show/panel laisse open false et un prochain clic utilisable. Fermeture idempotente, restauration de focus seulement si ancre valide.

on_change réentrant peut rouvrir autre popup ; handle/génération précise interdit de fermer le nouveau. Callback commencé jamais rejoué ; destruction no-throw, aucun callback applicatif d’annulation improvisé.

## 9. Dépendances et cas limites

Dépend de ColorPicker/Popover/Binding et chrome Button. Aucun appel audio, filesystem ou OS color-panel.

Alpha désactivé conserve le canal source selon ColorPicker. Externe pendant drag annule le geste du picker ; clôture de popup avec hex invalide l’abandonne sans nouvelle couleur.

Palette vide valide ; ids dupliqués refusés par picker. Source Binding supprimée devient non mutatrice à la prochaine revalidation ; ancre retirée = fermeture, pas relocalisation au centre.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/color_well.hpp` et `src/color_well.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Color/Overlay existants et ColorPicker cible. Importer ColorSwatch depuis son parent ; le noyau du well possède ouverture/preview, tandis que HSV/parser restent exclusifs au picker.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `color_well_live_color` : preview et picker partagent une valeur avec une seule notification.
- `color_well_escape_keeps` : fermeture Escape garde dernière couleur validée.
- `color_well_focus_anchor` : open/close et ancre supprimée ne focalisent jamais un nœud stale.
- `color_well_alpha_preview` : alpha zéro/partiel laisse checker visible.
- `color_well_two_instances` : deux wells simultanés isolent handles, teinte et subscriptions.
- `color_well_show_failure` : échec d’ouverture puis nouveau clic fonctionnent.

Créer `examples/features/color_well.cpp` et la cible `nativeui_example_color_well`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
