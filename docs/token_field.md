# TokenField

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Éditer une liste ordonnée de tags/destinataires sous forme de chips et d’un champ draft, avec
suggestions facultatives.

NativeUI possède TextInput, layouts et State<vector<T>>, mais aucun TokenField. Chips et boutons
retirer restent sous-parties d’un composant, pas une famille de fichiers supplémentaire.

MyGo : `ui/combobox.go`, `TokenField`. Enter/virgule ajoutent texte, Backspace draft vide retire
dernier, suggestions excluent tokens existants et l’input garde identité malgré chips précédentes.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class TokenField {
public:
  TokenField(std::string label, Binding<std::vector<std::string>> tokens,
             std::vector<std::string> suggestions = {});
  TokenField(std::string label, State<std::vector<std::string>>& tokens,
             std::vector<std::string> suggestions = {});
  TokenField&& allow_custom(bool value = true) &&;
  TokenField&& maximum_tokens(std::size_t value) &&;
  TokenField&& placeholder(std::string value) &&;
  TokenField&& style(TokenFieldStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<std::vector<std::string>> tags{std::vector<std::string>{"Audio"}};
auto field = ui::TokenField("Tags", tags, {"Audio", "Synthèse", "Effets"})
    .allow_custom().maximum_tokens(20).spec();
```

Defaults : allow_custom=true, maximum_tokens=0 illimité. Suggestions vector possédé ; modèle
distant/asynchrone hors v1, source reconstruite via Spec.

TokenFieldStyle cible : TextInputStyle, chip text/background/border/padding, remove ButtonStyle,
gaps et MenuItemStyle suggestions. Séparateur ajouté v1 = virgule ASCII, sans parsing CSV/quoting
implicite.

L’ordre des tokens est celui de vector. Aucun on_change supplémentaire : toutes opérations publient
un vector complet via Binding.

## 3. État, propriété et notifications

Binding<vector<string>> est la liste ; draft, popup generation et chip actif sont locaux. L’éditeur
est un enfant à identité fixe indépendante du nombre/ordre de tokens.

Ajouter trim ASCII, refuser vide et doublon exact ; conserver casing/Unicode. allow_custom=false
exige égalité exacte d’une suggestion ; maximum atteint refuse sans effacer draft.

Une opération de collage avec virgules prépare la nouvelle liste complète et publie une seule valeur
: segments complets ajoutés, segment final garde draft. Doublons/vides sont ignorés, tokens
dépassant maximum restent dans draft joint avec virgules.

Écriture externe des tokens ne détruit pas le draft, mais recalcule suggestions et annule une
suppression/choix armé d’ancienne génération. Valeurs externes dupliquées sont rendues sans
correction silencieuse, avec identité occurrence locale.

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

Enter sur suggestion ajoute celle-ci ; sinon ajoute draft si autorisé, puis remet draft vide en cas
d’ajout réussi ou de doublon déjà présent. Virgule commit segments complets ; défaut clipboard suit
TextInput single-line.

Backspace quand draft vide retire dernier token immédiatement ; Delete retire chip actif. Pour
accessibilité clavier, Left au début de draft peut activer dernier chip, puis Left/Right traversent
chips et draft ; Escape revient au draft sans mutation.

Bouton remove clique au relâchement, garde focus draft. Les boutons ne créent pas un arrêt Tab par
chip : TokenField un arrêt, navigation interne aux flèches.

Suggestions filtrent comme Autocomplete et excluent tokens existants ; Up/Down/Enter,
Tab/blur/Escape et PointerCancel suivent moteur suggestions. Molette uniquement scroll du panneau.

ReadOnly interdit ajout/retrait mais expose texte et liste ; disabled bloque actions. Composition
Update n’interprète pas une virgule préedit : découpage seulement Commit.

## 5. Mesure et layout

Layout wrapping de chips et éditeur, avec largeur minimale de draft ; quand l’espace manque,
renvoyer entier chip à la ligne suivante et augmenter hauteur du champ.

Chip trop long occupe largeur disponible, texte ellipsé, remove reste dans l’aire ; toutes positions
logiques. Le popup s’ancre au cadre entier, pas seulement à la dernière petite zone de texte.

Ajouter/retrait tokens invalide layout ; draft à taille minimale stable paint/scroll, ne remount pas
l’éditeur. Le parent scroll gère une liste très haute, sans virtualisation prétendue.

## 6. Présentation et invalidation

Chaque chip a face distincte et libellé ; chip actif porte ring/selection local, remove a affordance
hover. La valeur textuelle n’est pas l’identité d’un Node mutable.

Parent Focus ring inclut le champ ; forme interne du chip ne doit pas produire un second focus
simultané. Les données externes sont prises au snapshot du layout.

Pas d’animation continue ni drag-reorder en v1. Color/font/padding changent invalidation suivant
leurs métriques.

## 7. Accessibilité

Cible : Group nommé, éditeur TextInput/ComboBox et chips ListItem/Text avec boutons Remove “Retirer
<token>”. L’ordre sémantique suit la liste, avec aucune action mutante si ReadOnly.

Un rôle TokenField n’existe pas ; les chips restent dans le modèle backend-neutre. L’input unique
garde identité même si la liste devient vide.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Préparer vector nouvellement possédé, terminer geste/capture et état draft, puis Binding.set. Aucun
pointeur vers string d’un ancien vector gardé dans remove callback.

Une suppression d’un duplicat externe cible occurrence et génération ; si modèle change, annuler
l’action plutôt que retirer un token de même texte mais nouvelle position.

Exception d’observer laisse vector commis et éditeur utilisable ; draft à disposition au checkpoint,
aucun ajout automatique rejoué. Un mount de chip levant ne publie pas une liste partielle.

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

Dépendances : [text_input](text_input.md), [autocomplete](autocomplete.md), noyau privé de layout
wrapping dans token_field.cpp, Overlay et State.

Tokens externes empty/duplicate sont affichés fidèlement, retrait autorisé par identité occurrence ;
les ajouts utilisateur ne créent pas de nouveaux empty/duplicate. Maximum nouveau inférieur à taille
existante ne tronque pas le modèle.

Suggestions absentes et allow_custom=false rendent ajout indisponible mais retrait reste possible.
Trim/égalité sont explicités ; aucun normaliseur email/network caché.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/token_field.hpp` et `src/token_field.cpp`. Le header expose les
déclarations publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un
véritable noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

TokenFieldStyle, chip model private et options restent dans le couple ; le .cpp porte liste retenue
à keys, wrapping, draft, opérations de vector et popup via noyau suggestions partagé.

Inscrire `src/token_field.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`token_field_add_batch` : Enter/virgule/collage produisent tokens trim sans doublons et une seule
notification batch.

`token_field_limits` : maximum/custom restriction refusent sans perte draft ni truncation externe.

`token_field_remove_keyboard` : remove, Backspace vide, chip Left/Right/Delete et Escape corrects.

`token_field_input_identity` : ajout/retrait/réordre gardent même éditeur/caret ; suggestions
excluent tokens.

`token_field_external_duplicates` : snapshot duplicate render fidèle ; occurrence armée annulée
après modèle changé.

`token_field_composition` : virgule preedit n’ajoute rien ; Commit une opération.

`token_field_throw_recovery` : observer, chip mount ou callback retirant le champ gardent
liste/flags cohérents.

Ajouter `examples/features/token_field.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
