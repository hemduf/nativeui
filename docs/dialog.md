# Dialog

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Contrôleur de dialogue modal portable, corps composé de Spec et actions identifiées. Ajouter la variante alerte dans le même couple de fichiers, sans deuxième composant AlertDialog.

Implémentation actuelle dans [dialog.hpp](../include/nativeui/dialog.hpp), `Dialog`, `DialogSpec`, `DialogAction`, show/active/close. Une seule génération Dialog par UI ; service [Overlay](../include/nativeui/overlay.hpp) unique pour focus modal/barrière/presentation.

MyGo : `ui/widgets.go`, `Modal` ; `ui/base.go`, `DialogBase` ; `ui/feedback.go`, `AlertDialog`. MyGo Modal dismiss outside, Alert bloque outside et infère Cancel depuis texte. NativeUI bloque déjà outside ; conserver son rôle Cancel explicite, indépendant de langue.

## 2. API publique et composition

API actuelle exacte : `explicit Dialog(UI&)`, `Completion=std::function<void(DialogResult)>`, `show(DialogSpec, Completion)->DialogShowResult`, `active() const noexcept`, `close()->bool`, destructeur noexcept ; contrôleur non copiable/non déplaçable. Aucun `spec()` builder à inventer.

Exemple existant vérifié, uiInstance est une UI déjà montée :

```cpp
ui::Dialog dialog{uiInstance};
ui::DialogSpec request;
request.title = "Confirmer";
request.body = ui::Label{"Appliquer les modifications ?"}.spec();
request.actions = {
    {"cancel", "Annuler", true, ui::DialogActionRole::Cancel},
    {"apply", "Appliquer", true, ui::DialogActionRole::Default}
};
auto shown = dialog.show(std::move(request), [](ui::DialogResult) {});
(void)shown;
```

Ajouts cibles proposés : `AlertDialogSpec { std::string title; std::string message; std::vector<DialogAction> actions; }` et `DialogShowResult show_alert(AlertDialogSpec, Completion)`. Ce helper crée un DialogSpec et son corps de message ; il ne déduit pas les rôles depuis labels.

Ajouter à la fin de DialogSpec `std::optional<DialogStyle> style` et `std::string description`, conservant les champs historiques title/body/actions/backdrop_color et leur défaut. DialogStyle possède width/padding/gaps/palette ; aucun slot Theme supposé existant.

## 3. État, propriété et notifications

Dialog emprunte UI et tient un état weak par génération. UI doit vivre tant que les opérations directes du contrôleur sont utilisables ; après teardown show retourne Unavailable et close false.

show possède body/actions/Completion ; pas de State<bool> parallèle. Résultats actuels Shown/Busy/InvalidSpec/Unavailable conservés ; completion décrit Action(id) ou Dismissed, exactement une fois sur clôture effective.

Données actions sont un snapshot immuable pour la session. Modèle externe de corps via Binding est valide, mais actions ne sont pas remplacées implicitement pendant une pression. close conserve le premier résultat demandé pendant une reprise.

## 4. Interactions

Outside et backdrop consomment pointeur sans fermer. Tab/Shift+Tab restent dans la portée modale ; focus initial sur Default enabled, sinon premier descendant disponible, sinon panneau.

Enter remonte au Default uniquement si le descendant focalisé l’a ignoré ; TextInput/TextArea peuvent le consommer. Escape = Action du Cancel enabled, sinon Dismissed. Boutons Disabled ne s’activent ni au pointeur ni sémantiquement.

close programmatique = Dismissed. Déactivation UI ferme sans completion selon le contrat actuel. PointerCancel annule les gestes enfants ; aucun drag du panneau ni scroll global, seulement ScrollView du corps.

## 5. Mesure et layout

Chemin historique : marge viewport24, maximum width560, padding20, section gap12 et actions gap8 unités logiques. Panneau centré ; largeur/hauteur bornées au viewport.

Titre et actions hors scroll ; corps dans ScrollView vertical owned par le panneau. Viewport minuscule priorise chrome et réduit corps à zéro sans hauteur négative.

