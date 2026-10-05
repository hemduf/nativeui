# Autocomplete

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer du texte libre avec suggestions facultatives ; une saisie sans suggestion demeure une valeur
valide. Cela diffère de la sélection fermée EditableComboBox.

NativeUI a TextInput et popup, mais aucun Autocomplete. Le nouvel éditeur réutilise le moteur de
suggestions privé sans dupliquer Overlay/Focus.

MyGo : `ui/combobox.go`, `Autocomplete`, `comboboxBase`, `matching`. Présente suggestions contenant
la frappe, prefixes d’abord, exclut équivalent complet et ne présélectionne pas au premier
caractère.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class Autocomplete {
public:
  using SuggestionsProvider = std::function<std::vector<std::string>()>;
  using Filter = std::function<bool(std::string_view candidate, std::string_view query)>;
  Autocomplete(std::string label, Binding<std::string> value,
               std::vector<std::string> suggestions);
  Autocomplete(std::string label, State<std::string>& value,
               std::vector<std::string> suggestions);
  Autocomplete(std::string label, Binding<std::string> value, SuggestionsProvider suggestions);
  Autocomplete(std::string label, State<std::string>& value, SuggestionsProvider suggestions);
  Autocomplete&& filter(Filter value) &&;
  Autocomplete&& placeholder(std::string value) &&;
  Autocomplete&& on_submit(std::function<void(const std::string&)> callback) &&;
  Autocomplete&& style(AutocompleteStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<std::string> city{""};
auto city_input = ui::Autocomplete("Ville", city,
    std::vector<std::string>{"Paris", "Pau", "Lyon"})
    .on_submit([](const std::string&) {}).spec();
```

Filter par défaut et fournisseur reprennent [EditableComboBox](editable_combo_box.md) : ASCII
casefold, préfixes puis substring stables ; Unicode exact au défaut, politique étendue injectée.

AutocompleteStyle cible : TextInputStyle et MenuItemStyle, metrics popup ; la string freeform du
Binding n’est pas contrainte à une suggestion.

## 3. État, propriété et notifications

Binding<string> est texte live : toutes éditions valides du moteur texte publient, avec ou sans
correspondance. Suggested value chosen est une écriture normale du même Binding.

Highlighted initial absent après frappe ; une flèche choisit une cible de preview sans set. Prendre
une suggestion copie tout son texte, met caret à la fin et ferme popup.

Écriture externe modifie l’éditeur, ferme anciennes suggestions et ne produit pas submit. Une valeur
identique issue de son propre commit conserve caret.

Escape annule popup seulement et garde texte live, sans revenir à un choix antérieur. Une seconde
Escape popup fermée suit la politique de TextInput snapshot.

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

Frappe query non blanc ouvre matches, en excluant suggestion égale au texte selon comparaison défaut
; query vide/blanc ne montre aucun panneau.

Bas/Haut ouvrent/ciblent première/dernière puis naviguent sans bouclage. Enter avec surligné prend
suggestion sans on_submit ; Enter sans cible submit texte live. Clic suggestion valide au
relâchement.

Échap ferme preview ; Tab ferme et sort, sans choisir ; perte focus ferme. L’éditeur garde
caret/focus, contrairement au popup de sélection ComboBox.

Molette du panneau scroll ; champ fermé ne sélectionne rien. ReadOnly garde sélection/copie mais pas
suggestion ou commit ; Disabled suit disponibilité.

Composition Update ne sollicite pas provider ; Commit query relance une seule génération. Le pont
native preedit/candidate reste séparé.

## 5. Mesure et layout

Taille stable TextInputStyle ; popup largeur au moins le champ, hauteur jusqu’à huit lignes puis
viewport scroll. Le parent ne grandit pas avec la liste.

Suggestions longues clippées/ellipsées dans lignes ; paint et hit-test suivent la même hauteur
MenuItemStyle. Ancre logique suivie par service overlay.

Aucun panneau vide sélectionnable : aucune suggestion ou texte équivalent complet ferme le panneau.
Resize conserve la cible par texte stable si encore présent.

## 6. Présentation et invalidation

Focus ring sur champ ; highlighted sur ligne sans confondre la valeur déjà dans Binding. Placeholder
conservé pour empty.

Filtre calculé hors paint puis snapshot possédé publié. Changement query/list metrics remesure popup
; highlight couleurs paint seulement.

Suggestion provider peut être coûteux mais reste synchrone UI ; pas de debounce/network implicite.
Recherche distante demande app utilisant snapshots sûrs, pas cette v1.

## 7. Accessibilité

Cible : ComboBox éditable avec expanded, text_value et description de suggestions ; options nommées
ListItem. Actions SetValue/Select sont disponibles si mutables.

Highlighted est annoncé via identité backend-neutre du snapshot, jamais un Node*. Valeur libre sans
matches reste text_value valide, aucune erreur sémantique.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Filtrage/provider prépare snapshot complet, génération query liée. Exception conserve texte live,
ferme panel périmé et laisse caret utilisable.

Choix termine popup/capture avant Binding.set ; submit copie valeur/callback avant appel. Un
observateur ou submit levant n’est pas rejoué, modèle publié reste.

Retrait d’une suggestion ou State invalid annule cible de l’ancienne génération ; aucune réponse
clipboard/provider différée après destruction ne peut modifier le remplaçant.

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

Dépendances : [text_input](text_input.md), [editable_combo_box](editable_combo_box.md), noyau
suggestions, Overlay/Focus/State.

Dédupliquer suggestions exactes, garder première occurrence ; query blanc non suggéré mais valeur
conservée. Suggestions vides n’empêchent jamais submit libre.

Pas d’apprentissage global des choix ni d’historique entre instances ; options/string callbacks
appartiennent au composant ou modèle applicatif explicite.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/autocomplete.hpp` et `src/autocomplete.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

AutocompleteStyle et provider/filter restent dans header du composant ; autocomplete.cpp porte
comportement freeform/submit, runtime, mise en page et peinture, partage seulement le moteur
suggestions privé.

Inscrire `src/autocomplete.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`autocomplete_freeform` : texte hors liste publie et submit normalement ; aucun retour forcé à
suggestion.

`autocomplete_highlight` : premier caractère ne présélectionne pas ; Down puis Enter prend une seule
suggestion.

`autocomplete_submit_choice` : Enter sur cible ne submit pas ; Enter sans cible submit texte exact.

`autocomplete_escape` : fermeture popup conserve texte live ; seconde Escape suit baseline
TextInput.

`autocomplete_filter_empty` : vide/blanc/equivalent ferme panneau, dédup et order stables.

`autocomplete_external_failure` : external write/provider/filter failure ferment périmé sans perdre
texte.

`autocomplete_remove_throw` : suggestion retirée/callback retire ou lève : aucune mutation périmée.

Ajouter `examples/features/autocomplete.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
