# Badge

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Court texte ou compteur visuel dans une pillule, utilisable près d’un item de navigation. Présentation readonly ; aucune action ou filtre implicite.

Absent de NativeUI. [Label](label.md), TextService et primitives arrondies existent ; une simple composition actuelle peut en dessiner, sans builder autonome.

MyGo : `ui/sidebar.go`, `Badge`, petite pillule de texte SingleLine avec fond texte alpha0,1. La cible rend taille/couleurs explicites et peut observer un texte déjà formaté par l’application.

## 2. API publique et composition

API cible proposée :

```cpp
class Badge {
public:
    explicit Badge(std::string text);
    explicit Badge(Binding<std::string> text);
    explicit Badge(State<std::string>& text);
    Badge&& style(BadgeStyle value) &&;
    Spec spec() &&;
};
```

BadgeStyle nouveau : TextStyle text, Color background, double horizontal_padding=6, vertical_padding=1, minimum_width=0. Rayon=demi-hauteur, pas une option de click. Aucun `max_count`/capping99+ implicite.

Exemple cible proposé :

```cpp
ui::State<std::string> count{"3"};
auto badge = ui::Badge{count}.spec();
```

## 3. État, propriété et notifications

Constructeur string possède snapshot ; overload Binding observe string effective et State convertit immédiatement. Le contenu est affiché tel que fourni, pas parsé comme nombre.

Source détruite : Binding invalid, dernière string lisible, observe inactive/set jamais utilisé et pas notification automatique. Aucune donnée détruite n’est réinterprétée comme « zéro ».

Update texte = layout/paint/semantics ; pas de callback onchange. Les handles de subscription restent localisés à chaque badge ; aucun badge current global.

## 4. Interactions

Badge n’est ni focusable ni targetable interactif ; pointeur, molette, glissement, clavier, texte et drops Ignored.

Dans Sidebar/Button, l’activation vient de l’item parent. Pas de tooltip automatique, dismiss ou clear au clic.

Validation et annulation non applicables ; Disabled ne supprime pas la valeur de compteur, ReadOnly n’a pas de mutation à empêcher.

## 5. Mesure et layout

Mesure TextService en une ligne + padding, minimum width optionnel ; parent fournit contraintes et clipping. Long texte est clipé, sans ellipse ajoutée ni retour à la ligne dans v1.

Texte vide conserve pillule de padding dans le builder, comme une décoration explicite. Pour supprimer place quand vide, l’application compose Visibility/If ; pas de collapse caché.

Place minuscule borne rayon à demi-hauteur réelle et ne peint pas hors bounds. Unit logique, marge externe propriété du layout parent.

## 6. Présentation et invalidation

BadgeStyle nouveau : défaut texte taille12/famille héritée, foreground Theme.text, background=Theme.text alpha0,1. Pas de slot de Theme badge présumé existant.

Variation couleur/availability = paint ; typographie/padding/texte = layout. Disabled applique palette.disabled au texte, fond inchangé ; aucun style de press/hover.

Pas d’animation, blink ou annonce « urgent » selon couleur. Une couleur transparente garde la mesure.

## 7. Accessibilité

Contrat cible : Text avec valeur string, ou None si la valeur est déjà incorporée dans le nom/description du parent par composition. Aucun Role::Badge absent ajouté furtivement.

La valeur ne doit pas être lue deux fois quand l’item parent donne déjà « Boîte, 3 non lus » ; le choix d’exclusion appartient au conteneur.

Ponts natifs T068 différés. Aucun IME/action ; updates backend-neutres ne prouvent pas annonce native live.

## 8. Cycle de vie et récupération

UI/main-thread ; snapshot string possédé et RAII d’observation pour overload dynamique. Invalidateurs stale no-op après retrait.

Allocation texte/mesure échouée ne publie pas style/texte partiels. Reprise après exception utilise le dernier snapshot valide sans forcer une notification applicative.

Destruction no-throw callback-silent ; aucun timer. Un observer modifiant la même source et levant ne laisse pas de guard du badge bloqué.

## 9. Dépendances et cas limites

Dépend de TextService/Painter/Binding/Theme, pas de Sidebar ou modèle de messages. Une sidebar peut consommer Badge sans importer de service de compteurs.

UTF-8 invalide réparé comme Label ; nombre négatif,99+ ou texte non numérique sont des valeurs valides. Capping/classement arrivent en amont.

Padding/minimum_width non finis/négatifs = invalid_argument avant publication. Texte long/empty/graphemes emoji restent des cas du layout, sans ressource chargée à paint.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/badge.hpp` et `src/badge.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : TextService/Label primitives existantes. BadgeStyle dans le header parent, subscription/layout/pillule dans badge.cpp. Pas de template variadique ni spécialisation numérique par type de compteur.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `badge_static_dynamic` : constructeurs string/Binding/State affichent même valeur.
- `badge_text_layout` : texte vide,99+,long et Unicode donnent padding/clipping prévu.
- `badge_source_lifetime` : source détruite laisse dernière string sûre sans callback.
- `badge_parent_input` : Badge dans Button laisse activation au parent et aucun focus.
- `badge_style_isolation` : deux badges et Theme séparés ne partagent pas données/styles.

Créer `examples/features/badge.cpp` et la cible `nativeui_example_badge`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