Contrat enrichi : la rangée d’actions trop large se replie en plusieurs lignes selon ordre visuel, sans changer les rôles/default du parcours focus. Aucun bouton ne déborde du panneau ; corps reste le seul contenu scrollable.

## 6. Présentation et invalidation

DialogStyle nouveau fournit overrides typographiques/chrome/layout ; défaut conserve palette et géométrie actuelle hors cas d’overflow corrigé. Backdrop_color historique garde sa priorité explicite.

Variante alerte construit titre/message et actions dans le même panneau. Default visuellement accentuée ; Cancel/Normal gardent chrome standard, sans règle « dernier bouton = default ».

Theme et corps Binding invalidés selon services existants. Ouverture/fermeture passent par structural invalidation Overlay ; aucune boucle d’animation ou second stack modal.

## 7. Accessibilité

Contrat cible : Dialog nommé title, description explicite ou message d’alerte ; Group/actions/corps conservent leurs rôles. SemanticRole::AlertDialog est absent : utiliser Dialog avec description ; extension dédiée serait un contrat séparé.

Modal masque la navigation vers les descendants de fond selon l’arbre sémantique cible ; ordre visuel des actions ne doit pas être confondu avec Default priorisé pour focus.

Ponts natifs T068 différés. Les inputs du corps utilisent committed text ; preedit/candidate rectangles natifs restent différés DESIGN17.4, sans ajout implicite par Dialog.

## 8. Cycle de vie et récupération

show prépare contenu/callbacks avant acquisition de slot. Publication échouée libère génération et overlay ; échec close/reconciliation conserve handle/génération/resultat pour retry exact, pas session artificiellement Busy.

La fermeture depuis dispatch est différée intégralement au checkpoint retenu, sans fallback synchrone sur échec d’enqueue. Slot et état local sont terminal avant completion ; une completion qui lève n’est jamais rejouée.

Particularité actuelle à conserver : destructeur active close peut invoquer completion ; il invalide d’abord callbacks retenus, force cleanup terminal et contient toutes exceptions. Déactivation/teardown UI abandonnent sans completion. Aucun état global ; top-level destruction seulement aux limites prouvées sûres.

## 9. Dépendances et cas limites

Dépend de Overlay, FocusScope, ScrollView, Button, Label/TextService et DialogState existants. Ne pas remplacer UI::dialog_state_ ni son flux de checkpoint.

Body sans factory = InvalidSpec. Id vide/dupliqué, plusieurs Default ou Cancel = InvalidSpec avant slot. Zéro action autorisé ; Escape/close restent opérationnels.

Titre/message vides autorisés. show_alert donne un body Spec valide même avec message vide. Deux Dialog d’une UI se partagent le slot Busy ; deux UI restent indépendantes. Actions disabled et destruction en completion sont des cas obligatoires.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/dialog.hpp` et `src/dialog.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `dialog.hpp` existant et detail/dialog_state.hpp. Préserver toutes les déclarations publiques, structs/enums et includes historiques de dialog.hpp. Déplacer vrai contrôleur, panel/chrome et callbacks en dialog.cpp ; les helpers de composition ne deviennent pas un header lourd.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `dialog_legacy_results` : Shown/Busy/InvalidSpec/Unavailable et Action/Dismissed sont conservés.
- `dialog_default_enter` : Enter consommé par descendant ne déclenche pas Default.
- `dialog_alert_roles` : rôles explicites non liés aux labels traduits.
- `dialog_action_wrap` : actions longues replient dans un viewport minuscule sans overflow.
- `dialog_close_fault` : reconciliation qui lève conserve premier résultat et retry owner.
- `dialog_destructor_completion` : cleanup no-throw contient completion qui lève et libère slot.
- `dialog_reentrant_show` : completion peut ouvrir le dialogue suivant sans fermer sa génération.

Créer `examples/features/dialog.cpp` et la cible `nativeui_example_dialog`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Réutiliser les exemples `examples/features/t063_dialog.cpp` et les tests de transactions Dialog actuels ; ajouter alertes, sémantique et overflow sans affaiblir les preuves de reprise.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
