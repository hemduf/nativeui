# NumberInput

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer un double borné à partir d’un texte et de flèches, avec draft temporairement incomplet. La
valeur numérique ne reçoit jamais NaN ni texte invalide.

NativeUI a TextInput et SliderDomain float, mais aucun champ numérique. Réutiliser le noyau
d’édition sans relier directement son draft string au State<double>.

MyGo : `ui/number.go`, `NumberInput`. Publie dès que saisie parse dans plage, ajoute Up/Down et
boutons, puis formate à la perte de focus. Cible conserve live-valid et formalise les conflits
externes.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class NumberInput {
public:
  NumberInput(std::string label, Binding<double> value);
  NumberInput(std::string label, State<double>& value);
  NumberInput&& range(double minimum, double maximum) &&;
  NumberInput&& step(double value) &&;
  NumberInput&& precision(unsigned digits) &&;
  NumberInput&& on_submit(std::function<void(double)> callback) &&;
  NumberInput&& style(NumberInputStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<double> copies{1.0};
auto count = ui::NumberInput("Copies", copies).range(1.0, 99.0).step(1.0)
    .precision(0).on_submit([](double) {}).spec();
```

Defaults : range[0,100], step=1, précision dérivée de la représentation décimale minimale du step,
bornée à 17 digits. precision(0..17) explicite remplace ce calcul ; hors limites rejeté.

NumberInputStyle contient TextInputStyle, StepperStyle, gap et message/couleur invalid. Style des
sous-parties dans le couple NumberInput ; pas de callbacks de parsing arbitraires requis pour v1.

Parser locale-indépendant : trim espaces ASCII, signe, décimales point et exposant ; refuser
trailing bytes, NaN/Inf et overflow. Une virgule ne représente pas un séparateur décimal.

## 3. État, propriété et notifications

State<double> est valeur validée ; string draft, validity et focus_baseline sont locaux. Un draft
parse finite et dans plage publie live la valeur saisie sans snap au step ; la granularité n’impose
pas l’arrondi lors frappe.

Textes “”, “-”, “1e” sont des drafts incomplets sans mutation. Une valeur hors plage reste draft
invalid et affiche erreur ; blur restaure la représentation de dernière valeur valide.

Changement externe différent pendant focus remplace draft et baseline avec sa valeur effective et
termine composition locale ; il gagne sur un draft ancien. Une publication identique issue de
l’édition ne reformatte pas la frappe.

Enter valide le draft valide, met à jour baseline, formate et appelle submit une fois ; Escape
restaure baseline par une écriture si nécessaire. Les flèches relisent la valeur actuelle puis
snap/clamp sur grille comme Stepper.

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

Text editing reprend TextInput : sélection, clipboard, undo/redo draft. Up/Down changent valeur de
step et reformatent ; Home/End restent navigation textuelle, sans sauter aux bornes.

Stepper attenant prend l’action plus/moins et son maintien 400/80 ms ; clic garde le focus du champ
pour continuer la saisie. Un seul Tab stop sur l’éditeur, actions stepper via clavier/sémantique.

Enter invalid/incomplete garde focus et erreur, sans submit ; blur invalid abandonne draft et
affiche dernière valeur valide. Escape annule à baseline ; PointerCancel du Stepper conserve les
increments live déjà publiés.

ReadOnly permet sélection/copie sans parse-mutant ni flèches ; disabled bloque édition. Molette
ignorée pour éviter modification accidentelle d’un nombre pendant scroll.

Committed text normal suit TextInput ; absence de pont natif preedit est explicite. Pendant
composition synthétique active, ne parser qu’après Commit, pas chaque preedit Update.

## 5. Mesure et layout

Champ prend largeur TextInputStyle, Stepper largeur compacte, gap ; hauteur commune. Préserver une
largeur indépendante de la longueur de draft pour éviter déplacement pendant frappe.

Le message invalid peut être exposé en description et peint dans espace réservé par style, sans
affaissement du champ ; Form/Field gère une ligne d’erreur externe si besoin.

Caret/selection clip dans aire de texte. Toutes coordonnées logiques ; un format scientifique long
défile horizontalement.

## 6. Présentation et invalidation

Validité invalid affecte border/text/message, pas la valeur présentée au Stepper. Blank placeholder
et nombre zéro sont des contenus distincts.

Le format est locale-indépendant, fixed aux digits choisis ; l’arrondi de présentation ne republie
pas une valeur arrondie au blur.

Draft change paint et scroll, géométrie/style metrics layout ; le Stepper repeint sa disponibilité
près bornes. Aucun validator/parse appelé depuis paint.

## 7. Accessibilité

Cible : Custom avec numeric_value/value_range et text_value draft, nom label, actions
Increment/Decrement/SetValue quand mutable.

SpinButton n’existe pas dans SemanticRole actuel ; invalid/error est description backend-neutre tant
qu’aucun champ dédié n’est ajouté. Les flèches ne doublent pas les arrêts Tab.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer parse et représentation avant commit ; libérer capture/context puis set. Callback submit
copié avec double stable, après état local cohérent, sans accès à this après appel.

Observer peut remplacer la valeur, retirer champ ou lever. Garder valeur déjà commise, reprendre
draft à prochain checkpoint ; ne republier pas automatiquement la valeur du draft après un échec.

Réponse clipboard génération et contrôle valid vérifiés. Démontage annule draft/composition et
répétition du Stepper sans submit/rollback destructeur.

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

Dépendances : [text_input](text_input.md), [stepper](stepper.md), domaine double privé,
TextService/State. Ne pas changer SliderDomain float pour cet ajout.

Domain min<=max et step>0 finite ; min==max laisse un champ constant ReadOnly effectif pour
modifications numériques. Externe NaN/inf rendu min avec description invalid sans correction
automatique.

Double aux bornes extrêmes, exponent overflow, step sous-normal et precision17 doivent être testés
sans addition overflow produisant NaN. User paste “1,2” reste invalid plutôt que silencieusement 12.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/number_input.hpp` et `src/number_input.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Header expose NumberInput/NumberInputStyle et callbacks ; .cpp possède draft/parser/formatter,
synchronisation, composition TextInput/Stepper et rendu.

Noyaux texte/stepper sont partagés par interfaces privées, sans copier leur implémentation dans un
header NumberInput.

Inscrire `src/number_input.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`number_input_parse` : signed/decimal/exponent admis ; vide/partiel/comma/NaN/trailing/overflow sans
mutation.

`number_input_live_valid` : frappe valide publie sans snap ; format blur ne change pas double.

`number_input_external_conflict` : écriture externe gagne sur draft actif et devient Escape
baseline.

`number_input_submit_blur` : invalid Enter reste edit sans submit ; invalid blur restaure dernière
valeur.

`number_input_stepper` : increments snap/clamp, maintien, no wheel et un Tab stop.

`number_input_composition` : Update non parse, Commit parse une fois ; clipboard périmé ignoré.

`number_input_throw_remove` : observer/submit retire/ lève et prochaine action fonctionne.

Ajouter `examples/features/number_input.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
