# TextInput

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Saisir une ligne UTF-8 avec curseur, sélection, clipboard et validation de soumission. La valeur du
Binding est mise à jour pendant l’édition ; Entrée est un événement submit distinct.

NativeUI : [widgets_text_input.inc](../include/nativeui/detail/widgets_text_input.inc),
TextInputComponent public ; builder dans
[widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), modèle
[text_edit.hpp](../include/nativeui/text_edit.hpp). Sélection, undo/redo, max codepoints, scroll
horizontal et Escape snapshot sont présents.

MyGo : `ui/editor.go`, `TextInput`, `textInput` ; `ui/base.go`, `TextInputBase`. L’essentiel existe
déjà. Enrichissement cible : extraction du noyau, publication sémantique et sécurité des
callbacks/observations réentrants.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
using SubmitCallback = std::function<void(const std::string&)>;
TextInput(std::string label, Binding<std::string> state);
TextInput(std::string label, State<std::string>& state);
TextInput&& placeholder(std::string value) &&;
TextInput&& max_length(std::size_t value) &&;
TextInput&& on_submit(SubmitCallback callback) &&;
TextInput&& style(TextInputStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<std::string> name{"Oreto"};
auto input = ui::TextInput("Nom", name).placeholder("Votre nom")
    .max_length(128).on_submit([](const std::string&) {}).spec();
```

Default max_length=256 codepoints ; max_length(0) signifie illimité dans TextEditModel. Label est
distinct du placeholder ; ne pas ajouter password ou contrôle IME natif comme fonctionnalité
actuelle.

Conserver TextInputComponent public, son SubmitCallback et TextInputStyle ; les extensions
nécessaires à la composition spécialisée doivent être privées au noyau sans modifier ces signatures.

## 3. État, propriété et notifications

Binding<string> est vérité persistante, TextEditModel contient texte/curseur/anchor/historique
locaux. Les offsets sont alignés aux frontières codepoint UTF-8, pas des indices d’octets
arbitraires.

Chaque edit réel commit une nouvelle string ; observation d’une valeur différente remplace modèle,
remet scroll_x et historique comme actuellement. Une valeur externe identique ne repositionne pas le
curseur.

focus_snapshot pris à l’entrée ; Entrée le met à jour avant submit ; Escape restaure ce snapshot via
Binding si texte diffère. Conserver cet effet historique même si une valeur externe est arrivée
depuis l’entrée, et le documenter clairement.

Pour nouveaux composants dérivés, utiliser leur propre politique de draft/conflict plutôt que
modifier silencieusement la restauration historique de TextInput.

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

Pointeur : clic place caret ; double clic sélectionne mot, triple clic tout ; Shift étend ; drag
sélectionne avec capture et fait défiler pour garder curseur visible.

Clavier : Gauche/Droite caractère ou mot selon modifiers, Home/Haut début et End/Bas fin, Shift
étend ; Backspace/Delete retirent ; commandes Copy/Cut/Paste/SelectAll/Undo/Redo normalisées.

Entrée appelle SubmitCallback et met à jour snapshot ; Escape restaure. ReadOnly autorise
sélection/copie/focus mais bloque insert/cut/paste/undo et restauration mutante ; disabled suit
disponibilité.

TextInput nettoie CR/LF/Tab en séparations de ligne unique et retire contrôles non imprimables à
insertion. La molette ne modifie pas le texte ; pas de drag/drop applicatif automatique.

Le noyau traite Composition Start/Update/Commit/Cancel et déduplique le TextInput consécutif au
commit. Ces handlers headless ne prouvent pas une couverture native preedit/candidate ; la livraison
plateforme avancée est séparée (DESIGN §17.4).

## 5. Mesure et layout

Taille préférée issue de TextInputStyle control_width/control_height, indépendante du texte ; label
et aire de texte gardent le découpage source. Le scroll horizontal maintient caret dans content.

Mesure et hit-test utilisent TextService et style de police identique à paint ; clipping content,
selection et caret bornés au champ.

Resize et font replacement recalculent offsets/scroll en unités logiques. Max length s’applique aux
codepoints, sans couper une séquence UTF-8 ; aucun auto-grow implicite.

## 6. Présentation et invalidation

Conserver TextInputStyle avec états base/hovered/pressed/focused/disabled/read_only ; placeholder
distinct, caret/selection et underline de composition dans le noyau existant.

Tick du caret actif seulement focusé et visible ; perte de focus/démontage stoppe text input et
composition. Couleur seule paint, dimensions/police layout puis recompute scrolling.

L’entrée external n’écrit pas de valeur par défaut et n’appelle pas submit. Le renderer commun
répare les bytes UTF-8 invalides pour mesure et peinture sans changer le Binding automatiquement.

## 7. Accessibilité

Cible : TextInput, nom label, text_value modèle, read_only/enabled et actions Focus/SetValue
appropriées. Placeholder n’est pas le nom accessible ni la valeur.

La source actuelle ne publie pas semantics ; sélection/caret textuels demandent un modèle sémantique
enrichi distinct si un pont en a besoin. Éviter de revendiquer une API text range native livrée.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

La source observe actuellement avec capture this ; l’extraction doit remplacer cette dépendance
dangereuse par token/runtime détaché pour résister au retrait par un observateur antérieur.

Submit actuel poursuit des opérations de modèle après on_submit : cible copie texte/callback,
termine selection/invalidation avant l’appel et ne relit plus le composant ensuite.

Clipboard est différé : réponse conserve un jeton faible d’instance et génération de requête.
Réponse après retrait, State invalid, passage ReadOnly ou nouvelle requête est ignorée sans
insertion.

Échec insert/clipboard/submit restaure toutes les captures et flags de dispatch. Destruction annule
composition sans commit ni submit, no-throw.

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

Dépendances : TextEditModel, TextService, PlatformServices clipboard/text input, ThemeBinding et
focus ; [text_area](text_area.md) partage le moteur d’édition.

Texte vide, >max fourni externement, UTF-8 multioctet/malformed et remplacement externe pendant
sélection suivent le modèle existant ; tester roundtrip de la valeur plutôt qu’inventer une
validation de contenu.

Aucune promesse full grapheme navigation ou mot linguistique : l’édition actuelle est codepoint et
classes mots du modèle. La limitation native IME est explicite, sans bloquer les committed text.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/text_input.hpp` et `src/text_input.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Déplacer TextInputComponent public et builder vers text_input.hpp/text_input.cpp en préservant les
signatures, max defaults et style header historique.

Le .cpp porte callbacks sûrs, abonnement détaché, selection/clipboard/scroll/caret/paint ; le header
conserve seulement adaptateurs nécessaires aux autres builders, sans un .inc de comportement
permanent.

Inscrire `src/text_input.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`text_input_utf8_max` : insert multioctets respecte limite codepoint et normalise single-line sans
découpe byte.

`text_input_selection_history` : clics/drag/word/Shift, Copy/Cut/Paste et undo/redo donnent
résultats attendus.

`text_input_submit_escape` : Entrée snapshot puis submit ; Escape restaure baseline historique avec
external change.

`text_input_composition` : synthetic preedit ne commit pas ; Commit suivi texte identique ne double
pas.

`text_input_stale_clipboard` : réponse après retrait/ReadOnly/modèle invalid ignorée.

`text_input_remove_throw` : observer ou submit retire/ lève ; caret/capture/dispatch réutilisables.

`text_input_headless_clip` : caret/selection/placeholder restent clippés à plusieurs fonts/scales.

Ajouter `examples/features/text_input.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
