# IconView

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Affichage déclaratif de SvgIcon, monochrome teinté par défaut, taille relative au texte et ratio préservé. Distinct de SvgIcon qui est une ressource préparée et de ImageView multicolore.

La ressource [SvgIcon](../include/nativeui/svg.hpp) et draw_svg existent ; [skia_svg.cpp](../src/skia_svg.cpp) rend le DOM sans teinte. Il n’existe ni IconView public ni overload monochrome actuel.

MyGo : `ui/svg.go`, `Icon` et `Painter.Icon` ; `ui/widgets.go`, intrinsicSize. Icône à hauteur de police, ratio SVG, teinte TextColor quels que soient fills, décorative sauf label. Cette teinte est une extension du rendu NativeUI à implémenter.

## 2. API publique et composition

API cible proposée :

```cpp
class IconView {
public:
    explicit IconView(SvgIcon value);
    explicit IconView(Binding<SvgIcon> value);
    explicit IconView(State<SvgIcon>& value);
    IconView&& size(double height) &&;
    IconView&& color(Color value) &&;
    IconView&& monochrome(bool value = true) &&;
    IconView&& alt(std::string value) &&;
    IconView&& decorative(bool value = true) &&;
    Spec spec() &&;
};
```

Défauts : hauteur Theme typography.control_size, couleur Theme.text, monochrome=true, decorative=true, alt vide. Color ne s’applique qu’en mode monochrome ; mode false conserve exactement couleurs du SVG.

Exemple cible proposé :

```cpp
ui::SvgIcon saveIcon;
auto icon = ui::IconView{saveIcon}
    .size(16.0)
    .color(ui::Color{0.2f, 0.4f, 0.9f, 1.0f})
    .spec();
```

## 3. État, propriété et notifications

SvgIcon handle possédé/copié, backing partagé stable ; overload Binding observé, State immédiatement converti. Couleur/size/alt sont des valeurs possédées.

Update resource change intrinsic ratio donc layout et paint ; changer la couleur de thème n’affecte que teinte implicite, jamais la donnée SVG partagée.

Source détruite : Binding invalid, get dernier handle et observe inactive sans notification automatique ; aucune set ni callback on_change. Ressource invalid après update est sûre à mesurer/peindre.

## 4. Interactions

IconView ignore pointeur, drag, molette, clavier, texte et drops. Aucun focus/capture ; placé dans Button il ne détourne pas activation ni nom du Button.

Pour une icône cliquable, l’application compose Button avec son nom. alt informative ne rend pas l’image interactif.

Validation/annulation non applicables. Rotation/animation ne sont pas incluses implicitement ; Spinner est le composant d’activité dédié.

## 5. Mesure et layout

Hauteur spécifiée ou typographique, largeur = hauteur × ratio intrinsic_size. Ressource invalide : carré de hauteur choisie comme placeholder de layout, aucun pixel ; pas de division par zéro.

Bounds finaux utilisent centered Contain, clipés et sans étirer les shapes. Le DPR est traité par le renderer ; la taille publique reste logique.

size0 = absence pixels avec mesure nulle, et aucune rect invalide passée au backend. Largeur parent réduite peut réduire l’image au contain sans modifier ratio demandé.

## 6. Présentation et invalidation

Monochrome cible : utiliser masque alpha de rendu SVG et appliquer Color uniforme, en conservant alpha original des formes × alpha Color. Ne pas modifier fills/strokes du DOM partagé à chaque paint.

Extension privée de l’adapter SVG nécessaire ; source actuelle ne la fournit pas. Le masque/backend cache éventuel dépend de backing identity, destination/résolution et couleur, ownership instance/contexte explicite, jamais namespace global mutable.

Theme change réévalue size seulement si hauteur implicite, color seulement si teinte implicite. Pas de IconViewStyle requis pour les options fixées ici, ni slot Theme imaginaire.

## 7. Accessibilité

Décorative : SemanticRole::None. Informative : Image avec name=alt et aucune action ; ce rôle existe, pas Role::Icon.

Le parent Button conserve son nom applicatif ; ne pas automatiquement concaténer alt et label et lire deux fois « Enregistrer ». Une icône informative sans alt est explicitement sans nom.

Ponts T068 différés. Pas d’IME ni annonce live liée à pixel/resource.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement et cache du widget/renderer ont RAII et weak invalidateur. Source buffer original peut disparaître après SvgIcon.parse conformément à ressource.

Masque/tint préparés sans muter shared DOM : exception restitue save/clip/transform et laisse une prochaine peinture possible. Pas de raw SvgData* conservé sans handle owner.

Destruction no-throw et callback-silent ; clearing un cache d’une UI ne retire pas resource toujours partagée par une autre. Aucune registration OS/module-global.

## 9. Dépendances et cas limites

Dépend de SvgIcon/Painter/private adapter, Theme et Binding. Aucun chargement ResourceProvider/XML dans paint ; SVG animé ou externe reste hors ressource v1.

size non finite/négative = invalid_argument ; zéro valide. Couleur non finie = invalid_argument à Spec, composants hors [0,1] clamp pour teinte vue sans modifier options source.

monochrome(false) ignore color explicite comme décidé, preserve image-native colors ; ImageView couvre fit Fill/Cover pour des SVG illustratifs. Fichier absent/parse échoué donnent handle invalide, pas bouton de retry invisible.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/icon_view.hpp` et `src/icon_view.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : svg.hpp et adapter privé de src/skia_svg.cpp. SvgIcon/SvgCache gardent leurs API historiques. L’adapter monochrome reste backend privé ; icon_view.cpp contient mesure/composition/tint request, pas un simple fichier include vide.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `icon_view_ratio_size` : hauteur typo/explicite et ratio SVG corrects.
- `icon_view_monochrome_alpha` : formes rouges/bleues deviennent la teinte demandée avec alpha conservé.
- `icon_view_native_colors` : monochrome false conserve les couleurs d’origine et ignore tint.
- `icon_view_shared_resource` : deux vues différemment colorées ne mutent pas DOM de l’autre.
- `icon_view_invalid_clip` : SVG invalide et bounds zéro ne peignent rien ni polluent scope.
- `icon_view_accessible_icon` : decorative et alt exposent exactement les rôles attendus.

Créer `examples/features/icon_view.cpp` et la cible `nativeui_example_icon_view`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
