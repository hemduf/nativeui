# SearchField

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Champ de recherche avec loupe, texte et effacement, sans moteur de recherche incorporé. Le Binding
transporte la requête pour filtrage applicatif.

NativeUI fournit TextInput et primitives d’icône, mais aucun SearchField. La loupe et le bouton
clear restent des sous-parties du même composant.

MyGo : `ui/combobox.go`, `SearchField`, `magnifier`. Effacer par bouton ou Escape, Enter Submitted,
placeholder “Search” et maintien du focus sont portés en API retenue.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class SearchField {
public:
  SearchField(std::string label, Binding<std::string> query);
  SearchField(std::string label, State<std::string>& query);
  SearchField&& placeholder(std::string value) &&;
  SearchField&& max_length(std::size_t value) &&;
  SearchField&& on_submit(std::function<void(const std::string&)> callback) &&;
  SearchField&& style(SearchFieldStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<std::string> query{""};
auto search = ui::SearchField("Chercher dans les presets", query)
    .placeholder("Rechercher").on_submit([](const std::string&) {}).spec();
```

Defaults : placeholder “Rechercher”, max_length=0 illimité. SearchFieldStyle : TextInputStyle,
taille/gap loupe, clear ButtonStyle et métriques internes ; aucun slot Theme nouveau supposé.

Query changes sont les notifications Binding ; on_submit reçoit copie de la requête au Enter. Aucun
debounce, requête réseau ou scheduler applicatif caché.

## 3. État, propriété et notifications

Binding<string> est la requête live ; caret/selection/undo locaux au noyau TextInput. Une écriture
externe remplace texte selon contrat sûr d’éditeur sans provoquer submit.

Le clear met query à vide une fois et remet caret/history de la requête effacée ; il ne publie ni
search result ni rollback de filtre.

La baseline Escape de TextInput est remplacée par la politique SearchField : Escape sur query non
vide efface, au lieu de restaurer un snapshot historique. Ce remplacement est limité au nouveau
composant.

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

Clic dans champ donne caret/focus, éditions et clipboard suivent TextInput. Bouton clear apparaît si
query non vide ; clic le déclenche puis maintient focus dans l’éditeur.

Enter submit la query courante sans transformation ; Escape query non vide clear ; Escape query vide
remonte pour Dialog/FindBar parent. Pendant composition, la première annulation composition ne clear
pas la query.

Clear n’est pas un arrêt Tab supplémentaire ; la commande sémantique d’effacement reste accessible.
Loupe non interactive. Molette remonte au parent.

ReadOnly bloque frappe/clear, garde sélection/copie. Disabled bloque focus/action ; PointerCancel
clear armé ne publie rien.

## 5. Mesure et layout

Champ en Row de hauteur TextInput : loupe fixe, editor flex, zone clear de largeur réservée même
vide afin de ne pas déplacer le texte.

La largeur préférée inclut padding, loupe/gaps, zone editor et clear. Clipping de texte ne doit pas
atteindre loupe/bouton ; caret visible via scroll horizontal du noyau.

Coordonnées logiques et scale via paint ; longue requête ne fait pas croître tout le formulaire.

## 6. Présentation et invalidation

Loupe couleur muted, clear en affordance secondaire au hover ; Focus ring autour du champ complet.
Placeholder distinct de query empty.

Query non vide->vide repaint visibilité clear et content ; largeur réservée évite layout. Style
metrics de loupe/gap/font demandent layout.

Pas d’animation permanente ni de spinner de recherche implicite. L’application peut ajouter Spinner
à côté via composition.

## 7. Accessibilité

SemanticRole n’a pas SearchField : utiliser TextInput avec nom descriptif et text_value, actions
Focus/SetValue. Le clear est un Button nommé “Effacer la recherche”, avec action Activate et identité retenue interne au composant.

La loupe et icônes décoratives Role None. Le résultat et son count sont exposés par la vue qui
effectue la recherche, pas inventés par le champ.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Copier query/callback après fin d’édition et invalidation avant submit ; une action peut retirer le
SearchField sans accès postérieur.

Clear termine capture, prépare la valeur vide et met le caret cohérent avant Binding.set. Exception
d’observer laisse query vide et composant récupérable.

Clipboard différé utilise token/génération de l’éditeur ; query invalid, composant retiré ou
ReadOnly ignore sa réponse. Destruction ne clear pas le Binding.

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

Dépendances : [text_input](text_input.md), [button](button.md), primitives icône et State.
[find_bar](find_bar.md) ajoute count/navigation, pas SearchField.

Query vide : Enter peut submit vide, clear absent, Escape remonte. Espaces sont conservés exactement
; aucune normalisation trim/case imposée au moteur applicatif.

IME : committed Unicode et composition headless hérités ; couverture native preedit/candidate
séparée. Externe query change pendant clear annule l’ancienne action au checkpoint si identité
remplacée.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/search_field.hpp` et `src/search_field.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

SearchFieldStyle et sous-parties clear/loupe dans le même couple ; search_field.cpp porte routage
spécialisé, layout et composition du noyau texte.

Inscrire `src/search_field.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`search_field_live_query` : édition publie requête exacte, espaces/multioctets conservés.

`search_field_clear_focus` : clear/Escape effacent une fois et maintiennent caret ; aucun arrêt Tab
de plus.

`search_field_escape_empty` : Escape vide remonte ; preedit cancel précède clear.

`search_field_submit` : Enter passe un snapshot exact y compris vide, pas de debounce caché.

`search_field_layout` : zone clear réservée sans déplacement caret ; clipping/scales corrects.

`search_field_throw_stale` : observer/submit lève/retire et stale clipboard n’altère aucun nouveau
champ.

Ajouter `examples/features/search_field.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
