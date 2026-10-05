# Canvas

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Surface de dessin et d’événements personnalisés en coordonnées locales. Le toolkit reste responsable du routage, clipping, focus et backend ; Canvas n’est pas une fenêtre native.

Builder et `CanvasComponent` public dans [widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc) et [widgets_basic.inc](../include/nativeui/detail/widgets_basic.inc). La façade CanvasContext2D et CanvasInputContext est dans [component_base.hpp](../include/nativeui/component_base.hpp).

MyGo : `Element.Draw`/`DrawOver` et Painter dans `ui/element.go`/`ui/paint.go` sont les primitives correspondantes, pas un Canvas autonome de catalogue. Aucun portage de fenêtre/render loop Go.

## 2. API publique et composition

API existante à conserver : `Canvas(Size, DrawCallback)` et `Canvas(float width, float height, DrawCallback)`. `DrawCallback = std::function<void(CanvasContext2D&)>` ; `InputCallback = std::function<EventResult(const InputEvent&, CanvasInputContext&)>`.

Le template rvalue `on_input(Callback&&)` accepte EventResult ou void. Le void est adapté en Handled ; toute autre valeur est rejetée à la compilation. `on_input` rend focusable ; `focusable(bool = true)` peut ensuite désactiver le focus. `spec() &&` conserve ces décisions.

Exemple existant vérifié :

```cpp
auto surface = ui::Canvas{240.0f, 100.0f,
    [](ui::CanvasContext2D& g) {
        g.fill_rect({0.0f, 0.0f, g.width(), g.height()},
                    ui::Color{0.1f, 0.2f, 0.3f, 1.0f});
    }}.focusable(false).spec();
```

## 3. État, propriété et notifications

Le builder possède les callbacks std::function et la taille. Les captures applicatives sont à la charge de leur propriétaire ; une référence doit vivre au moins jusqu’au démontage.

Canvas ne possède aucun State implicite et n’émet aucun on_change. Un input callback mutateur choisit son état et demande explicitement invalidate via CanvasInputContext.

Le modèle applicatif est UI/main-thread. Un Canvas partageant un Binding avec un autre widget doit souscrire dans un owner valide ; Canvas ne devine pas cette dépendance depuis la lambda.

## 4. Interactions

PointerDown/Move/Up/Wheel et DropOffer/Data sont traduits par soustraction de l’origine des bounds. Les touches et évènements non positionnels passent tels quels ; préserver ce détail actuel.

Sans callback input : Ignored, aucun focus automatique. Avec callback : résultat propagé ; capture/release, clipboard et invalidation se font à travers CanvasInputContext existant.

Aucun drag, zoom, scroll ou bouton implicite. Le callback implémente et teste son PointerCancel ; validation/annulation métier appartiennent à l’application. Focusable false retire le Canvas du parcours Tab.

## 5. Mesure et layout

Taille préférée actuelle = largeur/hauteur bornées au minimum 1 lors de la construction du composant. Préserver ce comportement pour valeurs ordinaires et ne pas changer la signature float.

Le parent assigne les bounds réels. `g.width()/height()/size()` décrivent ces bounds locaux réels, pas la taille demandée. Paint traduit vers l’origine locale (0,0) et clippe la surface.

Contrat enrichi fixé : taille NaN ou <=1 conserve le repli historique1 ; +inf est rejetée par invalid_argument avant publication. Aucune transformation invalide ne contamine un frère. Mesure ne dépend pas du draw callback.

## 6. Présentation et invalidation

La lambda compose Painter via CanvasContext2D : primitives, texte, images et SVG existants. Pas de slot Canvas Theme présumé ; l’application choisit ses couleurs.

Le draw callback ne doit pas modifier la structure retenue pendant paint. Toute invalidation deferred revient au checkpoint normal ; pas de rendu récursif.

Encadrer clip et transformation par scopes RAII pour les équilibrer si draw lève. L’enrichissement corrige la restauration exceptionnelle ; l’extraction doit garder les pixels du chemin normal. Ces corrections sont cibles et ne sont pas décrites comme déjà livrées.

## 7. Accessibilité

Contrat cible par défaut : Custom, nom/description fournis par la composition ; Canvas décoratif peut rester None. Pas de valeur ni d’action supposée.

L’application qui dessine un contrôle doit aussi exposer ses actions sémantiques backend-neutres ; des pixels ne décrivent pas sa fonction.

Pas de promesse de pont T068 livré. Le texte dessiné n’ouvre pas d’IME ; saisie personnalisée reste limitée aux chemins committed existants et à la dépendance preedit DESIGN17.4.

## 8. Cycle de vie et récupération

UI/main-thread. PaintContext/InputContext/CanvasContext2D/CanvasInputContext sont empruntés uniquement pendant l’appel ; ne pas les capturer dans du travail différé.

Les callbacks sont possédés, copiés en snapshot avant leur invocation si une mutation réentrante peut les remplacer. Après exception, restaurer clip/transform et les guards de dispatch ; un callback commencé n’est pas rejoué.

Démontage annule capture/focus via Tree, sans exécuter de draw. Destruction no-throw. Retrait d’un sous-arbre possible au checkpoint ; destruction top-level différée, pas sur la pile active.

## 9. Dépendances et cas limites

Dépend de Component, Painter/CanvasContext2D, Input et routage existants. Aucun objet SkCanvas/Pugl ni thread de rendu nouveau dans la surface publique.

Callback draw vide : surface vide. Callback input vide : events ignored. Size nulle/négative : clamp historique. Input hors bounds peut arriver après capture et doit garder ses coordonnées locales non clampées.

Drop ne lit aucun fichier implicitement ; bytes/MIME livrés selon services existants. Les timers applicatifs doivent être annulables et ne capturer que des identités lifetime-safe.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/canvas.hpp` et `src/canvas.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_builders.inc`, `widgets_basic.inc`, `component_base.hpp`. Conserver CanvasComponent public et la façade de contexte commune dans ses includes historiques ; seul le comportement Canvas est déplacé.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `canvas_local_coordinates` : positions et dimensions après translation parent sont exactes.
- `canvas_input_adapter` : void/Handled/Ignored et focusable explicite gardent leur contrat.
- `canvas_throw_balanced` : un draw qui lève laisse clip et transform intacts pour un frère.
- `canvas_capture_teardown` : démontage pendant drag rend inoffensive la suite des événements.
- `canvas_headless_primitives` : primitives, images et SVG restent renderer-independent.

Créer `examples/features/canvas.cpp` et la cible `nativeui_example_canvas`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Reprendre `tests/canvas_tests.cpp` et les scènes headless existantes comme oracle de compatibilité, puis ajouter les cas de restauration.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
