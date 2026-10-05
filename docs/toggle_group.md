# ToggleGroup

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Regrouper des actions indépendantes en segments joints : gras/italique/souligné. Le groupe est un
arrêt de Tab et une unité visuelle, sans état de sélection exclusif.

NativeUI fournit Row, RadioGroup et les services de focus, mais pas ce conteneur. Lire les contrats
de [widgets.hpp](../include/nativeui/widgets.hpp) et [focus.hpp](../include/nativeui/focus.hpp).

MyGo : `ui/toggle.go`, `ToggleGroup`, `segmentTrack`. Le contexte temporaire de style MyGo est
remplacé par une portée retenue par instance.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class ToggleGroup {
public:
  ToggleGroup(std::string label, std::vector<Spec> controls);
  ToggleGroup&& style(ToggleGroupStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<bool> bold{false};
ui::State<bool> italic{false};
std::vector<ui::Spec> items;
items.push_back(ui::ToggleButton("B", bold).spec());
items.push_back(ui::ToggleButton("I", italic).spec());
auto group = ui::ToggleGroup("Style", std::move(items)).spec();
```

ToggleGroupStyle cible : padding de track, gap, radius, fill/border et patches segmentés pour
Button/ToggleButton. La propagation du style utilise une portée locale ; elle n’exige aucun nouveau
slot global Theme.

controls sont des Specs possédées : Button et ToggleButton sont les contrôles principaux ; contenus
non interactifs admis comme décor, contrôles imbriqués conservent leur propre gestion de focus.

## 3. État, propriété et notifications

Le groupe n’a pas de Binding de sélection ; chaque ToggleButton détient son bool. Le groupe garde
uniquement une identité du dernier enfant focusable actif pour roving Tab.

Les Specs et le label sont possédés. Le vector controls est immuable dans la génération montée ;
remplacer la Spec crée une nouvelle génération et annule anciennes actions/focus. Les identités
retenues servent aux enfants de cette génération ; ne pas identifier le contrôle actif par son label
“B”.

Retirer le contrôle actif déplace la cible roving vers le prochain disponible, puis précédent ;
groupe vide ne demande pas le focus et ne publie aucune valeur.

## 4. Interactions

Tab entre sur le dernier contrôle disponible actif, ou le premier au premier accès ; un seul arrêt.
Shift+Tab sort selon le runtime.

Gauche/Droite et Haut/Bas passent au contrôle focusable suivant/précédent, avec bouclage ; Home/End
choisissent premier/dernier. Disabled et invisibles sont sautés.

Flèches ne basculent pas les bools ; Espace/Entrée et clic sont délégués au contrôle. Molette et
capture d’un drag extérieur ne sont pas consommées par le groupe.

Un TextInput incorporé conserve ses touches d’édition : ne détourner que les commandes de navigation
de groupe non prises par l’enfant. ReadOnly est propagé aux contrôles de valeur.

## 5. Mesure et layout

Row horizontal, segments à hauteur étirée commune ; largeur intrinsèque somme des enfants, gaps et
padding du track. Sous contrainte trop étroite, clipper ou laisser le parent scroll, sans cacher
silencieusement un contrôle.

Le groupe ne crée pas d’overflow menu : cette politique appartient à Toolbar. Les dimensions restent
logiques ; largeur des segments peut différer selon leurs labels.

Style de track modifiant padding/gap/typographie demande layout ; changement de sélection d’un
enfant ne remesure le groupe que si ses métriques changent.

## 6. Présentation et invalidation

Track discret, bordure extérieure commune ; segments visuellement joints et selected en relief
local. La portée de style ne traverse pas les limites d’un autre ToggleGroup.

Focus visible sur le contrôle actif, pas un second ring ambigu autour du groupe entier. L’état
disabled d’un enfant ne désactive pas automatiquement les voisins.

Invalidation enfant remonte via les services retenus ; ne pas reconstruire tous les segments pour un
bool modifié.

## 7. Accessibilité

Cible : Group nommé par label ; chaque Button/ToggleButton publie son action et sa valeur. Aucun
rôle RadioGroup ni selected unique, puisque plusieurs pressés sont permis.

L’ordre des enfants du snapshot correspond à l’ordre visuel. Les éléments décoratifs ne deviennent
pas des arrêts ou actions.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Inscrire et retirer les participants au focus de manière RAII ; disparition d’un segment armé annule
son capture avant mise à jour roving.

Une action d’enfant peut supprimer le groupe ; le groupe ne demande plus le focus après callback
sans revérifier l’identité du propriétaire. Échec de montage d’un enfant ne publie pas une liste
partielle.

Tout état et routage restent confinés au thread UI/main. Les abonnements et captures sont libérés
par instance ; aucun registre mutable global ne transporte les interactions.

Les callbacks sont possédés et copiés avant appel. Restaurer captures, drapeaux et identité avant de
publier une valeur ou appeler l’application. Un callback commencé qui lève ne sera jamais rejoué ;
les exceptions C++ directes peuvent repartir après restauration des invariants.

La suppression d’un sous-arbre suit la réconciliation sûre. La destruction du propriétaire UI/window
depuis un callback doit passer par un point sûr différé ; aucune sécurité de destruction synchrone
du propriétaire n’est promise.

Destruction et démontage sont no-throw. Les invalidateurs différés portent un jeton faible de
propriétaire et une identité monotone ; après retrait ils deviennent inopérants, sans retenir un
Node ou contexte emprunté.

## 9. Dépendances et cas limites

Dépendances : [row](row.md), [focus_scope](focus_scope.md), [style_scope](style_scope.md),
[button](button.md), [toggle_button](toggle_button.md). Réutiliser le focus commun, aucun service
parallèle.

Tous les enfants disabled : groupe non navigable ; retour enabled rétablit première cible. Deux
groupes avec labels identiques gardent focus et styles isolés.

Ne pas ajouter d’exclusivité ni de callback aggregate Changed : les bools indépendants et leurs
observers sont suffisants.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/toggle_group.hpp` et `src/toggle_group.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Les adaptateurs éventuels de construction variadique restent dans le header ; le .cpp porte
arrangement horizontal, portée visuelle et contrat de participation au focus.

Inscrire `src/toggle_group.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`toggle_group_roving` : un seul Tab stop et bouclage flèches/Home/End ; disabled sautés.

`toggle_group_values` : navigation ne modifie aucun bool ; activation d’un segment ne touche pas les
autres.

`toggle_group_remove_focus` : retrait du segment courant choisit cible valide et annule un geste
armé.

`toggle_group_style_scope` : présentation segmentée reste localisée ; bouton extérieur conserve son
style.

`toggle_group_empty_resize` : groupe vide/all disabled et réduction de largeur gardent
focus/clipping cohérents.

`toggle_group_child_throw` : échec mount/callback remet état focus/portée en état récupérable.

Ajouter `examples/features/toggle_group.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
