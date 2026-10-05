# Label

**Statut : existant à extraire.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Afficher une chaîne immuable, sans édition ni action. Les usages principaux sont les libellés de contrôles et les textes de statut courts.

Présent dans [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc) : `Label(std::string)`, alias `TextLabel`, et `LabelComponent` public dans [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). Mesure par `TextService` ; peinture centrée verticalement avec alignement horizontal.

MyGo : `ui/widgets.go`, `Text` et `Textf`, même version de référence. MyGo enveloppe automatiquement les lignes selon la largeur ; le Label actuel mesure une chaîne sans layout de paragraphe. [RichText](rich_text.md) porte le nouveau contrat de paragraphe.

## 2. API publique et composition

API existante à conserver exactement : constructeur par valeur ; fluent rvalue `size(float)`, `color(Color)`, `align(TextAlign)`, `weight(FontWeight)`, `bold(bool = true)`, `slant(FontSlant)`, `italic(bool = true)`, `family(std::string)`, `fallback_families(std::vector<std::string>)`, `style(TextStyle)` et `spec() &&`.

Exemple existant vérifié :

```cpp
auto caption = ui::Label{"Niveau"}
    .size(14.0f)
    .bold()
    .align(ui::TextAlign::Left)
    .spec();
```

Aucun Binding de texte ajouté dans cette extraction. Un texte réactif est reconstruit par la composition applicative existante ; ne pas inventer un observateur implicite sur une chaîne copiée.

## 3. État, propriété et notifications

Le builder possède la chaîne et le `TextStyle`, dont famille et fallbacks. Le composant reçoit leurs valeurs par déplacement ; aucune `string_view` applicative conservée.

Le texte est immuable après montage. Aucun callback de changement ; aucune écriture dans un modèle applicatif.

L’identité appartient au nœud retenu. Une reconstruction de contenu doit respecter le mécanisme de réconciliation existant et ne doit pas changer la clé du parent par défaut.

## 4. Interactions

Pas de focus, capture, survol actif, glissement ou sélection. Pointeur, molette, clavier et texte sont ignorés pour permettre le routage aux parents.

Validation et annulation ne s’appliquent pas. Un libellé rendu dans un bouton ne remplace pas la cible d’activation du bouton.

Les actions d’accessibilité sont absentes. Le mode Disabled n’installe aucune interaction supplémentaire.

## 5. Mesure et layout

Toutes les dimensions sont logiques. `TextService::measure(text, style)` fournit largeur et hauteur ; la peinture utilise exactement la même réparation UTF-8 et les mêmes polices.

L’alignement déplace l’ancre dans la place attribuée, sans modifier la mesure intrinsèque. La contrainte du parent et son clipping restent déterminants en cas de texte trop long.

Conserver le comportement actuel sans retour automatique ni ellipse ajoutée silencieusement. Une hauteur nulle ou une largeur réduite ne doit pas produire de géométrie non finie.

## 6. Présentation et invalidation

`TextStyle` reste le style public. `size` conserve son clamp à zéro ; `bold(false)` et `italic(false)` reviennent aux valeurs Regular/Upright.

La couleur de style explicite prévaut ; ne pas promettre une souscription Theme absente du composant actuel. Les nouvelles propriétés de thème attendent un enrichissement distinct.

Rendu headless : choix d’une police de test déterministe, texte identique à la mesure, et ancre verticale au milieu de la place. Pas de timer ni animation.

## 7. Accessibilité

Contrat cible : `SemanticRole::Text`, texte possédé exposé comme valeur lisible ; pas d’action ni de focus. Les décorations répétées peuvent être exclues par la composition.

Le hook `Component::semantics` et le rôle existent ; ce composant actuel ne garantit pas leur publication. L’extraction doit ajouter ou préserver la publication conformément au contrat sémantique du toolkit.

Les ponts natifs VoiceOver/UIA/AT-SPI sont différés T068 ; une inspection du snapshot headless ne prouve pas leur fonctionnement.

## 8. Cycle de vie et récupération

UI/main-thread seulement ; aucune registration par instance n’est nécessaire pour le texte immuable.

Une allocation ou une mesure de police qui échoue ne publie pas un composant partiel. Les caches éventuels de mesure sont privés et ne deviennent pas un registre mutable global.

Destruction no-throw ; aucun callback applicatif. Ne pas garder PaintContext ou Painter hors de paint ; les ressources de police suivent leur ownership existant.

## 9. Dépendances et cas limites

Dépend de Component, TextService et Painter existants ; aucun chargeur de fichier ni API native de texte supplémentaire.

Chaîne vide : largeur nulle et hauteur de texte conforme à TextService. UTF-8 invalide : mêmes U+FFFD à la mesure et à la peinture, sans modifier l’entrée applicative.

Famille absente : utiliser le fallback existant. Taille zéro, longues chaînes, accents combinés et caractères hors latin sont des scénarios explicites.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/label.hpp` et `src/label.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_builders.inc` et `widgets_basic.inc`. Conserver `TextLabel` et la classe publique `LabelComponent`, sa signature et les méthodes de style actuelles.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `label_utf8_measure_paint` : mesure et pixels concordent pour Unicode valide et invalide.
- `label_alignment` : les trois alignements conservent la taille intrinsèque.
- `label_empty_zero` : texte vide et taille zéro ne créent ni erreur ni géométrie invalide.
- `label_legacy_alias` : TextLabel et tous les fluent existants compilent.

Créer `examples/features/label.cpp` et la cible `nativeui_example_label`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Étendre les preuves existantes de `tests/label_tests.cpp` et `examples/features/t026_label.cpp`, sans supprimer leurs assertions.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
