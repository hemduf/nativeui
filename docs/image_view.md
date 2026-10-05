# ImageView

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Composant déclaratif d’image raster ou SVG conservant les couleurs du document, avec fit et mesure intrinsèque. Distinct de la ressource Image et de l’icône monochrome IconView.

NativeUI a [Image](../include/nativeui/image.hpp), [SvgIcon](../include/nativeui/svg.hpp), caches et CanvasContext2D.draw_image/draw_svg dans [component_base.hpp](../include/nativeui/component_base.hpp). Aucun ImageView public actuel ; SVG existant utilise centered contain.

MyGo : `ui/widgets.go`, `ImageSource`, `Image`, `Element.intrinsicSize`, `Element.Fit`. Bitmap ou SVG, taille naturelle en DIP et fit configuré ; Icon est séparé pour teinte. Il faut assembler les ressources préparées sans copier la politique de stockage Go.

## 2. API publique et composition

API cible proposée :

```cpp
class ImageView {
public:
    using Source = std::variant<Image, SvgIcon>;
    explicit ImageView(Image value);
    explicit ImageView(SvgIcon value);
    explicit ImageView(Binding<Source> value);
    explicit ImageView(State<Source>& value);
    ImageView&& fit(ImageFit value) &&;
    ImageView&& size(Size value) &&;
    ImageView&& pixel_scale(double value) &&;
    ImageView&& alt(std::string value) &&;
    ImageView&& decorative(bool value = true) &&;
    Spec spec() &&;
};
```

Défauts : centered Contain, sans size explicite, pixel_scale=1, alt vide et decorative=true. `pixel_scale` convertit seulement taille intrinsèque raster pixels→logique ; pour SVG intrinsic_size est déjà logique et pixel_scale n’a pas d’effet.

Exemple cible proposé :

```cpp
ui::Image photo;
auto view = ui::ImageView{photo}
    .fit(ui::ImageFit::Cover)
    .size({96.0f, 64.0f})
    .alt("Aperçu du document")
    .decorative(false)
    .spec();
```

## 3. État, propriété et notifications

Le composant possède un handle copyable Image/SvgIcon ou un Binding de Source ; State converti au constructeur. Les handles partagent leur backing et ne gardent pas un buffer encodé emprunté.

Update Source prépare dimensions/snapshot puis publie, invalide layout si taille intrinsèque change et paint/semantics. Une ressource remplacée pendant paint reste en snapshot indépendant jusqu’à la fin du paint courant.

Binding invalide après destruction source conserve dernière ressource lisible, observe inactif sans notification automatique ; pas de setter ni on_change. Charger des bytes et gérer fin async restent à l’application, hors paint.

## 4. Interactions

Aucun input, focus, capture, activation ou drag/drop implicite. Pointeur, molette, clavier et texte Ignored ; un Button/Link parent fournit l’interaction éventuelle.

Aucune validation/annulation. Afficher une photo ne crée pas de menu contextuel ni accès fichier/réseau à la volée.

Ne pas utiliser le rectangle de pixels comme proxy d’un lien. Sémantique image et actions de parent restent séparées.

## 5. Mesure et layout

Mesure naturelle = Image.size()/pixel_scale ou SvgIcon.intrinsic_size ; size explicite donne préféré indiqué en coordonnées logiques. Ne pas assimiler pixels raster au framebuffer DPR.

Fill étire aux bounds, Contain garde ratio et centre avec bandes libres, Cover garde ratio et coupe les côtés dans les bounds. Mesure reste identique entre fit modes ; seul mapping peinture change.

SVG requiert extension privée du draw adapter pour Fill/Cover ; ne pas prétendre que le draw_svg actuel les offre. Raster réutilise draw_image. Destination nulle/nonfinie = no-op, clipping équilibre même sur exceptions.

## 6. Présentation et invalidation

ImageView conserve couleurs et alpha originaux ; pas de tint, Theme image slot ou checker imposé. Une transparence laisse voir fond composé par l’application.

Changement fit/Source = paint ; dimensions/pixel_scale = layout et paint. Pas d’animation SVG ou GIF implied : ressources statiques telles que preparées.

Aucun repaint continu ni decode lors de paint. Le rendu resource/backend utilise des scopes RAII de clip/transform pour isoler chaque image de ses frères.

## 7. Accessibilité

decorative=true : None par défaut. decorative=false : SemanticRole::Image, nom alt, aucune action ; alt sert uniquement à l’accessibilité et n’est pas un fallback texte visible.

Une Image informative doit donner alt non vide ; alt vide autorisé mais documenté comme image sans nom, à détecter par exemple self-test applicatif. Binding changé met à jour resource bounds sans attribuer un nouvel id à chaque frame.

Hooks/rôle existent ; ponts T068 différés. Aucun IME ni capacité OCR/native annoncée.

## 8. Cycle de vie et récupération

UI/main-thread pour Binding/composition et préparation resource selon contrats Image/SvgIcon. Handles gardent backing valide même si cache préparateur est vidé.

Snapshot Source acquis avant measure/paint et aucun pointeur raw backend retenu dans le composant. Erreur adapter/render restaure scopes puis propagation C++ après invariants ; aucun retry automatique de callback.

Destruction no-throw, subscription RAII, aucune fonction de chargement appelée au démontage. Aucune donnée/resource registry mutable globale nouvelle ; deux UI peuvent afficher handles distincts de mêmes noms.

## 9. Dépendances et cas limites

Dépend des ressources Image/SvgIcon et adapters Painter existants. ResourceProvider/ImageCache/SvgCache sont préparés/possédés par l’application ; le provider doit vivre selon le contrat de son cache, pas celui du widget.

Handle invalide : aucun pixel, naturel {0,0}, size explicite toujours conservée ; pas de decode/réseau fallback. SVG external file/network restent interdits par ressource v1.

pixel_scale doit être fini strictement positif ; size explicite composantes finies >=0, sinon invalid_argument. SVG intrinsic invalide est déjà ressource invalide. Source valide→invalide recalcule naturel sans hit area interactive fantôme.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/image_view.hpp` et `src/image_view.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : image.hpp, svg.hpp et adapters draw_image/draw_svg. Préserver Image/ImageTexture/ImageCache et SvgIcon/SvgCache dans leurs headers historiques. Source variant et ImageView restent dans image_view.hpp ; privée extension SVG fit reste derrière adaptateur backend, sans type Skia dans widget public.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `image_view_intrinsic_scale` : taille raster/pixel_scale et SVG logique correspondent à mesure.
- `image_view_fit_modes` : Fill/Contain/Cover raster et SVG sont bornés et centrés.
- `image_view_invalid_swap` : resource invalid puis replacement async préparé recalcule sans I/O paint.
- `image_view_alpha` : transparence originelle et absence de tint respectées.
- `image_view_cache_lifetime` : vider un cache ne retire pas le backing retenu par une vue.
- `image_view_clip_fault` : exception de rendu puis image voisine gardent transformations.
- `image_view_semantics` : decorative/alt donnent rôle et nom attendus.

Créer `examples/features/image_view.cpp` et la cible `nativeui_example_image_view`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
