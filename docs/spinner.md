# Spinner

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Indicateur circulaire d’activité de durée inconnue, sans pourcentage et sans interaction. Utile au voisinage d’un Label de statut ou d’un bouton occupé.

Absent du toolkit ; primitives arc/path et [animation.hpp](../include/nativeui/animation.hpp) existent, avec AnimationContext et Dispatcher qui calculent les animations sur horloge injectée.

MyGo : `ui/indicators.go`, `Spinner`, douze rayons avec cycle900ms et RoleProgress inconnu. Cible reprend ce rythme et l’animation interrompable, sans appeler AnimationFrame dans une boucle Go importée.

## 2. API publique et composition

API cible proposée :

```cpp
class Spinner {
public:
    explicit Spinner(std::string label = {});
    Spinner&& active(Binding<bool> value) &&;
    Spinner&& active(State<bool>& value) &&;
    Spinner&& size(double value) &&;
    Spinner&& reduced_motion(bool value = true) &&;
    Spinner&& style(SpinnerStyle value) &&;
    Spec spec() &&;
};
```

Défauts : active=true si Binding absent, taille18 DIP, cycle900ms, douze rayons. SpinnerStyle contient color/thickness_ratio et taille optionnelle ; `.size` explicite prime sur taille de style. Nombres nouveaux double.

Exemple cible proposé :

```cpp
ui::State<bool> working{true};
auto busy = ui::Spinner{"Chargement"}
    .active(working)
    .size(18.0)
    .spec();
```

## 3. État, propriété et notifications

Label/style/options possédés ; Binding<bool> optionnel actif est observé RAII, surcharge State convertie immédiatement. Phase et handle d’animation sont propres à l’instance.

Si Binding source disparaît : invalid=false, get dernière valeur mais activité considérée inactive à la prochaine revalidation ; aucun callback ni notification destruction automatique. Ne pas écrire false dans modèle.

active=false conserve mesure mais ne dessine pas de rayons ; phase repart de zéro lors d’une nouvelle activation. Aucun on_change/finished ni lien automatique avec un traitement métier.

## 4. Interactions

Display-only : pointeur, glissement, molette, clavier, texte et drops Ignored ; non focusable, aucune capture ni validation/annulation.

Disabled n’autorise aucune action ; applique couleur désactivée et suspend animation tout en gardant un pictogramme statique si actif. ReadOnly ne change pas le sens de busy.

Quand placé dans Button, le parent gère l’activation ; Spinner n’a aucune zone interactive invisible ni commande d’arrêt.

## 5. Mesure et layout

Préféré/minimum : carré taille choisie ; place plus large centre le spinner sans l’étirer. Diamètre réel = min(largeur,hauteur,taille demandée).

Rayons aux fractions0,22..0,46 du diamètre, épaisseur0,09 par défaut, bornés à la place. Taille0 produit surface vide sans divisions/wake.

Dimensions logiques ; pas de round de diamètres au framebuffer dans le composant. Padding est responsabilité de la composition, aucun espace externe caché.

## 6. Présentation et invalidation

Couleur par défaut = muted_text du Theme ; SpinnerStyle override possédé. Douze rayons, fading de lead jusqu’à15% opacity, pas d’opacité du parent réécrite.

Cycle à tween linéaire0→1 avec AnimationContext existant, rearm protégé par token/génération. Phase animée = paint only ; ni layout ni annonce sémantique toutes les16ms.

Reduced_motion explicite ou timing absent = dessin statique à phase0 pour actif. Caché/collapsé/démonté cesse tout wake ; retour visible redémarre zéro. Aucune préférence OS réputée livrée dans cette option.

## 7. Accessibilité

Contrat cible : ProgressBar avec nom label, aucune numeric_value/range pour une activité inconnue et aucune action. Inactif est omis comme indicateur d’activité plutôt que annoncé terminé.

Spinner décoratif d’un Button peut être exclu par composition, Button décrit son état applicatif séparément. Ne pas doubler lecture du Label voisin.

Rôle/hooks disponibles, publication cible testable headless ; ponts T068 différés. Pas d’IME.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement et contexte/handle d’animation RAII. Montage valide d’abord la taille/options, puis acquiert timing et token ; teardown annule avant de libérer paint target.

Échec timer/tween armement retire entrée provisoire et garde phase statique. Une future transition inactive→active peut réessayer ; aucun busy guard empoisonné.

Completion interne de cycle vérifie vie/génération avant rearm ; callback stale après Hidden/démonté no-op, jamais joué synchroniquement si enqueue échoue. Destruction no-throw et callback-silent.

## 9. Dépendances et cas limites

Réutilise AnimationContext/DispatcherProvider et Theme. Pas de service timer concurrent, thread ou horloge murale commune ; clock de l’animation existante assure tests manuels.

size non fini ou négatif = invalid_argument ; zéro valide. thickness_ratio non fini/non positif ou >0,5 = invalid_argument avant publication. Label vide autorisé pour décoration.

Active change pendant cycle annule exactement ce cycle et ses futures reprises. Deux Spinner se partagent éventuellement Dispatcher owner mais pas phases/options/tokens.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/spinner.hpp` et `src/spinner.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : AnimationContext/Painter/Theme existants. SpinnerStyle reste dans le parent ; noyau mesure/rayons/subscription/cycle dans spinner.cpp, sans inline scheduler ou nouvelle API native.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `spinner_cycle` : douze rayons et cycle900ms suivent horloge manuelle.
- `spinner_active_lifetime` : active false et destruction source suspendent au prochain accès sans writeback.
- `spinner_reduced_static` : reduced motion et dispatcher indisponible donnent phase statique.
- `spinner_zero_hidden` : taille zéro/Hidden/Collapsed arrêtent wake et conservent place prévue.
- `spinner_timer_fault` : armement refusé ou exception permet prochaine transition.
- `spinner_multi_instance` : phases/couleurs/timers teardown ne touchent pas une autre UI.

Créer `examples/features/spinner.cpp` et la cible `nativeui_example_spinner`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
