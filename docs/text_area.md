# TextArea

Statut : **existant à enrichir**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Saisir plusieurs lignes, garder curseur et sélection visible dans un viewport local. TextArea est un
éditeur de texte brut, pas RichText ni FindBar.

NativeUI : [widgets_text_area.inc](../include/nativeui/detail/widgets_text_area.inc),
TextAreaComponent public ; builder
[widgets_builders.inc](../include/nativeui/detail/widgets_builders.inc), TextEditModel multilignes
et TextAreaStyle.

MyGo : `ui/editor.go`, `TextArea`, `textInput` ; `ui/base.go`, `TextAreaBase`. L’édition multiline
est déjà portée ; enrichissement requis pour noyau séparé, sémantique et callbacks lifetime
robustes.

## 2. API publique et composition

API actuelle à conserver ; les déclarations suivantes sont dans `namespace ui`.

```cpp
TextArea(std::string label, Binding<std::string> state);
TextArea(std::string label, State<std::string>& state);
TextArea&& placeholder(std::string value) &&;
TextArea&& max_length(std::size_t value) &&;
TextArea&& style(TextAreaStyle value) &&;
Spec spec() &&;
```

Exemple utilisant l’API actuelle :

```cpp
ui::State<std::string> notes{"Première ligne\nSeconde ligne"};
auto editor = ui::TextArea("Notes", notes).placeholder("Écrire des notes")
    .max_length(4096).spec();
```

Default max_length=0, illimité ; contrairement à TextInput, aucun on_submit public actuellement.
Entrée insère une nouvelle ligne, sans callback de validation implicite.

Conserver TextAreaComponent public et TextAreaStyle existant. Ne pas ajouter wrapping automatique,
mise en forme Markdown ou syntax highlighting sous le nom d’extraction.

## 3. État, propriété et notifications

Binding<string> contient texte brut ; modèle local caret/anchor/history, cache de lignes et
scroll_x/y. Une modification utilisateur commit live une string complète.

Observation externe différente remplace modèle, rebuild_lines et remet scrolls/historique comme
source actuelle. Une valeur identique ne casse pas le caret.

focus_snapshot fixé à l’entrée, Escape peut restaurer ce snapshot avec une écriture Binding ; aucune
validation soumission par Enter. Préserver cette politique historique, y compris après remplacement
externe.

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

Clic/double/triple clic et drag sélection suivent le texte multilignes. Up/Down conservent la
colonne préférée ; Left/Right caractère/mot, Home/End début/fin de ligne, modifier primaire
début/fin document.

Enter insère LF ; insertion/paste normalisent CR ou CRLF vers LF, préservent LF/Tab et retirent
autres contrôles. Copy/Cut/Paste/SelectAll/Undo/Redo communs.

Shift étend sur plusieurs lignes ; Backspace/Delete traversent les séparateurs. Escape restaure
snapshot. ReadOnly garde sélection/copie et navigation, bloque modifications.

La source actuelle ignore PointerWheel. Cible d’enrichissement : molette fait défiler viewport local
vertical, Shift horizontal, sans modifier Binding/caret ; à une borne sans mouvement l’événement
remonte au parent. Cette extension est documentée distinctement de l’extraction.

Composition headless Start/Update/Commit/Cancel et déduplication du commit sont présents ;
intégration native preedit/candidate reste un contrat plateforme à vérifier séparément, selon DESIGN
§17.4.

## 5. Mesure et layout

Mesure préférée fixe issue control_width/control_height de TextAreaStyle. Layout line-aware et
clipping du viewport local ; longues lignes défilent horizontalement, pas un soft-wrap implicite.

rebuild_lines produit lignes et offsets cohérents sur CRLF normalisé ; tenir le caret visible après
déplacement vertical, sélection et resize.

Texte, selection cross-line, caret et placeholder utilisent la même police TextService en
coordonnées logiques ; mesure/rendu malformed suivent réparation commune.

## 6. Présentation et invalidation

TextAreaStyle définit fonts, fill/border, selection/caret et composition underline. Fond et ring
communs, une seule cible focus pour le viewport.

Le cache de lignes est invalidé par modification texte ou métriques de police ;
valeur/caret/selection paint, dimensions/style métrique layout.

Tick caret uniquement quand focus/visible ; pas de boucle même si unfocused. Une erreur de paint
maintient le stack clip équilibré et laisse la dernière frame commise.

## 7. Accessibilité

Cible : TextArea, name=label, text_value texte brut, read_only/enabled et Focus/SetValue si permis.
La structure lignes n’est pas N champs textuels accessibles.

Aucun override semantics actuel dans le noyau étudié ; model de sélection et APIs natives text range
ne sont pas revendiqués comme réalisés.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Abonnement actuel capture this ; extraction utilise runtime/token détaché et garde l’observation
inopérante après retrait durant un passage.

Cache de lignes et texte sont préparés avant publication cohérente. Échec allocation/rebuild ne
publie pas un caret pointant dans un ancien buffer avec nouvelle string.

Clipboard différé porte génération/weak token ; après retrait, changement ReadOnly ou invalid
Binding, ignorer la réponse. Démonter termine drag, composition et text input sans commit
destructeur.

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

Dépendances : [text_input](text_input.md), TextEditModel, TextService, clipboard, ThemeBinding et
focus. RichText/FindBar sont des composants séparés.

Vide, ligne finale vide, ligne très longue, nombreuses lignes, emojis/malformed, maximum atteint sur
collage multiline et remplacement externe pendant drag doivent rester déterministes.

Pas de filesystem IO, édition de document système ni callbacks audio. L’application calcule la
recherche et l’enregistrement.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/text_area.hpp` et `src/text_area.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

Déclarations TextArea et TextAreaComponent restent publiques dans text_area.hpp ; text_area.cpp
porte modèle retenu, cache de lignes, scrolling, handlers et peinture.

Préserver text_area_style.hpp et les includes collectifs ; les helpers d’édition partagés restent
privés ou dans text_edit.hpp actuel sans dupliquer le moteur.

Inscrire `src/text_area.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`text_area_line_normalize` : CR/CRLF vers LF ; Tab préservé ; Enter insère LF et aucun submit.

`text_area_vertical_selection` : Up/Down conservent colonne, Home/End ligne/document et Shift
sélection cross-line.

`text_area_history_escape` : undo/redo et focus snapshot restaurent les textes attendus.

`text_area_viewport` : caret visible avec lignes longues/resize ; peinture selection équilibrée et
clippée.

`text_area_composition_clipboard` : synthetic composition sans double commit ; stale clipboard
rejeté.

`text_area_replace_throw` : observateur, rebuild et paint en échec récupèrent sans cache/texte
partiel.

`text_area_public_component` : anciennes signatures et TextAreaComponent direct restent compilables.

Ajouter `examples/features/text_area.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
