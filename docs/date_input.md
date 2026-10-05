# DateInput

**Statut : nouveau à implémenter.**

[Catalogue des composants](widgets.md)

Référence NativeUI : `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; référence MyGo : `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Champ compact de date civile optionnelle affichant ISO YYYY-MM-DD et ouvrant un Calendar. Ce premier port suit le sélecteur MyGo ; ce n’est pas un éditeur libre de date.

Absent du toolkit NativeUI. [Overlay](../include/nativeui/overlay.hpp), focus et boutons existent ; [Calendar](calendar.md) et [Popover](popover.md) sont des composants cibles.

MyGo : `ui/date.go`, `DateInput` et `calendarGrid` en mode moveChooses=false. Clic/Enter/Space ouvrent, navigation change le cursor, clic/Enter choisissent et rendent focus au champ. MyGo conserve heure/fuseau ; la cible ne les transporte pas.

## 2. API publique et composition

API cible proposée :

```cpp
class DateInput {
public:
    using Value = std::optional<std::chrono::sys_days>;
    DateInput(std::string label, Binding<Value> value);
    DateInput(std::string label, State<Value>& value);
    DateInput&& range(Value minimum, Value maximum) &&;
    DateInput&& reference_day(std::chrono::sys_days value) &&;
    DateInput&& placeholder(std::string value) &&;
    DateInput&& clearable(bool value = true) &&;
    DateInput&& on_change(std::function<void(Value)> callback) &&;
    DateInput&& style(DateInputStyle value) &&;
    Spec spec() &&;
};
```

Défauts : absence autorisée, placeholder « Choisir une date », clearable=true, bornes ouvertes, reference_day=sys_days{}. DateInputStyle couvre chrome, text et largeur préférée ; CalendarStyle hérité en configuration interne cohérente.

Exemple cible proposé : `ui::DateInput{"Échéance", deadline}.clearable().spec()` ; deadline est un State<Value> converti en Binding à la construction ; sa destruction ultérieure conserve le dernier snapshot sans mutation possible.

## 3. État, propriété et notifications

Value externe via Binding ; surcharge State convertie immédiatement. Source détruite : Binding invalide, get dernière valeur, set ignoré/observe inactif sans notification automatique. Revalider avant edit/open et fermer popup au prochain checkpoint d’accès si indisponible, sans on_change. Open, cursor et handle d’overlay sont privés. À l’ouverture, copier valeur valide ou reference_day borné dans le cursor, sans écrire Value.

Choisir valide appelle Binding.set puis on_change une fois si valeur acceptée diffère. Clear choisit nullopt par le même chemin. Mise à jour externe ouverte remplace sélection/cursor sans on_change.

La fermeture sans choix conserve la valeur externe actuelle. Éviter un second State public pour open ; le parent réutilise seulement le handle Overlay lifetime-safe.

## 4. Interactions

Trigger : clic primaire terminé, Enter et Space ouvrent/ferment une seule fois par pression ; Tab entre dans le parcours normal. Le Calendar reçoit le focus au cursor à l’ouverture.

Popup : flèches/Home/End/PageUp/PageDown déplacent brouillon, Enter/Space ou clic commit et ferment. Escape et clic extérieur ferment sans commit. Clear est une vraie action nommée et désactivée si valeur absente/ReadOnly.

ReadOnly ne permet ni ouverture mutatrice ni clear ; la valeur reste lisible. PointerCancel annule un trigger pressé. Molette ignorée ; aucun parsing de TextInput dans cette v1.

## 5. Mesure et layout

Mesure du trigger selon texte ISO ou placeholder, icône et clear, largeur préférée style. La grille conserve sa largeur naturelle : ne pas forcer celle du champ lorsqu’il est très large.

Popup ancré par NodeId, placement Auto du service Overlay, bornes viewport. Header et grille Calendar bornent leur contenu ; parent ne clone pas la politique de placement.

Resize/DPR/scroll de parent réévalue ancre. Ancre retirée ou non disponible ferme ; pas de fallback centré d’un date picker orphelin.

## 6. Présentation et invalidation

DateInputStyle nouveau : surface/border/text/placeholder/icon/focus et métriques. Rendu de date avec chiffres tabulaires si la police le permet, sans API de font-features inexistante dans TextStyle.

Binding change = texte + paint/semantics ; passage placeholder/date pouvant modifier taille = layout. Popup open = invalidation structure Overlay.

