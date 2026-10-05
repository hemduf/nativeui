# RichText

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Composer un paragraphe de fragments possédés avec polices, couleurs, décorations, surlignages et actions inline. Le texte reste non éditable ; édition riche et markup HTML ne font pas partie de ce composant.

Absent du catalogue public NativeUI. [text.hpp](../include/nativeui/text.hpp) expose TextStyle et TextService mais aucun layout public multi-run ; [Label](label.md) est un affichage simple.

MyGo : `ui/richtext.go`, `Span`, `RichText`, `encodeSpans`, `spanPaint.runs`, `Painter.RichText`. Ses fragments se replient comme un paragraphe ; les enfants Link peuvent conserver leur interaction sur leurs mots. Il faut reprendre ce comportement sans importer le moteur Go.

## 2. API publique et composition

API cible proposée :

```cpp
struct RichTextSpan {
    std::string id;
    std::string text;
    std::optional<TextStyle> style;
    std::optional<Color> background;
    bool underline{};
    bool strikethrough{};
    std::function<void()> on_activate;
};
class RichText {
public:
    explicit RichText(std::vector<RichTextSpan> spans);
    RichText&& style(TextStyle value) &&;
    RichText&& wrap(bool value = true) &&;
    Spec spec() &&;
};
```

Un style de span fourni remplace tout le TextStyle de base pour ce run ; une absence hérite intégralement. Couleur de décoration = couleur du texte. L’id est obligatoire/unique seulement pour un span interactif.

Exemple cible proposé :

```cpp
auto paragraph = ui::RichText{std::vector<ui::RichTextSpan>{
    {.text = "Lire "},
    {.id = "guide", .text = "le guide", .underline = true,
     .on_activate = [] {}},
    {.text = " avant de commencer."}
}}.wrap().spec();
```

## 3. État, propriété et notifications

Le composant possède fragments, styles et callbacks. Aucun pointer ou string_view vers le modèle utilisateur n’est conservé. Pas de Binding de texte ni de sélection dans cette première API.

Cache de paragraphe par instance indexé par texte/style/largeur ; les offsets désignent le buffer UTF-8 réparé possédé. Les clés interactives restent stables lors d’un reflow.

Une activation n’émet aucune écriture texte. Le callback est pris en snapshot avant invocation ; l’état pressed est terminal avant code utilisateur.

## 4. Interactions

Span interactif : clic primaire press/release dans la même zone de run ; capture puis annulation sur PointerCancel, désactivation ou retrait. Un run sur plusieurs lignes accepte tous ses rectangles.

Tab/Shift+Tab parcourent uniquement les actions inline. Enter/Space activent l’action focalisée une fois ; les répétitions clavier ne dupliquent pas une pression.

Le texte ordinaire ignore événements et molette. Pas de sélection, clipboard, saisie, glissement de contenu ni validation. Escape annule la pression courante sans déclencher une action.

## 5. Mesure et layout

Mesurer un paragraphe complet, sans addition naïve de mesures indépendantes qui briserait ligatures, bidi ou clusters. Les retours à la ligne explicites séparent les paragraphes ; wrap actif par défaut utilise la largeur contrainte.

Largeur non bornée : une ligne par paragraphe. Largeur nulle : aucune géométrie non finie ni boucle de reflow. Mot trop long : coupure aux frontières de grapheme autorisées, jamais dans une séquence UTF-8.

Le rendu et le hit testing consomment le même résultat de layout et ses rectangles de runs. Clipper aux bornes retenues ; toutes les mesures et offsets géométriques sont logiques.

## 6. Présentation et invalidation

Base TextStyle et overrides complets ; backgrounds derrière glyphes, soulignements/suppressions sur métriques de police et non coordonnées magiques par span.

Contrat visuel fixé : une action inline est soulignée en hover/pressed, et ses rectangles de runs reçoivent un focus ring dans la couleur de texte du run au focus clavier. Le fond explicite est conservé. Aucun type RichTextStyle ni slot Theme supplémentaire dans cette v1.

Style ou largeur modifié = reflow et repaint ; action/id seul = structure sémantique et interactions. La peinture reste pure et ne lance pas de callback d’action.

## 7. Accessibilité

Contrat cible : Text pour le paragraphe ordinaire et Button/Custom pour une action inline selon le mapping approuvé. SemanticRole::Link n’existe pas dans la version étudiée ; ne pas le publier fictivement.

Nom d’action = texte du span ; bornes logiques = union de ses rectangles, identité stable. La lecture textuelle ne duplique pas chaque run déjà décrit par la valeur globale.

Le pont natif T068 est différé. Texte non éditable : aucun IME actif ; si sélection/édition sont ajoutées plus tard, elles doivent respecter la séparation committed/preedit de DESIGN17.4.

## 8. Cycle de vie et récupération

UI/main-thread ; subscriptions absentes. Ne pas garder Painter, contextes ni nœuds retenus dans le cache de paragraphe.

Un reflow est préparé puis publié atomiquement. S’il échoue, conserver le résultat précédent valide ; si aucun résultat n’existe, propagation C++ après nettoyage.

Une action qui retire son sous-arbre ou lève n’est jamais rejouée. Libérer capture et marque pressed avant invocation ; destruction no-throw. Destruction top-level UI différée au checkpoint sûr.

## 9. Dépendances et cas limites

Dépendance structurante : ajouter un adaptateur privé de layout multi-run du texte existant dans ce couple, avec backend privé et contrat mesure/peinture commun ; aucune nouvelle API Skia publique.

Spans vides ignorés dans le layout mais ne créent pas d’action invisible. Ids interactifs vides/dupliqués = invalid_argument avant publication. Aucun parsing implicite de markup.

UTF-8 invalide réparé identiquement dans tous les runs ; clusters combinés traversant une frontière de run restent géométriquement cohérents. Retrait d’un span actif annule son action ; un nouvel id ne réutilise pas son focus.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/rich_text.hpp` et `src/rich_text.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : TextService et Painter existants. `RichTextSpan` et le cache ne doivent pas être ajoutés à widgets_basic.inc ; le composant utilise les services existants et un noyau non template.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `rich_text_mixed_runs` : styles, fond et décorations suivent chaque fragment.
- `rich_text_unicode_reflow` : bidi, emoji et clusters restent entiers sur largeur réduite.
- `rich_text_inline_hit` : action répartie sur deux lignes hit-teste ses seuls rectangles.
- `rich_text_remove_pressed` : retrait de l’id pendant pression supprime l’activation.
- `rich_text_callback_throw` : après une action réentrante qui lève la suivante fonctionne sans rejouer la première.

Créer `examples/features/rich_text.cpp` et la cible `nativeui_example_rich_text`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
