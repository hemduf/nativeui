# TimeInput

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Heure civile optionnelle de la journée sous forme de segments HH:MM, avec secondes en option. Aucun jour, fuseau, timestamp ou traitement de DST.

Absent de NativeUI ; les chemins Input committed et le modèle State sont disponibles. Le composant gère chiffres et segments sans moteur d’édition riche.

MyGo : `ui/timeinput.go`, `TimeInput`, segments hours/minutes. Haut/bas tournent chaque segment, chiffres composent deux digits, gauche/droite déplacent le focus. MyGo conserve date et seconds/timezone ; la cible conserve seconds dans un entier de secondes depuis minuit.

## 2. API publique et composition

API cible proposée :

```cpp
class TimeInput {
public:
    using Value = std::optional<std::chrono::seconds>;
    TimeInput(std::string label, Binding<Value> value);
    TimeInput(std::string label, State<Value>& value);
    TimeInput&& show_seconds(bool value = true) &&;
    TimeInput&& clearable(bool value = true) &&;
    TimeInput&& on_change(std::function<void(Value)> callback) &&;
    TimeInput&& style(TimeInputStyle value) &&;
    Spec spec() &&;
};
```

Défauts : format 24h, HH:MM, secondes masquées mais conservées, valeur absente autorisée, clearable=true. Domaine de toute valeur valide : seconds{0}..seconds{86399}. TimeInputStyle décrit textes, separators, selection/focus, padding et largeur des segments.

Exemple cible proposé :

```cpp
ui::State<ui::TimeInput::Value> alarm{
    std::chrono::seconds{9 * 3600 + 30 * 60}};
auto field = ui::TimeInput{"Alarme", alarm}
    .show_seconds(false).spec();
```

## 3. État, propriété et notifications

Binding porte la valeur entière ; surcharge State convertie immédiatement. Source détruite : valid=false, get dernière valeur, set ignoré/observe inactif sans notification automatique ; revalider avant edit et ne pas notifier on_change. Segment actif et buffer de 1–2 chiffres appartiennent à l’instance. Aucun timer global pour digits.

Chaque edit accepté met à jour la valeur complète puis on_change une fois si réellement changée. Focus et déplacement entre segments ne modifient pas le modèle. Externe remplace segments/buffer sans on_change.

Absence n’est pas minuit : afficher --:--. La première modification part de 00:00:00 puis applique le chiffre ou pas ; clear remet nullopt. Masquer seconds ne les arrondit jamais.

## 4. Interactions

Clic choisit le segment, Tab/Shift+Tab parcourent segments puis contrôles voisins. Gauche/droite va au segment adjacent sans wrapping de focus. Haut/bas incrémente/décrémente avec modulo 24 ou 60 indépendant, sans retenue entre segments.

Chiffres committed ASCII : premier digit lance buffer ; second valide si dans domaine, sinon devient premier digit nouveau. À deux chiffres ou digit qui ne peut commencer un nombre valide, avancer au segment suivant ; au dernier rester.

Enter valide le buffer local et laisse le modèle ; Escape abandonne seulement buffer incomplet, pas les valeurs déjà publiées. Molette/glissement ignorés. ReadOnly lit/navigue mais consomme edits sans écrire ; Disabled n’est pas focusable.

## 5. Mesure et layout

Chaque segment garde une largeur de deux chiffres max, mesurée avec TextService, padding et séparateurs non interactifs. Minimise les jumps 09→10 ; pas de largeur au rythme du buffer.

Place bornée : clipper segments dans les bounds ; pas de chevauchement avec clear. Place nulle interdit clic et ne produit aucune division.

Coordonnées logiques ; rectangles du focus et des segments proviennent du même layout. Show_seconds modifie structure/focus au checkpoint, jamais pendant traversée active.

## 6. Présentation et invalidation

TimeInputStyle nouveau : chrome de groupe, texte inactif, segment sélectionné, separators, focus ring et disabled/read-only. Nom label indépendant de la représentation.

Changement externe/chiffre = paint/semantics. Édition buffer seule = paint du segment. Show_seconds et métriques style = structure/layout. Ne pas exiger API font-features absente ; choisir police de test déterministe.

Aucun curseur clignotant requis pour des segments ; aucune animation/timer continu. Valeur invalidée par externe donne une présentation invalide --:-- et description, sans writeback.

## 7. Accessibilité

Contrat cible : Group et deux/trois enfants Custom portant noms « heures », « minutes », « secondes », numeric_value/range et actions Increment/Decrement/SetValue/Focus.

SemanticRole::Stepper absent ; ne pas l’annoncer comme livré. Contrat de SetValue segment borné 0..23/59, sans modification des autres segments ni de date/fuseau inexistants.

TextInput committed disponible ; preedit/candidate rectangles non livrés DESIGN17.4. Les snapshots sont testables mais les ponts natifs T068 restent différés.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement RAII et état buffer réinitialisé au démontage/perte de focus. Annuler input text natif lors du démontage si le segment l’active.

Avant Binding.set snapshot de valeur/options ; callbacks réentrants peuvent remplacer modèle/visibilité, donc revalider vie avant de déplacer focus. on_change commencé jamais rejoué.

Exception remet guards/segment en état cohérent sans compléter automatiquement les digits. Destruction no-throw et callback-silent. Pas de timer/read-only global ou partage de buffer entre instances.

## 9. Dépendances et cas limites

Dépend de chrono, Binding, Input committed, Focus et TextService ; aucune boîte native ou parser locale système.

Valeur externe négative ou >=86400 : diagnostic de domaine et affichage absence invalide, source intacte. Nouvel edit part du zéro sûr. Fraction de seconde absente par choix du type seconds.

Externe pendant buffer écrase buffer, même si changement provient d’une normalisation de Binding. Clipboard texte arbitraire et AM/PM ne sont pas acceptés dans v1 ; seuls digits committed sont edits.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/time_input.hpp` et `src/time_input.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Input/State/Focus/TextService existants. Value est optional<chrono::seconds>, jamais time_point ; helpers de segmentation restent dans le couple non template.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `time_input_segment_wrap` : 23→00 et 59→00 ne modifient pas les autres segments.
- `time_input_digits` : 0/9/2/4 et 59/60 suivent exactement les règles de buffer.
- `time_input_hidden_seconds` : modifier HH:MM conserve seconds cachées.
- `time_input_absent_invalid` : absence et valeurs hors domaine ne se confondent pas avec minuit.
- `time_input_external_focus` : externe et retrait du segment actif effacent buffer/focus correctement.
- `time_input_throw_recover` : observer qui lève ne double pas edit et permet le digit suivant.

Créer `examples/features/time_input.cpp` et la cible `nativeui_example_time_input`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
