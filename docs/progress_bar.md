# ProgressBar

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Affichage de progression déterminée ou, en extension, activité indéterminée. Le contrôle ne modifie jamais la valeur et ne représente pas un niveau métier avec seuils ; ce rôle appartient à Meter.

Présent dans [widgets_progress_meter.inc](../include/nativeui/detail/widgets_progress_meter.inc), ProgressBar et detail::BoundedDisplayComponent. Domaine finite min<max ; valeur vue clampée, non finite=min ; directions horizontal/vertical et formatter optionnel. Styles dans [progress_style.hpp](../include/nativeui/progress_style.hpp).

MyGo : `ui/widgets.go`, `Progress`, plage0..1, négatif marque indéterminé et Reverse permet RTL. Cible conserve plage float explicite et utilise un mode indéterminé séparé pour ne pas détourner une valeur négative valide.

## 2. API publique et composition

API existante vérifiée : `explicit ProgressBar(State<float>&, float minimum=0, float maximum=1)`, `orientation(ProgressOrientation) &&`, `formatter(Formatter) &&` avec std::function<std::string(float)>, `style(ProgressBarStyle) &&`, `spec() &&`.

Exemple existant vérifié :

```cpp
ui::State<float> progress{0.4f};
auto bar = ui::ProgressBar{progress, 0.0f, 1.0f}
    .orientation(ui::ProgressOrientation::Horizontal)
    .formatter([](float value) { return std::to_string(value); })
    .spec();
```

Ajouts cibles : constructeur `ProgressBar(Binding<float>, float minimum=0, float maximum=1)` et fluent rvalue `indeterminate(bool=true)`, `reversed(bool=true)`, `reduced_motion(bool=true)`. Signatures float et State historiques conservées ; pas de callback on_change ni sentinel négatif.

## 3. État, propriété et notifications

Source actuelle emprunte directement State<float>& ; elle doit rester vivante jusqu’au démontage. Contrat enrichi : convertir la surcharge State en Binding immédiatement, de même API source mais ownership runtime plus sûr.

Le Binding de la nouvelle API tient le dernier control block. Source détruite : valid=false, dernière valeur lisible, set jamais appelé, observe inactif sans notification destruction. Revalider pour désarmer activité au prochain accès ; aucun événement utilisateur inventé.

La valeur effective de vue est clampée sans writeback, fraction calculée en double pour plages float larges. Mode indéterminé ignore valeur pour géométrie mais ne supprime pas la valeur du modèle. Observations invalident uniquement paint/sémantique.

## 4. Interactions

Display-only : pas de focus, capture, survol actif, molette, glissement, clavier ou saisie. Les inputs retournent Ignored et ne se servent pas du formatter comme action.

ReadOnly n’ajoute aucune fonction ; Disabled conserve le style disabled et une valeur descriptive. Le mode indéterminé n’est pas une validation ou opération à annuler par Escape.

Écriture utilisateur impossible, y compris via SemanticAction::SetValue. L’annulation d’un traitement reste un Button séparé de l’application.

## 5. Mesure et layout

Chemin historique utilise tailles préférées horizontal/vertical et formatted selon le style ; formatter n’est pas appelé pour mesure. Préserver la place lorsqu’un texte change.

Horizontal remplit depuis gauche ; vertical depuis bas. reversed inverse l’origine de remplissage ; fraction0/1 donne vide/plein. Tous les rectangles sont bornés à la piste et logiques.

Indéterminé : segment30% de longueur traversant piste en1,4s, clip aux bords ; mode statique reduced_motion/sans timing = segment centré30%, sans saut de mesure. Dimensions zéro ne demandent pas d’animation ni pixels.

## 6. Présentation et invalidation

ProgressBarStyle/ProgressStylePatch existants conservés dans progress_style.hpp, avec leur précédence thème puis style explicite puis disponibilité. Ne pas promettre Theme slots inconnus.

Déterminé : formatter reçoit valeur effective float. Indéterminé : pas d’appel formatter numérique, la piste suffit ; texte d’activité se compose par Label voisin. Reversed ne modifie pas formatter.

Animation utilise AnimationContext existant, tween linéaire0→1 et reprise de cycle protégée par génération. reduced_motion bool explicite, défaut false ; pas de détection OS affirmée livrée. Arrêt Hidden/Collapsed/démonté et source invalide.

## 7. Accessibilité

Contrat cible : ProgressBar, range/numeric_value effective pour déterminé ; indéterminé omet numeric_value et value_range et donne description « progression indéterminée ». Pas de valeur -1 inventée dans une plage.

Aucune action mutatrice ni focus. Semantics update suit données/mode et non chaque frame d’animation ; un lecteur ne doit pas être inondé de ValueChanged de phase.

Les hooks/rôle existent mais l’override actuel n’est pas preuve de publication. Ponts natifs T068 différés ; aucun IME.

## 8. Cycle de vie et récupération

UI/main-thread ; subscriptions/animation handles par instance RAII, arrêt au démontage/désactivation visuelle. Un invalidateur stale est no-op lifetime-safe.

Formatter possédé peut lever/réentrer : pas de mutation structure synchronique dans paint, restoration des scopes/guards avant propagation C++. Invocation commencée jamais rejouée automatiquement.

Sans DispatcherProvider valide, phase statique centrée ; aucun thread/sleep ni fallback synchronique d’un callback différé. Échec d’armement retire handle non publié puis garde un rendu statique et permet une reprise au prochain vrai accès. Destruction no-throw.

## 9. Dépendances et cas limites

Réutilise Binding/Theme/Painter/AnimationContext et DispatcherProvider existants. Pas d’adapter audio ou modification de paramètres.

Domaines non finis/inversés/égaux : invalid_argument quand le composant est instancié comme actuellement. Externe NaN/inf = min visible ; modèle intact. Valeur hors plage = clamp view uniquement.

Retour indéterminé→déterminé montre immédiatement la dernière valeur. Mise à jour externe masquée doit se voir au remontage. Format vide autorisé ; un formatter mutateur de UI doit différer ses changements selon CODE_REVIEW.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/progress_bar.hpp` et `src/progress_bar.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_progress_meter.inc` et progress_style.hpp. Préserver ProgressOrientation, Formatter et points d’entrée styles. Extraire noyaux réels ProgressBar et Meter dans chacun de leurs .cpp ; helpers domain/style peuvent rester privés partagés sans .cpp factice.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `progress_bar_legacy_range` : domaines valides/larges et invalid_argument gardent leurs résultats.
- `progress_bar_formatter_effective` : formatter reçoit clamp/mimimum des valeurs non finies sans writeback.
- `progress_bar_orientation_reverse` : les quatre sens remplissent leur piste exactement.
- `progress_bar_indeterminate_clock` : cycle1,4s, clip et passage au déterminé sous horloge manuelle.
- `progress_bar_reduced_hidden` : motion réduite, absent timing et Hidden arrêtent wakes avec phase statique.
- `progress_bar_formatter_throw` : exception de formatter n’empêche pas la prochaine frame ni le frère.

Créer `examples/features/progress_bar.cpp` et la cible `nativeui_example_progress_bar`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Reprendre `examples/features/t033_progress_meter.cpp` et les tests Progress/Meter existants ; qualifier la surcharge Binding et l’activité indéterminée séparément.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
