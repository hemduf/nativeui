# Popover

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Décorateur d’une ancre contenant un panneau composé, contrôlé par un Binding<bool> d’ouverture. Non modal par défaut, focus géré explicitement ; sert DateInput/ColorWell/autres panneaux courts.

Pas de Popover public dans NativeUI. [OverlaySpec](../include/nativeui/overlay.hpp) et les services retained d’overlay fournissent ancre NodeId, Auto/flip/clamp, Escape/outside et lifetime ; ils sont l’unique stack.

MyGo : `ui/widgets.go`, `Popover` et `stylePanel` ; `ui/base.go`, `PopoverBase`. Panneau au-dessous, largeur au moins ancre, fermeture outside/Escape. Les sous-composants MyGo utilisent PopoverBase quand largeur indépendante souhaitée.

## 2. API publique et composition

API cible proposée :

```cpp
class Popover {
public:
    template<class Anchor, class Content>
    Popover(Binding<bool> open, Anchor&& anchor, Content&& content);
    template<class Anchor, class Content>
    Popover(State<bool>& open, Anchor&& anchor, Content&& content);
    Popover&& placement(OverlayPlacement value) &&;
    Popover&& match_anchor_width(bool value = true) &&;
    Popover&& focus_on_open(bool value = true) &&;
    Popover&& on_close(std::function<void()> callback) &&;
    Popover&& style(PopoverStyle value) &&;
    Spec spec() &&;
};
```

Défauts : placement Auto, match_anchor_width=true, focus_on_open=false, fermeture Escape/outside activée. PopoverStyle décrit surface/border/radius/padding/max_size ; types templates convertissent enfants en Spec immédiatement, noyau non template.

Exemple cible proposé :

```cpp
ui::State<bool> open{false};
auto view = ui::Popover{open,
    ui::Button{"Options", [&] { open.set(!open.get()); }},
    ui::Label{"Paramètres avancés"}}
    .focus_on_open().spec();
```

## 3. État, propriété et notifications

Binding<bool> externe possède intention open ; surcharge State convertie immédiatement. Source détruite : valid=false et dernière valeur lisible, set ignoré/observe inactive sans notification automatique ; revalider avant accès/entrée et fermer silencieusement à ce checkpoint. Composant possède Spec d’ancre/contenu et un handle d’overlay. Aucun pointeur vers l’Element Go ou nœud retenu stocké ; seulement NodeId lifetime-safe.

Publication réussie suit open=true ; fermeture utilisateur écrit false puis on_close une seule fois. Écriture externe false ferme sans on_close utilisateur. Si show indisponible/échoue, rétablir open=false sans callback et permettre prochaine ouverture.

True reçu alors que ancre Hidden/Disabled/non montée est refusé puis rendu false. Contenu a une instance par période ouverte ; chaque reopen reconstruit son état local sans dupliquer l’ancre.

## 4. Interactions

Trigger n’est pas inventé : Anchor choisit ses propres événements. Exemple Button bascule open. Décorateur n’intercepte pas un autre clic d’ancre ni double action.

Escape ferme l’overlay supérieur concerné si les descendants l’ont ignoré. Clic extérieur ferme et est consommé par la politique actuelle pour éviter click-through ; panneau route ses contrôles normalement.

focus_on_open=true entre au premier descendant disponible, sinon le focus reste sur ancre. À la fermeture, restaurer focus antérieur/ancre seulement s’ils sont disponibles. Nonmodal ne piège pas Tab ; navigation hors panneau ne ferme pas automatiquement.

## 5. Mesure et layout

Décorateur mesure/layout uniquement Anchor, sans réserver de place au contenu ouvert. Panneau mesuré dans le viewport, padding du style, maximum_size borné.

Match anchor width donne minimum de largeur égale à l’ancre, bornée au viewport ; false conserve natural size. Flip/clamp/placement sont délégués entièrement à Overlay.

Redimensionnement/scroll suivent l’ancre ; retrait/Hidden/Collapsed/Disabled ferme le panneau, pas de fallback centré. Les contenus trop grands doivent explicitement composer ScrollView dans leurs bounds.

## 6. Présentation et invalidation

PopoverStyle nouveau et defaults palette/spacing existants. Pas de fond modal ni ombre native OS ; Painter portable pour chrome.

open/close = invalidation structure Overlay ; contenu Binding suit ses propres invalidateurs ; métriques de style/ancre = layout. Ne pas remonter l’ancre lors d’un repaint du popup.

Pas d’animation imposée. Les états de trigger n’altèrent pas la surface sans option explicite. Une couleur transparente conserve le hit area du panneau.

## 7. Accessibilité

Contrat cible : ancre conserve son rôle et reçoit expanded/open ; panneau Group nommé selon contenu ou nom du trigger. Pas de Role::Popover déclaré comme existant.

Le focus et les actions restent aux descendants. L’arbre expose un seul contenu ouvert ; la structure du panneau n’est pas copiée dans la description de l’ancre.

Ponts T068 différés. Saisie éventuelle du contenu conserve limite committed/preedit DESIGN17.4, sans que Popover ajoute des services IME.

## 8. Cycle de vie et récupération

UI/main-thread ; subscriptions RAII et handle weak à une génération précise. Démontage invalide callbacks d’abord et ferme son overlay sans callback applicatif.

Préparer Spec/chrome avant publication. Échec fermeture/invalidation garde handle pour retry exact ; une subscription réentrante open=true durant close crée une nouvelle génération seulement après clôture de l’ancienne.

on_close snapshot pris après transition terminale ; exception ne rejoue pas. La suppression de l’ancre pendant input passe par le checkpoint retenu ; top-level destruction différée. Aucun nouveau registry Tree/Focus/Overlay.

## 9. Dépendances et cas limites

Dépend des services Overlay/focus et Binding, pas de DialogState. Le modèle bool ne devient pas une state machine process-wide.

Anchor ou Content Spec sans factory = invalid_argument avant montage. Binding open invalid : dernière valeur reste lisible mais set est ignoré et observe inactive, sans notification de destruction automatique. Au prochain accès, fermer le panneau sans on_close utilisateur et ne pas prétendre avoir écrit false.

Reopen, external toggle rapide, retrait d’ancre et overlays imbriqués sont couverts. Tooltip reste pointer-transparent séparé ; il n’est pas reconverti en Popover interactif.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/popover.hpp` et `src/popover.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : OverlaySpec/OverlayHandle et service overlay existants. Les seuls templates sont conversion enfants et surcharge State ; fonctionnement/decorator/chrome/close dans popover.cpp. Ne pas ajouter un second gestionnaire de popup.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `popover_anchor_retained` : ouvrir/refermer ne remonte pas l’ancre.
- `popover_outside_escape` : fermeture écrit false et émet une seule notification utilisateur.
- `popover_width_placement` : match largeur/Auto/flip/clamp passent par Overlay.
- `popover_focus_policy` : focus opt-in et nonmodal sans trap ont le parcours fixé.
- `popover_stale_generation` : reopen pendant close ne ferme pas le nouveau panneau.
- `popover_publish_fault` : allocation/invalidation qui lève conserve un owner de reprise ou ferme sans ghost.

Créer `examples/features/popover.cpp` et la cible `nativeui_example_popover`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
