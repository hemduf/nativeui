# Divider

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Séparateur visuel horizontal ou vertical, distinct des poignées de SplitView. Il contribue une épaisseur au layout et s’étire sur son axe principal.

Pas de builder Divider dans NativeUI ; [Header](header.md) dessine aujourd’hui une ligne interne. Les primitives ligne/rectangle existent dans [component_base.hpp](../include/nativeui/component_base.hpp).

MyGo : `ui/widgets.go`, `Divider`, déduit l’orientation du parent row et impose 1 DIP de largeur ou hauteur. Le portage choisit une orientation explicite et déterministe, sans lecture cachée du parent.

## 2. API publique et composition

API cible proposée :

```cpp
enum class DividerOrientation { Horizontal, Vertical };
class Divider {
public:
    explicit Divider(DividerOrientation value = DividerOrientation::Horizontal);
    Divider&& thickness(double value) &&;
    Divider&& color(Color value) &&;
    Spec spec() &&;
};
```

Épaisseur par défaut 1 unité logique ; couleur de défaut = bordure du thème. Valeur finie >= 0 obligatoire pour thickness ; invalide = invalid_argument avant publication.

Exemple cible proposé : `ui::Divider{ui::DividerOrientation::Vertical}.thickness(1.0).spec()`. L’orientation verticale doit être choisie par l’application dans une Row.

## 3. État, propriété et notifications

Options possédées ; aucun état mutable applicatif, Binding ni callback. L’identité suit le nœud retenu.

La couleur explicite est distincte de la couleur de défaut thémée. Seul l’abonnement au Theme de l’instance peut mettre à jour une couleur implicite.

Le séparateur ne communique ni valeur ni position de drag. Aucun événement de changement.

## 4. Interactions

Aucun focus, capture, survol, molette, glissement, clavier ou texte ; tous les inputs sont Ignored.

Validation/annulation non applicables. Une orientation Vertical ne donne pas un comportement de resize.

Une superposition avec un Button laisse l’activation au Button ; le Divider ne doit pas masquer une cible d’entrée interactive.

## 5. Mesure et layout

Horizontal : préféré {0, épaisseur}, minimum de l’épaisseur borné par contrainte. Vertical : préféré {épaisseur, 0}. Le parent fournit la longueur finale.

Axe principal étiré dans la place reçue ; épaisseur max bornée à la dimension transverse disponible. Zéro d’épaisseur ne peint rien et ne doit pas exiger une taille artificielle.

Dessiner dans les coordonnées logiques sans arrondir la place retenue au DPR. Ne jamais peindre au-delà des bounds lors de fenêtre minuscule.

## 6. Présentation et invalidation

Rectangle/ligne de couleur bordure ; pas d’animation, focus ring, hover ou accent implicite. Un alpha nul garde la géométrie sans pixels.

Theme implicite change = paint ; épaisseur ou orientation modifiée par reconstruction = layout et paint.

Pas de DividerStyle obligatoire pour trois propriétés ; si étendu, le type appartient au couple Divider et ne fait pas apparaître un slot Theme existant fictif.

## 7. Accessibilité

Séparateur décoratif : SemanticRole::None. Ne pas inventer Role::Separator, absent du contrat actuel.

Si l’application doit signaler une séparation fonctionnelle, ses Group sont nommés autour du Divider ; la ligne ne se substitue pas à cette structure.

Ponts natifs T068 différés. Aucun texte/IME ni action.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement au Theme privé RAII si couleur implicite. Il cesse au démontage.

Construction échouée ne laisse aucun abonnement. Un invalidateur stale est lifetime-safe ; aucune lambda ne capture un nœud brut.

Destruction no-throw, aucune notification applicative. Deux instances peuvent choisir des épaisseurs/couleurs indépendantes.

## 9. Dépendances et cas limites

Réutilise Component et Painter. Pas de lien à Pugl, Skia public ou au parent Row.

Dimensions nulles, contraintes non bornées et alpha transparent sont autorisés. NaN/inf ou épaisseur négative sont rejetés pour la nouvelle API.

La présence de plusieurs Divider successifs est valide ; aucun collapse de lignes ni gap implicite.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/divider.hpp` et `src/divider.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : primitives Painter existantes ; comportement nouveau. Le Header conserve son séparateur historique ou réutilise ce noyau explicitement, sans dépendance circulaire entre composants.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `divider_orientation` : tailles et pixels horizontaux/verticaux sont cohérents.
- `divider_zero_narrow` : épaisseur zéro et place trop étroite ne débordent pas.
- `divider_pass_through` : aucun input ni focus capturé.
- `divider_theme_isolation` : couleur implicite change seulement dans son UI.

Créer `examples/features/divider.cpp` et la cible `nativeui_example_divider`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
