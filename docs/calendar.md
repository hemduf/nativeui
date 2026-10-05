# Calendar

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Calendrier grégorien mensuel sélectionnant une date civile optionnelle, sans heure ni fuseau. Réutilisable en ligne et dans DateInput.

Absent de NativeUI ; les fondations [State/Binding](../include/nativeui/state.hpp), focus et Component existent. Aucun service calendrier natif n’est importé.

MyGo : `ui/date.go`, `Calendar`, `calendarGrid`, `setDay` et `sameDay`. Affiche six semaines commençant lundi ; flèches changent la sélection inline, Home/End le mois et PageUp/PageDown le mois affiché. MyGo conserve heure/fuseau de time.Time ; la cible est volontairement date civile.

## 2. API publique et composition

API cible proposée :

```cpp
class Calendar {
public:
    using Value = std::optional<std::chrono::sys_days>;
    Calendar(std::string label, Binding<Value> selection);
    Calendar(std::string label, State<Value>& selection);
    Calendar&& range(Value minimum, Value maximum) &&;
    Calendar&& reference_day(std::chrono::sys_days value) &&;
    Calendar&& today(Value value) &&;
    Calendar&& commit_on_navigation(bool value = true) &&;
    Calendar&& on_change(std::function<void(Value)> callback) &&;
    Calendar&& style(CalendarStyle value) &&;
    Spec spec() &&;
};
```

Par défaut : lundi en tête, six lignes de sept jours, reference_day = sys_days{} (1970-01-01), today absent, bornes ouvertes et commit_on_navigation=true. CalendarStyle décrit tailles de cellules, textes, couleurs selected/cursor/today et espacements.

Exemple cible proposé :

```cpp
ui::State<ui::Calendar::Value> selected{std::nullopt};
auto picker = ui::Calendar{"Jour", selected}
    .reference_day(std::chrono::sys_days{
        std::chrono::year{2026}/10/4})
    .spec();
```

## 3. État, propriété et notifications

Le modèle selection est externe ; Binding copié, surcharge State convertie en Binding au constructeur. Source détruite : Binding invalide, dernière valeur encore lisible, set ignoré et observe inactif ; aucune notification automatique. Revalider valid() avant edit puis ne pas notifier on_change si mutation impossible. Cursor/mois affiché sont privés à l’instance.

on_change appartient aux commits utilisateur qui modifient la valeur effective ; une écriture externe met à jour cursor/mois et paint mais n’appelle pas ce callback. Aucune normalisation du modèle au montage.

commit_on_navigation=false conserve cursor séparé ; clic ou Enter commit le cursor. Une nouvelle valeur externe remplace le brouillon et son mois ; aucun mouvement ancien ne réécrit cette mise à jour.

## 4. Interactions

Un arrêt Tab sur la grille, flèches +/-1 et +/-7 jours ; Home/End = premier/dernier jour du mois, PageUp/PageDown = mois précédent/suivant avec clamp du jour au mois cible. Navigation sature aux bornes sélectionnables.

PointerDown/Up sur une cellule valide commit une fois ; cancellation/retrait avant Up ne choisit rien. Boutons mois accessibles au clavier ; Enter/Space choisissent le cursor en mode brouillon.

Pas de molette ni scroll implicite. ReadOnly autorise navigation de lecture mais pas commit. Escape abandonne une pression locale ; dans Popover le parent gère la fermeture. Désactivé ne prend pas focus.

## 5. Mesure et layout

Grille fixe sept colonnes et six semaines, sans sauts de hauteur selon le mois. Tous les jours hors mois visibles sont sélectionnables s’ils sont dans les bornes ; leurs clés = date sys_days, pas index de cellule.

Mesure = en-tête/navigation + ligne jours + six lignes + gaps/padding, en unités logiques. Une largeur étroite borne cellules et clippe, sans overlap avec les boutons du mois.

Au resize, rectangles de cellules et hit testing partagent le même layout. Contraintes non bornées utilisent tailles de style ; vue nulle ne permet aucun hit ni division par zéro.

## 6. Présentation et invalidation

CalendarStyle nouveau, défaut résolu depuis palette/typographie/espacements existants. État choisi, cursor, jour marqué today, jours hors mois et jours indisponibles sont visuellement distincts.

Date externe et déplacement cursor = paint/semantics ; changement mois = contenu et semantics ; métriques de style = layout + paint. Les transitions n’exigent aucune animation.

Marqueur today injecté sans effet sur sélection. Les textes sont français par défaut (mois/jours), version v1 fixe ; ne pas introduire un service global de locale.

## 7. Accessibilité

Contrat cible : Group nommé label, cellules Button nommées par date complète, selected et enabled selon choix/bornes. La grille conserve focus et expose son cursor par snapshot/description ; Role::Table n’existe pas actuellement.

Actions cellule Activate/Select et boutons mois Activate. Ne pas annoncer une action activeDescendant native déjà disponible ; l’identité logique par date reste testable.

Hooks backend-neutres disponibles ; ponts T068 différés. Aucun éditeur texte/IME dans Calendar.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement de sélection RAII et invalidateurs lifetime-safe. Au démontage, annuler pression/capture et abandonner le cursor local sans écriture.

Préparer cellules/layout/snapshot avant publication ; erreur de construction ne conserve pas la moitié d’un mois ni un abonnement orphelin.

Avant callback on_change, mutation logique terminale puis snapshot de callback/valeur. Réentrance qui retire le composant ou modifie selection ne provoque ni second commit ni replay après exception. Destruction no-throw ; top-level destruction différée.

## 9. Dépendances et cas limites

Dépend de Button, TextService, Binding, focus et chrono. Key::PageUp/PageDown sont absents de input.hpp étudié : extension additive en fin d’enum requise, valeurs historiques conservées, normalisation Pugl/plateforme et tests dans la frontière backend src, jamais dans Calendar. [DateInput](date_input.md) compose ce noyau avec commit_on_navigation=false ; ne duplique pas les règles calendaires.

Bornes minimum > maximum = invalid_argument avant montage. Valeur externe hors bornes affichée comme indisponible ; cursor clampé, modèle intact. min==max donne un jour sélectionnable.

Année représentable : grégorien chrono year 1..9999 pour affichage v1 ; bornes/reference hors domaine sont rejetées, valeur externe hors domaine affiche absence invalide sans conversion overflow. Absence démarre sur reference ; aucune date système implicite.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/calendar.hpp` et `src/calendar.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : composition/focus/Binding existants et chrono C++20. Les types CalendarStyle/Value restent dans le couple ; le noyau de grille est réutilisé par DateInput sans template public de backend.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `calendar_leap_month` : février 2024/2100 et passages fin de mois suivent le calendrier.
- `calendar_cursor_commit` : mode inline et mode brouillon ont des notifications distinctes.
- `calendar_range_identity` : bornes égales/hors plage et clés de date restent cohérentes.
- `calendar_external_draft` : écriture externe remplace cursor sans callback utilisateur.
- `calendar_remove_cell` : changement mois/retrait pendant pression ne choisit pas une autre cellule.
- `calendar_throw_recover` : callback réentrant qui lève laisse un autre choix possible.

Créer `examples/features/calendar.cpp` et la cible `nativeui_example_calendar`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
