# RadioButton<T> et RadioGroup<T>

Statut : **existant à extraire**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Proposer des choix exclusifs partageant un contrôleur de sélection. RadioGroup est un objet de
modèle/focus, pas un composant à mesure indépendante.

NativeUI : [widgets_checkbox_radio.inc](../include/nativeui/detail/widgets_checkbox_radio.inc),
RadioGroup<T>, RadioButton<T>, registry de valeurs et noyau detail::RadioButtonComponent ;
[focus_group.hpp](../include/nativeui/detail/focus_group.hpp) définit la participation retenue.

MyGo : `ui/widgets.go`, `Radio[T]` ; `ui/toggle.go`, `RadioGroup`. Le regroupement de focus et choix
typed existent déjà ; conserver API et contraintes effectives, sans imposer T=int.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
template<class T>
class RadioGroup {
public:
  explicit RadioGroup(Binding<T> selected);
  explicit RadioGroup(State<T>& selected);
};
template<class T>
class RadioButton {
public:
  RadioButton(const RadioGroup<T>& group, T value, std::string label);
  RadioButton&& style(RadioStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<int> size{1};
ui::RadioGroup<int> group{size};
auto small = ui::RadioButton(group, 1, "Petit").spec();
auto large = ui::RadioButton(group, 2, "Grand").spec();
```

Le header conserve constructeurs, deduction et la condition d’égalité de State ; le registry typed
produit les adaptateurs is_selected/select/observe vers le noyau non template.

RadioStyle reste le style public existant. Les conteneurs Row/Column arrangent les Specs radio ; pas
de fichier radio_group.cpp vide ni composant public supplémentaire pour le contrôleur.

## 3. État, propriété et notifications

Le group possède copies Binding<T>, token de focus et registre de valeurs partagés ; RadioButton en
copie les contrôles internes. L’objet RadioGroup lexical peut sortir de portée après fabrication des
Specs, sans invalider ces copies.

T value est possédé avec identité de registry stable. Valeurs en doublon dans un groupe sont déjà
rejetées par registre ; conserver le rejet et la durée de vie de ses identités.

Sélection hors options : toutes les faces non selected, mais le premier participant disponible sert
d’entrée Tab. Une écriture externe ne force aucune valeur par défaut au mount.

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

Clic au relâchement et Espace au KeyUp choisissent value ; une valeur déjà sélectionnée ne génère
pas de notification State supplémentaire. Entrée n’est pas activatrice dans la source actuelle.

Focus de groupe : un arrêt Tab, entrée sur la valeur sélectionnée disponible ; flèches/Home/End
parcourent et sélectionnent les participants disponibles via le runtime commun.

ReadOnly empêche select(), tout en permettant focus/lecture. Disabled et cachés sont exclus du choix
par navigation ; valeurs sélectionnées disabled restent observables.

Molette ignorée ; PointerCancel, retrait ou perte de focus annulent l’armement sans désélectionner.
Revenir hors puis à l’intérieur suit la machine PressActivationState existante.

## 5. Mesure et layout

Mesure : diamètre externe, leading_padding, label_gap, mesure du label et minimum_width ; hauteur
control_height. Conserver tous champs et résultats RadioStyle.

Le groupe logique n’a pas de mesure ; les radios peuvent être arrangées dans plusieurs conteneurs,
mais le focus suit l’ordre retenu valide du même token.

La marque centrale n’entraîne pas de changement de largeur par défaut. Clipping du label et centre
du disque utilisent l’espace réellement alloué en unités logiques.

## 6. Présentation et invalidation

Outer/inner/mark colors et rayons sont issus du style résolu ; selected détermine la marque, pressed
l’interaction passagère. Focus ring ne remplace pas selected.

La classification d’invalidation tient compte de visibilité de la marque même sans couleur changée.
Patches selected à métriques différentes imposent layout.

Deux groupes partageant même State peuvent montrer la même valeur tout en gardant des tokens focus
différents. Aucun nom global de groupe.

## 7. Accessibilité

Cible : RadioButton avec selected/checked et action Select, nom label ; exposer relation de group
via le conteneur Group nommé par l’application.

RadioGroup n’est pas une Spec et n’introduit aucun nœud sémantique autonome. RadioButtonComponent ne
fournit pas actuellement semantics dans la source étudiée : ajout de publication requis avec
extraction.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Le registry de valeurs et token restent possédés par références partagées des Specs/composants ;
retirer une option libère son inscription conformément au registry existant.

Le noyau conserve callbacks is_selected/select/observe effacés et objets d’invalidation détachés.
Copier select puis le lancer après fin de capture ; observer réentrant ne doit pas retrouver un nœud
via une adresse recyclée.

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

Dépendances : State, focus de groupe commun, TextService, ThemeBinding.
[segmented_control](segmented_control.md) est un autre rendu exclusif, pas une réécriture de
RadioGroup.

T utilisateur peut lever lors de copy/equals : les inscriptions et montage doivent être
transactionnels. Un échec d’égalité pendant paint/layout ne publie pas une demi-frame.

Vide/tous disabled : aucun arrêt navigable ; retirer la sélection ne réécrit pas le Binding. Les
labels identiques n’affectent pas le registry fondé sur value.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/radio_button.hpp` et `src/radio_button.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Conserver RadioGroup<T>, RadioButton<T>, guides/surcharges et registry typed dans le header ; le
véritable noyau detail::RadioButtonComponent et peinture/interactions sont déplacés au .cpp.

widgets_checkbox_radio.inc devient un point d’entrée de compatibilité sans duplicata
d’implémentation ; ne pas déplacer les types publics déjà utilisés dans un namespace privé.

Inscrire `src/radio_button.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`radio_button_exclusive` : sélectionne un choix ; valeur identique ne repasse pas les observateurs.

`radio_button_roving` : un arrêt Tab ; flèches/Home/End sélectionnent en sautant disabled.

`radio_button_duplicates` : valeurs duplicate refusées ; labels duplicate avec valeurs distinctes
admis.

`radio_button_group_lifetime` : Specs restent valides après destruction lexicale du contrôleur.

`radio_button_generic_value` : type utilisateur compile sans instanciations explicites prédéfinies.

`radio_button_observer_copy_throw` : exception equality/copy/select et retrait réentrant restaurent
registry/focus et next action.

Ajouter `examples/features/radio_button.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
