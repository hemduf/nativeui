# Header

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Afficher un en-tête de panneau avec titre, sous-titre et séparateur. Il reste un composant de présentation, sans barre de navigation implicite.

Présent dans [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), `Header(std::string)` ; `HeaderComponent` public dans [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). Il mesure actuellement 640 × 70 et dessine toujours le sous-titre `SATURATION / CHARACTER`.

MyGo : pas de composant Header autonome ; `Text`, `Column` et `Divider` dans `ui/widgets.go` permettent cette composition. La suppression de la dépendance à un libellé audio est un enrichissement NativeUI.

## 2. API publique et composition

API actuelle conservée : `Header(std::string title)` et `spec() &&`.

API cible proposée :

```cpp
class Header {
public:
    explicit Header(std::string title);
    Header&& subtitle(std::string value) &&;
    Header&& style(HeaderStyle value) &&;
    Spec spec() &&;
};
```

`HeaderStyle` contient `TextStyle title`, `TextStyle subtitle`, padding, gap, border_color et border_width logiques. Les champs numériques nouveaux sont double ; aucun slot Theme supposé existant.

Exemple cible proposé : `ui::Header{"Bibliothèque"}.subtitle("Préréglages utilisateur").spec()`. Le constructeur historique sans setter conserve le sous-titre actuel ; `subtitle("")` le retire explicitement.

## 3. État, propriété et notifications

Titre, sous-titre et style sont possédés par valeur. Pas de Binding dans cette version ; mise à jour par reconstruction de la composition.

L’absence de setter subtitle est distincte d’une chaîne explicitement vide pour maintenir le comportement historique. Ne pas réinjecter la chaîne historique après que l’utilisateur a choisi une chaîne vide.

Pas de notification applicative, aucune donnée audio/host. Identité et durée de vie suivent le nœud retenu.

## 4. Interactions

Événements pointeur, glissement, molette et clavier ignorés ; pas de focus ni de saisie.

Une action telle que fermer un panneau doit être un Button voisin, pas une zone active cachée du Header.

Aucune validation/annulation. Le séparateur n’est pas une poignée de redimensionnement.

## 5. Mesure et layout

Préserver 640 × 70 comme taille préférée du chemin historique. Le chemin avec style/sous-titre personnalisé mesure les textes et espacements, bornés par les contraintes disponibles.

Titre et sous-titre sont sur deux lignes distinctes ; un sous-titre vide supprime sa ligne et son gap dans le chemin personnalisé. Les largeurs viennent de TextService.

Si la largeur est petite, clipper dans les bornes sans faire déborder le séparateur. Ne pas réduire silencieusement la taille typographique ; espace insuffisant = clipping déterministe.

## 6. Présentation et invalidation

Le style personnalisable couvre title/subtitle/border sans nom de produit imposé. Palette de défaut prise dans le Theme au montage dans le nouveau chemin ; overrides explicites gagnants.

La taille/famille/padding déclenchent invalidation de layout et paint lors d’une reconstruction ; la couleur seule paint.

Pas d’animation. Respecter les coordonnées logiques lors du dessin de la ligne ; toute adaptation aux pixels appartient au renderer.

## 7. Accessibilité

Contrat cible : Group avec titre Text et sous-titre Text ; pas d’un rôle Heading annoncé comme existant.

Le séparateur décoratif n’a pas de nœud sémantique autonome. Le titre doit rester lisible même si la zone visuelle est rognée.

Hooks backend-neutres disponibles ; publication cible à vérifier en headless. Ponts natifs T068 différés, sans promesse de lecture VoiceOver/UIA/AT-SPI.

## 8. Cycle de vie et récupération

Pas de capture, timer ou abonnement. Le composant possède tout le contenu utilisé durant paint et measure.

Sur une exception de préparation typographique, garder les invariants retenus ; aucune tentative de rendu avec un style partiellement publié.

Destruction no-throw et callback-silent. Aucun état partagé mutable entre en-têtes ou UI ; aucun contexte de mesure/paint retenu.

## 9. Dépendances et cas limites

Réutilise Label/TextService et le dessin du séparateur ; [Divider](divider.md) partage la convention visuelle mais pas une seconde boucle de layout.

Titre vide autorisé ; sous-titre seul autorisé. Couleurs transparentes autorisées. Dimensions de style non finies/négatives rejetées avant Spec avec invalid_argument pour les nouvelles options.

Pas d’équivalent métier MyGo à importer ; aucun accès audio ou format de plugin. Unicode suit la réparation commune de TextService.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/header.hpp` et `src/header.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_builders.inc` et `widgets_basic.inc`. Préserver `HeaderComponent(std::string)` et l’include collectif ; ajouter un overload/options interne pour le chemin enrichi, sans modifier le défaut historique.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `header_legacy_presentation` : construction simple garde taille et sous-titre historiques.
- `header_custom_subtitle` : chaîne personnalisée et chaîne vide suivent des layouts distincts.
- `header_narrow_clip` : titre long et séparateur restent dans leurs bornes.
- `header_semantic_text` : titre et sous-titre sont présents une fois dans le snapshot.

Créer `examples/features/header.cpp` et la cible `nativeui_example_header`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
