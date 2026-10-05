# Tooltip

**Statut : existant à enrichir.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Décorateur texte d’une ancre, présenté au repos de pointeur ou au focus sans capter input/focus. Pas de contenu interactif ; un panneau interactif utilise Popover.

Présent dans [tooltip.hpp](../include/nativeui/tooltip.hpp), Tooltip builder, detail::TooltipController et TooltipComponent. Le controller possède un timer Dispatcher et un overlay NonModal/Ignore/Auto.

MyGo : `ui/widgets.go`, `Element.Tooltip`. Delay600ms, panneau près du pointeur, description accessible et innermost tooltip. NativeUI a déjà son propre délai500ms, ancre NodeId et déclenchement focus ; garder ces choix plutôt que copier les temporisations globales Go.

## 2. API publique et composition

API existante exacte : `template<class Child> Tooltip(std::string text, Child&& child)`, `delay(milliseconds) &&`, `delay(milliseconds) &`, getter `delay() const noexcept`, getter `text() const noexcept`, `spec() &&`, constantes kDefaultDelay=500ms et kDefaultMaxWidth.

Exemple existant vérifié :

```cpp
auto help = ui::Tooltip{"Enregistrer le document",
    ui::Button{"Enregistrer", [] {}}}
    .delay(std::chrono::milliseconds{500})
    .spec();
```

Ajout cible proposé : `Tooltip&& style(TooltipStyle value) &&` ; type nouveau avec background/text/border/radius/padding/max_width et TextStyle. Préserver les deux ref-qualified delay et leurs clamping négatif à zéro.

## 3. État, propriété et notifications

Texte/Spec enfant/style possédés, pas de Binding pour texte dans cette version. Le controller possède hover/focus/available/suppressed/pointer_active et timer ; un overlay handle précis par ancre.

Éligibilité = ancre disponible et (hover ou focus) et absence de pointer interaction. Visible et pending restent distincts ; delay ne s’écoule qu’à partir d’une vraie transition d’éligibilité.

PointerDown ou perte disponibilité supprime une présentation continue : retrouver disponibilité sous pointeur immobile ne relance pas. Transition false puis true déclenche de nouveau le délai complet. Aucun callback utilisateur de shown/hidden ajouté.

## 4. Interactions

Tooltip observe hover/focus de l’ancre sans gérer son activation. Tab/clavier appartiennent à l’enfant ; panneau Ignore n’est ni targetable ni focusable.

PointerDown dismiss la tooltip, toute gesture dans l’arbre supprime hover eligibility ; fin de gesture seule ne relance pas sous pointeur stationary. Focus alone peut armer le délai selon controller actuel.

La source fixe dismiss_on_escape=false et dismiss_on_outside_pointer_down=false : Escape n’est pas intercepté par Tooltip et suit le routage de l’ancre. PointerDown dismiss passe par son controller, pas par une deuxième politique outside. Aucun glissement, molette ou validation dans le panneau.

## 5. Mesure et layout

Décorateur mesure et place son enfant comme auparavant. Panneau texte enveloppé par `wrap_tooltip_text`, padding et max width ; tailles en coordonnées logiques.

Overlay Auto depuis NodeId ; placement/clamp entièrement gérés par service existant. Pas de réservations de place ni copie de coordonnées écran OS.

Texte très long sans espaces : envelopper aux frontières UTF-8 selon helper ; v1 n’annonce pas layout multirun. Viewport plus petit clippe selon les services, sans produire taille négative.

## 6. Présentation et invalidation

TooltipStyle nouveau matérialise champs aujourd’hui privés de TooltipSurfaceComponent ; défaut conserve palette et métriques exactes. Aucun slot Theme nouveau prétendu livré.

Style métrique/text width = layout panneau ; couleurs = paint. L’ancre ne remonte pas lors de mutation du thème ou de fermeture tooltip.

Texte vide : aucune présentation. Pas d’animation/fade par défaut, ni process-global warmup/current-tooltip. Style d’un Tooltip n’altère pas les suivants.

## 7. Accessibilité

Contrat cible : description de l’ancre renseignée par texte si sa description explicite est vide ; ne pas écraser description applicative. Le panneau décoratif ne crée pas de focus ni lecture double.

SemanticRole::Tooltip absent ; utiliser Group/Text dans snapshot si panneau publié, ou description de l’ancre seule selon contrat d’exclusion existant. Ici défaut choisi : description ancre, panneau exclu.

Ponts natifs T068 différés. Aucun IME/édition. Un texte affiché dans tooltip reste owned même après disparition visuelle de l’overlay.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement/observation de disponibilité et timer annulés au démontage. Controller et handlers sont par instance et protégés weak ; invocation différée stale = no-op.

Échec schedule_after ne lance jamais la présentation synchroniquement. Échec show remet visible=false ; échec hide conserve handle/état de reprise de fermeture, conformément à la transaction actuelle.

Destruction no-throw, shutdown sans callback applicatif. Les scopes busy/suppressed restent cohérents après exception ; un callback d’intégration commencé n’est pas rejoué. Le prochain cycle false/true reste utilisable.

## 9. Dépendances et cas limites

Dépend du DispatcherProvider/Dispatcher existant, Overlay et focus/hover retained. Pas de nouveau timing service, OS tooltip ou stack overlay.

Sans Dispatcher valide, conserver une ancre normale et aucun tooltip ; pas de thread/sleep fallback. Delay négatif clamp0, texte vide inéligible.

Ancre Hidden/Collapsed/Disabled retire pending/visible ; read-only reste admissible pour information. UI deactivate ferme et laisse observation cohérente. Tooltip imbriquées ne partagent pas une variable « courant » globale.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/tooltip.hpp` et `src/tooltip.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : `tooltip.hpp` existant. Conserver constantes/getters/ref-qualifiers/template enfant et includes historiques ; controller et surface réelle dans tooltip.cpp. Garder le seam de tests de controller ou adapter sa visibilité sans casser ses consommateurs.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `tooltip_legacy_delay` : 500ms défaut et les deux overloads delay gardent leurs résultats.
- `tooltip_pointer_suppression` : PointerDown/release stationary ne réarme pas.
- `tooltip_availability` : Hidden/Disabled puis retour stationary ne réarme pas.
- `tooltip_style_wrap` : max width/padding/enveloppe influent mesure et pixels ensemble.
- `tooltip_description` : description explicite préservée et absence de lecture double.
- `tooltip_timer_fault` : échec timer/show/hide puis prochain cycle restent récupérables.

Créer `examples/features/tooltip.cpp` et la cible `nativeui_example_tooltip`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Réutiliser `tests/t062_tooltip_tests.cpp` et `examples/features/t062_tooltip.cpp` ; enrichir style/sémantique sans réécrire les scénarios de suppression actuels.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