Label applicatif sert au nom, pas nécessairement au texte du bouton. Aucune animation nécessaire. Icône dessinée via les primitives/IconView, sans chemin de ressource codé en dur.

## 7. Accessibilité

Contrat cible : Button avec nom label, text_value ISO ou absence, expanded selon popup, Activate/Focus. Pas de Role::DateInput annoncé comme existant.

Le Calendar expose cellules/boutons ; le focus revient à l’ancre encore valide après fermeture. Action Clear nommée « Effacer la date ». Valeur invalide expliquée en description sans mentir sur le modèle.

Ponts T068 différés. Pas de saisie texte ni preedit à cette étape ; ajout futur d’édition textuelle dépend de DESIGN17.4 et ne change pas les choix chrono.

## 8. Cycle de vie et récupération

UI/main-thread ; abonnement RAII, open handle et callbacks de popup protégés par token/génération. Fermeture idempotente et démontage enlèvent uniquement l’overlay de cette instance.

Préparer contenu avant ouverture ; échec de création/scheduling garde le champ fermé et re-tentable. Pas de capture/focus restauré vers un NodeId stale.

Action commit/clear terminale avant on_change ; callback qui ouvre un autre popup ne referme pas ce nouveau popup. Exception ne rejoue jamais la notification ; destruction no-throw, top-level UI différée.

## 9. Dépendances et cas limites

Dépend de [Calendar](calendar.md), [Popover](popover.md), Button, chrono et Binding. Navigation PageUp/PageDown exige les ajouts Key et normalisation backend décrits par Calendar ; rien de spécifique Pugl dans le widget. Même règles de domaine 1..9999 et bornes inclusives que Calendar.

Date absente utilise placeholder ; externe hors domaine/plage affichée invalide, pas corrigée. Ouverture choisit une référence bornée, pas le jour courant caché.

Bornes incohérentes rejetées avant montage. Retrait du modèle pendant popup sûr pour Binding ; le contenu devient non mutateur dès revalidation, sans notification de changement fictive. Mise à jour externe au dernier moment a priorité sur un brouillon stale.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/date_input.hpp` et `src/date_input.cpp`. Le header contient les déclarations publiques ; le `.cpp` contient un véritable noyau retenu, la mesure, le layout, les événements applicables et le rendu.

Origine à extraire ou réutiliser : Button/Overlay et futur Calendar. Type Value identique à Calendar ; aucun wrapper de date natif, parsing locale implicite ou nouvelle pile popup.

Les adaptateurs templates indispensables restent dans le header et délèguent au noyau non template. Préserver les includes historiques via leurs headers collectifs ; ne pas laisser une seconde implémentation dans les `.inc`.

Inscrire le futur `.cpp` dans `NativeUI::Core`, sans fichier vide ni switch central de widgets. Aucun type Pugl, Skia, OS ou SDK de plugin dans cette API.

Cette livraison est documentaire : aucune extraction ni modification de CMake n’est effectuée.

## 11. Tests et critères d’acceptation

Tests à réaliser lors de l’implémentation :

- `date_input_open_cancel` : navigation puis Escape/outside ne modifie pas la date.
- `date_input_choose_clear` : choix/effacement écrivent une fois et rendent focus à l’ancre.
- `date_input_external_open` : externe ouverte remplace le brouillon sans on_change.
- `date_input_anchor_remove` : retrait/Hidden/Disabled pendant popup retire tous les handles.
- `date_input_invalid_date` : absence et valeur hors domaine/plage restent lisibles sans correction.
- `date_input_reentrant_popup` : callback qui ouvre autre chose ou lève garde le service utilisable.

Créer `examples/features/date_input.cpp` et la cible `nativeui_example_date_input`, liés à `NativeUI::Core`. Le mode `--self-test` utilise des événements et une horloge déterministes, fonctionne sans écran et retourne un code non nul au premier échec.

Vérifier compilation du header seul, composition publique, rendu headless et coexistence de deux UI indépendantes. Couvrir les reprises après les fautes décrites ci-dessus sous ASan/UBSan lorsque la durée de vie est concernée.

Acceptation : les tests nommés passent, aucune capture/inscription ne subsiste après démontage, et l’API publiée correspond à ces contrats. Vérification effectuée ici : lecture des déclarations et sources ; aucun test C++ ni test interactif exécuté.
