# ToggleButton

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Un bouton qui garde une valeur pressée : gras, italique, option de barre d’outils. Il est distinct
de Toggle, interrupteur à curseur déjà présent.

NativeUI fournit [Button](button.md), [Toggle](toggle.md), Binding et patches de style ; aucun
ToggleButton public.

MyGo : `ui/toggle.go`, `ToggleBase`, `Toggle`, `pressedColor`. Le portage ajoute un nom sans
collision avec Switch<T>, qui demeure la composition conditionnelle.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class ToggleButton {
public:
  ToggleButton(std::string label, Binding<bool> pressed);
  ToggleButton(std::string label, State<bool>& pressed);
  ToggleButton&& style(ToggleButtonStyle value) &&;
  ToggleButton&& content(Spec value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<bool> bold{false};
auto control = ui::ToggleButton("Gras", bold).spec();
```

ToggleButtonStyle cible : base, hovered, pressed, selected, focused, disabled, read_only, avec
métriques de ButtonStyle. selected décrit la valeur persistante ; pressed décrit seulement l’appui
en cours.

content remplace le label visuel et conserve son nom sémantique. Les notifications utilisent
Binding<bool> ; aucun second callback on_change n’est nécessaire.

## 3. État, propriété et notifications

Binding<bool> est l’unique vérité persistante. Valeur sélectionnée et appui momentanés ne sont
jamais confondus ; activation calcule !pressed.get() au moment du commit.

Une écriture externe pendant l’appui met à jour l’apparence ; au relâchement, basculer la valeur
courante plutôt que celle capturée au début. Cela évite d’écraser une commande applicative récente.

Les surcharges State<T>& sont converties en Binding et ne gardent pas un emprunt brut. Après
destruction du State, Binding::valid() devient false, get() conserve la dernière valeur lisible,
set() est ignoré et observe() reste inactive. Aucune notification implicite de destruction :
vérifier valid à chaque dispatch/checkpoint pour couper mutation et callbacks utilisateur du modèle
disparu. Les modèles applicatifs capturés par une fermeture ne sont pas prolongés par Binding.

Une observation externe invalide la présentation sans simuler de geste utilisateur. Notifications
State synchrones : snapshot stable, ajouts au passage suivant, retraits sautés et écritures
récursives coalescées. Après exception, la valeur publiée demeure, les notifications du passage
restant sont interrompues et le dispatch doit être réutilisable.

## 4. Interactions

Clic intérieur au relâchement bascule une fois. Déplacement dehors, PointerCancel et perte de focus
n’écrivent pas ; aucun basculement à PointerDown, contrairement au Toggle existant.

Espace : armement KeyDown puis basculement KeyUp ; Entrée : premier KeyDown, répétitions supprimées.
Molette sans effet, glisser hors bouton annule.

ReadOnly reste focusable et expose la valeur mais consomme les entrées mutantes. Disabled suit la
disponibilité héritée et retire les gestes armés.

Dans ToggleGroup/Toolbar, les flèches déplacent le focus sans changer cette valeur ; seule une
activation choisit ou retire l’option indépendante.

## 5. Mesure et layout

Mesure reprend Button : label ou contenu, padding horizontal, hauteur contrôle et largeur minimum.
La valeur bool ne change pas les dimensions.

Clipper un contenu iconique sous contraintes étroites. Groupe et barre d’outils arrangent le
contrôle ; le bouton ne lit pas la largeur de voisins depuis paint.

Resize pendant capture utilise les limites courantes ; le point de relâchement est en coordonnées
logiques.

## 6. Présentation et invalidation

selected applique la face enfoncée persistante. hovered/pressed peuvent modifier les couleurs, puis
focus conserve son ring ; priorité disabled et read_only clairement appliquée avant l’affordance
active.

Le style segmenté vient de ToggleGroup localement. Il ne mutate pas les globals Theme ni le style de
boutons extérieurs à ce groupe.

Binding change : paint seule sauf patch selected comportant une métrique, auquel cas layout. Garder
la place du ring pour éviter des déplacements lors du focus.

## 7. Accessibilité

Cible : Button avec checked Checked/Unchecked et action Toggle ; SemanticRole::Toggle est réservé
ici à l’interrupteur visuel existant. Un rôle ToggleButton dédié pourra être ajouté séparément.

La valeur persistante est annoncée même hors focus ; le nom reste le label pour un contenu
uniquement iconique. ReadOnly supprime les actions mutantes.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

L’activation désarme, libère capture et prend copie du Binding avant set(). Un observateur qui
retire le bouton ne laisse aucun accès postérieur au composant.

Aucun rollback de la valeur publiée en cas d’exception d’un observateur ; la prochaine interaction
repart de la valeur courante.

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

Dépendances : [button](button.md), [toggle_group](toggle_group.md), disponibilité, ThemeBinding,
State et machine d’activation.

Label vide avec icône : fournir un nom accessible ; callback/observateur externe peut remplacer le
State mais jamais conserver un contexte d’input.

Un groupe exclusif doit utiliser SegmentedControl, pas observer plusieurs ToggleButton pour imposer
une exclusivité implicite.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/toggle_button.hpp` et `src/toggle_button.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

ToggleButtonStyle et variantes de contenu restent dans ce couple ; le noyau partage la machine
d’activation avec Button sans importer widgets_basic.inc comme implémentation.

Inscrire `src/toggle_button.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les
includes collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou
plugin dans l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`toggle_button_persistent` : la valeur selected reste après relâchement ; pressed temporaire
disparaît.

`toggle_button_cancel` : sortie et PointerCancel ne basculent pas ; molette ne change rien.

`toggle_button_external_during_press` : écriture externe puis activation bascule la valeur la plus
récente.

`toggle_button_read_only` : focus/lire permis, clavier et pointeur ne publient aucune valeur.

`toggle_button_remove_observer` : observateur retire le bouton ou lève ; suivant réutilisable.

`toggle_button_group_independent` : flèches déplacent seulement focus ; plusieurs valeurs peuvent
rester true.

Ajouter `examples/features/toggle_button.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
