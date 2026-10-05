# Meter

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Afficher une valeur bornée et, en extension, un état normal/avertissement/critique selon deux seuils. Display-only ; aucune saisie ni progression indéterminée.

Présent dans [widgets_progress_meter.inc](../include/nativeui/detail/widgets_progress_meter.inc) via Meter et BoundedDisplayComponent ; mêmes plages/orientations/formatter que ProgressBar, rayon de fill plus serré. Style dans [progress_style.hpp](../include/nativeui/progress_style.hpp).

MyGo : `ui/indicators.go`, `Meter` et `MeterLevels`. Warning/Critical définissent haut mauvais ou bas mauvais selon leur ordre ; égalité désactive seuils. Cible ajoute ce comportement sans importer les couleurs Theme.Success/Warning/Danger inexistantes dans ThemePalette NativeUI.

## 2. API publique et composition

API actuelle exacte : `Meter(State<float>&, float minimum=0, float maximum=1)`, rvalue `orientation(ProgressOrientation)`, `formatter(Formatter)`, `style(MeterStyle)` et `spec() &&`. Formatter prend float et retourne std::string.

Exemple existant vérifié :

```cpp
ui::State<float> level{0.7f};
auto display = ui::Meter{level, 0.0f, 1.0f}
    .formatter([](float value) { return std::to_string(value); })
    .spec();
```

Ajouts cibles : `Meter(Binding<float>, float minimum=0, float maximum=1)` ; `struct MeterLevels { double warning; double critical; }` ; `levels(MeterLevels) &&`, `threshold_colors(Color warning, Color critical) &&`. Seuils absents par défaut ; defaults amber {1,0.65,0,1}, critical {0.85,0.15,0.15,1}. API existante reste float.

## 3. État, propriété et notifications

Source actuelle est State<float>& empruntée et doit vivre jusqu’au démontage. Cible convertit constructeur State en Binding immédiatement, gardant signatures et supprimant le borrow direct interne.

Observer ne réécrit jamais le modèle : view clampée, non finie=min. Binding source supprimée devient invalid, get dernière valeur/set ignoré/observe inactive sans notification destruction ; aucun callback de changement Meter.

Les seuils options immuables sont appliqués à la valeur effective affichée, pas à un raw out-of-range. Normal/warning/critical sont dérivés et ne forment pas une seconde State publique.

## 4. Interactions

Aucun focus, capture, pointeur actif, glissement, molette, clavier ou saisie ; input Ignored comme aujourd’hui.

ReadOnly conserve style read-only et description. Disabled prend sa couleur disabled même quand seuil critique ; aucun clic pour acquitter une alerte implicite.

Validation/annulation métier non applicables. Une action telle que charger/remettre un compteur à zéro appartient à un Button voisin.

## 5. Mesure et layout

Tailles préférées de style conservent horizontal/vertical et variante formatted. Formatter n’est pas appelé en mesure ; pas de relayout au rythme de ses textes.

Horizontal depuis gauche, vertical depuis bas ; rayon fill historique conservé. Calcul fraction en double empêche overflow de la différence de deux floats finis.

View bornée et zero dimension : aucun fill hors piste. Seuils ne modifient ni la plage ni la longueur, seulement présentation/description ; clipping suit les bounds.

## 6. Présentation et invalidation

Sans levels, couleurs/precedence MeterStyle actuelles inchangées. Avec levels, valeur>=critical critique puis >=warning warning si critical>warning ; sinon <=critical critique puis <=warning warning.

Égalité warning==critical désactive seuils ; normal = resolved MeterStyle.fill. Threshold colors remplacent seulement fill pour états warning/critical, après normal style ; style disabled prévaut finalement. Bornes/texte/rayon continuent leur résolution existante.

Pas d’animation ni smoothing implicitement audio ; update = paint/semantics. Formatter reçoit valeur effective float. Dénomination du niveau est incluse dans description sans forcer le texte visible.

## 7. Accessibilité

Contrat cible : Meter avec nom fourni par composition, numeric_value/range effective ; description état normal/avertissement/critique si thresholds activés. Aucun Action::SetValue.

Comparaisons inclusives identiques aux pixels. Non finite externe présenté min, et description peut signaler source invalide sans annoncer un état métier fiable.

Rôle Meter et hooks existent ; publication concrète actuelle non présumée. Ponts T068 différés ; aucune prise en charge native déjà annoncée.

## 8. Cycle de vie et récupération

UI/main-thread ; subscription RAII et invalidateur par instance, désarmés au démontage. Aucun timer ni history/peak global.

Formatter réentrant/throw doit garder paint scopes/dispatch guards équilibrés et son invocation commencée n’est jamais rejouée. Les modifications structurelles sont différées au checkpoint retenu.

Préparation de plage/seuils/styles avant publication. Destruction no-throw sans callback applicatif ; source Binding disparue ne crée pas un UAF ni annulation fictive.

## 9. Dépendances et cas limites

Réutilise domain/Theme/Painter et Binding ; les helpers bornés peuvent être privés communs avec ProgressBar, chaque composant gardant un vrai cpp.

Range min/max non finite/inversée/égale = invalid_argument à instanciation historique. Levels non finis ou hors [min,max] = invalid_argument pour ajout cible ; égalité dans plage valide et désactive.

Min==Max n’est pas un compteur zéro : domaine invalide comme avant. Externe NaN/inf/hors plage ne sont jamais corrigés. Format vide autorisé ; aucune couleur sRGB convertie en integer native dans l’API.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/meter.hpp` et `src/meter.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `widgets_progress_meter.inc` et progress_style.hpp. Conserver MeterStyle/ProgressStylePatch et ProgressOrientation dans leurs points d’entrée historiques. Extraire vrai noyau du Meter dans meter.cpp, pas duplicate runtime in .inc.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `meter_legacy_display` : absence de thresholds conserve pixels, orientation, formatter.
- `meter_high_bad` : thresholds80/95 inclusifs déterminent warning/critical.
- `meter_low_bad` : thresholds20/5 inclusifs inversent la logique sans changer fraction.
- `meter_equal_invalid` : égalité désactive ; nonfinite/hors domaine rejeté avant publication.
- `meter_disabled_priority` : style Disabled prévaut sur fill critique et ReadOnly reste visible.
- `meter_formatter_throw` : exception puis valeur suivante restent récupérables.

Créer `examples/features/meter.cpp` et la cible `nativeui_example_meter`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Reprendre `examples/features/t033_progress_meter.cpp` et preuves de domaines larges existantes. Ajouter seuils, snapshots et Binding sans supprimer les contrats float.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
