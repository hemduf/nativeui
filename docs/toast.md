# Toast

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Contrôleur de messages temporaires et d’actions courtes, stack en bas de viewport. Il n’ouvre pas une fenêtre ni un dialogue modal et ne vole pas le focus.

Absent de NativeUI. [Overlay](../include/nativeui/overlay.hpp) et [Dispatcher](../include/nativeui/dispatcher.hpp) sont disponibles, mais aucun service d’annonce native livré ne doit être supposé.

MyGo : `ui/toast.go`, Context.Toast, Context.ToastAction, toast.life et buildToasts. Durée4s simple/8s action, dédup par message et pause hover, annonce sans focus. La cible reprend ces usages avec ids/handles explicitement isolés.

## 2. API publique et composition

API cible proposée :

```cpp
struct ToastSpec {
    std::string message;
    std::string action_label;
    std::function<void()> action;
    std::optional<std::chrono::milliseconds> duration;
};
enum class ToastShowStatus { Shown, InvalidSpec, Unavailable };
struct ToastShowResult { ToastShowStatus status; ToastHandle handle; };
class Toast {
public:
    Toast(UI& ui, Dispatcher dispatcher);
    Toast(const Toast&) = delete;
    Toast& operator=(const Toast&) = delete;
    ~Toast() noexcept;
    ToastShowResult show(ToastSpec value);
    bool dismiss(ToastHandle handle);
};
```

Toast est non déplaçable également. ToastHandle est opaque, owner faible et id monotone, valid() et bool comparabilité ; il ne prolonge pas UI/controller. Défault durée4s ou8s si action ; duration explicite strictement positive. Action sans label ou label sans action = InvalidSpec.

Exemple cible proposé : `ui::Toast notices{uiInstance, dispatcher}; auto shown = notices.show({.message = "Document enregistré"}); (void)shown;`. Le contrôleur possède ses messages tant qu’il vit ; sa destruction les abandonne proprement. Les panneaux produisent des Spec internes, sans faux builder public spec().

## 3. État, propriété et notifications

Contrôleur possède messages/actions, handles, elapsed restant, timers et son overlay de stack. UI/Dispatcher empruntés par token weak ; tout état mutable est local.

Même message dans ce contrôleur : remplacer l’ancienne entrée et créer nouvel id/durée ; ancien handle devient stale. Messages différents s’empilent, plus récent en bas. Aucune dédup globale inter-UI/inter-controller.

Action s’exécute une fois après retrait logique de son toast ; dismiss/expiration sont silencieux. Externe ne réécrit pas de State ; show explicite est la seule entrée de données.

## 4. Interactions

Panneau simple ne prend pas focus ; action Button peut être atteinte via Tab et activée Enter/Space/pointeur. La présence d’un toast ne change pas le focus courant.

Pause durée restante pendant hover ou focus dans le toast (ajout cible pour accès clavier). Reprise au départ du dernier trigger ; pas de reset de la durée restante.

Aucun glissement/molette ni modal barrier ; les clics hors stack passent aux contrôles sous-jacents. Escape n’efface pas arbitrairement tous les messages ; dismiss est programmatique dans cette v1.

## 5. Mesure et layout

Stack largeur au plus viewport-48 DIP, centrée horizontalement et en bas avec marge24 DIP, gap8. Message wrap, bouton action garde sa largeur ; viewport minuscule clippe sans overlap hors bounds.

Extension additive nécessaire du service : `OverlayPlacement::ViewportBottomCenter` ajouté en fin d’enum. Quand ancre absente, il place les bounds naturels de la stack au bas du viewport ; les placements existants sans ancre restent Center.

Le contenu natural stack porte l’inset inférieur24 et horizontal ; OverlayEntry ne couvre que ces bounds, pas une surface invisible pleine fenêtre qui intercepterait tous les inputs. Position réévaluée au resize par le service unique.

## 6. Présentation et invalidation

Style privé v1 issu palette/spacing ; surface lisible, textes contrastés, action accentuée. Pas d’option ToastStyle publique tant aucun configurateur prévu ; les règles visuelles sont fixées ici.

Timer ponctuel sur chaque expiration et callbacks de pause via Dispatcher ; les deadlines/remaining utilisent exactement son current_time injecté, accessible au contrôleur par friendship privée Dispatcher→Toast, sans seconde horloge publique ni steady_clock::now direct. Pas de poll/frame continu. Aucun fade requis v1 ; aucune dépendance reduced-motion pour la durée.

Insertion/retrait = structure/semantics ; hover/focus = deadline/presentation, sans reconstruire la stack ni réannoncer le message. Plusieurs show ne prennent pas de thread supplémentaire.

## 7. Accessibilité

Contrat cible : Group avec Text(message), Button(action_label) si action. SemanticRole::Status et annonce native n’existent pas dans l’API source : ne pas affirmer Announce ou lecteur d’écran livré.

Préparer un changement sémantique de contenu par nouveau message ; une annonce live native est une dépendance T068 explicite. Pause focus aide le clavier même en l’absence de pont.

Aucun focus forcé, IME ni textbox. Dédoublonner sémantiquement le message déjà affiché après un repaint ; la nouvelle génération reçoit une seule publication de structure.

## 8. Cycle de vie et récupération

UI/main-thread ; timers et overlay protégés par owner/génération, cancel au dismiss/remplacement/démontage. Destructor retire ses entrées/overlay et reste no-throw sans appeler action.

show prépare allocation/expiration avant publication ; si scheduler refuse ou lève, aucun toast immortel : rollback exact et Unavailable ou propagation après invariants restaurés. Pas de fallback synchrone pour action/expiration.

Action snapshot déplacé hors entrée puis retrait logique terminal avant code utilisateur ; callback commencé jamais rejoué après exception. Action peut show un nouveau message sans stale erase ; top-level destruction différée au checkpoint sûr.

## 9. Dépendances et cas limites

Dépend de Overlay/Dispatcher/Button/TextService. Réutiliser OverlayState et invalidation existants ; aucune seconde pile « notifications » indépendante du routage/focus.

Message vide = InvalidSpec. Durée nulle/négative = InvalidSpec. Dispatcher invalide/owner UI closing = Unavailable. Handles d’un autre contrôleur, stale ou déjà dismiss = false.

Vue désactivée/cachée : retirer stack visuelle et timers, abandon silencieux des messages de ce controller ; lors de réouverture aucun vieux message/actions ressuscités. Panneaux trop nombreux sont bornés/clippés, aucune queue non bornée implicite : capacité v1 32, 33e rejette Unavailable sans éviction d’une action acceptée.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/toast.hpp` et `src/toast.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Overlay/Dispatcher existants, aucune implémentation Toast source. ToastSpec/ToastHandle/show statuses restent dans toast.hpp ; vrai controller/stack/expiry dans toast.cpp. L’extension additive de placement reste dans le service Overlay existant et est testée sans changer ses autres politiques.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `toast_durations` : 4s/8s et explicite expirent sous horloge manuelle.
- `toast_pause_focus` : hover/focus gèlent durée puis reprennent restant exact.
- `toast_dedup_stale` : même message remplace génération et ancien handle ne touche pas nouveau.
- `toast_action_once` : click/keyboard puis callback réentrant ou qui lève ne rejouent pas action.
- `toast_bottom_hit` : resize/place bas-centre et input hors stack restent corrects.
- `toast_timer_reject` : queue pleine/exception avant enqueue ne laisse pas toast immortel.
- `toast_isolation_capacity` : deux UI, contrôleurs, capacité32 et teardown sont isolés.

Créer `examples/features/toast.cpp` et la cible `nativeui_example_toast`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
