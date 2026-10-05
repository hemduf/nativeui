# Avatar

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Avatar circulaire montrant une image raster si valide, sinon des initiales du nom sur un fond déterministe. Couvre profils, auteurs et items de collaboration sans service utilisateur réseau.

Absent de NativeUI ; [Image](../include/nativeui/image.hpp), TextService et Painter fournissent les fondations. [ImageView](image_view.md) permet le crop Cover mais Avatar possède sa propre logique de fallback.

MyGo : `ui/indicators.go`, `Avatar`, `initials`, `hslColor`, hash FNV-1a du nom pour hue. Cible conserve image Cover, fallback stable et rôle image ; Unicode doit être explicitement sûr plutôt qu’un slice byte.

## 2. API publique et composition

API cible proposée :

```cpp
class Avatar {
public:
    explicit Avatar(std::string name);
    explicit Avatar(Binding<std::string> name);
    explicit Avatar(State<std::string>& name);
    Avatar&& image(Image value) &&;
    Avatar&& image(Binding<Image> value) &&;
    Avatar&& image(State<Image>& value) &&;
    Avatar&& size(double diameter) &&;
    Avatar&& initials(std::string value) &&;
    Avatar&& style(AvatarStyle value) &&;
    Spec spec() &&;
};
```

Défauts : diamètre32 DIP, image absente, initials automatique, sémantique informative avec nom. AvatarStyle nouveau : TextStyle initials_text, optional background/foreground, border_width/color ; foreground automatique garantit contraste avec fond choisi.

Exemple cible proposé : `ui::Avatar{"Camille Martin"}.size(32.0).spec()` ; name et image dynamiques peuvent observer deux Bindings indépendants.

## 3. État, propriété et notifications

Name/image/static initials/style sont possédés ; overload State convertissent aussitôt en Binding. Données du compte/contact externes restent à l’application.

Observer name recalcule initiales/fond/semantics ; image update choisit view/fallback. Les deux observers publient snapshots indépendants au checkpoint ; pas de callback on_change ou loader intégré.

Source Binding détruite : get garde dernière name/image, valid=false et observe inactive sans notification destruction. Aucune tentative de dereference utilisateur ou reset des initials à zéro ; backing Image encore partagé peut rester visible.

## 4. Interactions

Display-only : aucun focus, capture, activation, molette, glissement ni drop. Tous les inputs Ignored ; Button/ContextMenu peuvent l’envelopper explicitement.

Pas d’action « changer photo » automatique ni fichier ouvert au double clic. La validation d’identité/login est hors du composant.

Annulation non applicable. ReadOnly/Disabled du parent n’effacent pas le nom ; Disabled peut atténuer bordure/texte de fallback via style.

## 5. Mesure et layout

Mesure carré diamètre ; layout centre un disque de diamètre min(bounds.w,bounds.h,diamètre). Image Cover recadre au centre dans un clip circulaire, jamais un carré aux coins visibles.

Initiales centrées avec TextService, taille automatique0,4×diamètre lorsque initials_text ne fournit pas d’override ; longue override initials est clipée et n’élargit pas Avatar.

Dimensions logiques ; diamètre zéro donne aucune pixel/mesure et aucune opération de clip invalide. Place rectangulaire n’étire pas le cercle en ellipse.

## 6. Présentation et invalidation

Fallback initiales automatiques v1 : découper mots par espaces/punctuation ASCII et frontières Unicode valides ; prendre premier grapheme du premier/dernier mot, un seul si un mot. Capitaliser ASCII a-z seulement, laisser autres graphemes sans transformation approximative.

Nom sans grapheme de lettre/chiffre utilisable ou vide : « ? ». Override initials explicite, y compris chaîne vide, remplace l’auto ; la valeur sémantique reste name.

Fond automatique = hue FNV-1a32 des bytes UTF-8 réparés du name, saturation0,45/lightness0,55 ; calcul interne sRGB. Foreground automatique noir ou blanc selon meilleur contraste WCAG de ces deux candidats ; style explicite peut remplacer ces choix sans cache global.

## 7. Accessibilité

Contrat cible : SemanticRole::Image nommé name ; pas de Text séparé pour initials déjà représentant ce nom. Image/fallback ne changent pas identité sémantique à chaque remplacement.

Nom vide = description « avatar sans nom » et valeur name vide ; l’application doit fournir nom informatif. Ne pas annoncer « Camille Martin » deux fois avec le Label voisin si composition l’exclut explicitement.

Ponts T068 différés. Aucun IME, données de profil native ou annonce de changement photo implicitement livrés.

## 8. Cycle de vie et récupération

UI/main-thread ; subscriptions séparées RAII, copy Image backing et name snapshot avant paint. Contextes de paint/measure sont empruntés pendant callback seulement.

Fallback préparé avant publication name ; allocation/mesure échouée laisse dernier snapshot cohérent. Crop clip/scopes doivent s’équilibrer si backend image ou text lève.

Destruction no-throw sans notification/load cancel métier ; chargeur async externe utilise token applicatif avant State update. Retrait du nœud rend invalidateurs stale no-op et ne conserve pas application en vie indéfiniment.

## 9. Dépendances et cas limites

Dépend de Image/TextService/Binding et clip Painter, pas du modèle de comptes/Sidebar. Decode/ResourceProvider préparés en amont, pas à la première paint.

Image invalide après échec load = initials ; remplacement valide revient à image sans cache d’erreur global. Image alpha transparent montre fond fallback ; image valide même totalement transparente ne redonne pas initials.

Diamètre/border_width nonfinis ou négatifs = invalid_argument ; zéro valide. UTF-8 invalide réparé pour hash/initials/texte identiquement. Grapheme composé/emoji peut être utilisé comme override initials ; la détection auto respecte frontières et ne coupe pas les bytes.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/avatar.hpp` et `src/avatar.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Image/State/TextService/Painter existants. AvatarStyle et options restent dans avatar.hpp ; vrai fallback/crop/layout/observe dans avatar.cpp. Réutiliser draw_image Cover sans dupliquer decode/cache ni introduire types Skia publics.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `avatar_image_fallback` : Image valide/invalide update choisit crop ou initials sans decode paint.
- `avatar_initials_unicode` : nom vide,un/deux mots,accents composés,UTF-8 invalide et override vide sont stables.
- `avatar_hash_contrast` : même nom donne même fond et choix noir/blanc le plus contrasté.
- `avatar_circle_clip` : Cover et ratio parent différent restent cercle sans pixels hors clip.
- `avatar_sources_lifetime` : name/image Binding sources disparues gardent derniers snapshots sûrs.
- `avatar_multi_instance` : updates/caches/styles d’un avatar n’influencent pas les autres.

Créer `examples/features/avatar.cpp` et la cible `nativeui_example_avatar`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
