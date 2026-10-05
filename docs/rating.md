# Rating

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Attribuer une note à étoiles, de zéro jusqu’au nombre maximal. Une seule valeur numérique, un seul
arrêt Tab, aperçu au survol sans écriture.

NativeUI ne propose pas de Rating dans [widgets.hpp](../include/nativeui/widgets.hpp) ;
PaintContext/Path et sliders fournissent les primitives de rendu et valeur.

MyGo : `ui/indicators.go`, `Rating`, `starPath`. Note entière, recliquer la note sélectionnée remet
zéro ; cible conserve ce défaut et propose une granularité optionnelle en double.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class Rating {
public:
  Rating(std::string label, Binding<double> value, std::size_t stars = 5);
  Rating(std::string label, State<double>& value, std::size_t stars = 5);
  Rating&& step(double value) &&;
  Rating&& clearable(bool value = true) &&;
  Rating&& style(RatingStyle value) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
ui::State<double> score{3.0};
auto rating = ui::Rating("Note", score, 5).step(1.0).clearable().spec();
```

Defaults : cinq étoiles, step=1, clearable=true. Accepter uniquement step=1 ou step=0.5 pour une
géométrie de fraction définie ; max=stars est le nombre complet.

RatingStyle cible : taille/gap des étoiles, couleurs vide/pleine/preview/disabled, contour et ring.
Les étoiles sont des sous-parties paint du contrôle, pas des composants publics avec fichiers
propres.

## 3. État, propriété et notifications

Binding<double> conserve la note ; stars/step/style sont possédés. Valeur effective rendu
clamp[0,stars], NaN/inf=0, aucune réécriture silencieuse à mount.

La note utilisateur est quantifiée au pas choisi : un clic sur une étoile choisit son nombre entier
; avec step=0.5, la moitié gauche choisit i+0.5 et droite i+1.

hover_value est local et temporaire ; PointerUp publie seulement la valeur ciblée. clearable=true et
cible exactement égale à la note effective remettent zéro. Écritures externes ne remettent pas la
note en preview.

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

PointerMove affiche aperçu ; Down arme la cible et capture ; Up intérieur valide la cible courante ;
Cancel/sortie au relâchement retire l’aperçu sans mutation.

Droite/Haut augmente de step, Gauche/Bas diminue ; Home=0, End=stars. Tab un arrêt ; les formes
d’étoile internes ne demandent jamais le focus.

ReadOnly laisse lire/focus, sans preview promettant une mutation ni action. Disabled ne reçoit pas
l’édition. Molette ignorée ; aucun drag-continue à l’extérieur.

Échap annule un appui/preview sans rollback ; la dernière valeur publiée par une commande clavier
reste autoritative.

## 5. Mesure et layout

Largeur=stars*star_size + max(stars-1,0)*gap ; hauteur star_size plus ring. La zone hit de chaque
étoile est son rectangle, pas seulement son chemin opaque.

Une largeur plus grande que l’intrinsèque laisse les étoiles alignées au début ; un parent plus
étroit clippe sans redéfinir leur valeur en fonction du viewport.

stars=0 produit un indicateur vide non focusable et sans action. Taille/gap doivent être finite>=0 ;
toutes coordonnées sont logiques.

## 6. Présentation et invalidation

Chemin d’étoile réutilisable immuable, rempli selon valeur effective ; demi-étoiles obtenues par
clipping horizontal local équilibré.

Preview utilise couleur distincte et ne détruit pas la valeur checked dans le modèle. Focus ring
autour du groupe d’étoiles réellement dessiné.

Value/hover ne changent que paint. stars/taille/gap invalident layout. Aucun shader/raster requis
dans l’API publique ni allocation de chemin par étoile à chaque événement.

## 7. Accessibilité

Cible : Slider, name=label, numeric_value, range[0,stars] step, actions Increment/Decrement/SetValue
si mutable ; texte “3 sur 5” facultatif calculé dans snapshot UI.

Étoiles individuelles Role None : éviter N doublons de focus/actions. ReadOnly conserve valeur et
supprime actions ; stars=0 n’annonce pas un slider réglable.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Commit termine capture et efface preview avant Binding.set. Si un observateur retire le Rating,
aucun chemin/instance n’est relu ensuite.

Exception de callback/observer : valeur commise reste, preview annulé, prochaine interaction
disponible. Démonter n’appelle aucune action clear.

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

Dépendances : Binding, PaintContext/Path, focus et thème. Le nombre d’étoiles fait partie du Rating
; aucun composant Star séparé.

Step invalide rejette la Spec avant publication ; valeur externe hors plage rendue borne sans
correction. Un changement de stars après reconstruction ne remet pas State à zéro.

Préserver une palette lisible en thème sombre ; étoiles à taille nulle ne deviennent pas des cibles
d’édition.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/rating.hpp` et `src/rating.cpp`. Le header expose les déclarations
publiques et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable
noyau retenu, interactions, mesure et rendu, jamais un fichier vide.

RatingStyle et valeur d’aperçu interne sont dans le couple Rating ; rating.cpp porte
quantification/hit-testing, capture, geometry et star path partagé immutable.

Inscrire `src/rating.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`rating_click_clear` : clic choisit une note, second même choix remet zéro si clearable.

`rating_half` : step=0.5 distingue moitié gauche/droite ; step=1 ne produit que notes entières.

`rating_preview` : survol/Cancel ne publient aucune valeur ; reprise affiche modèle actuel.

`rating_keys` : flèches/Home/End respectent limites et pas, un seul Tab stop.

`rating_external_zero` : NaN/hors plage render sans write ; stars=0 non éditable.

`rating_remove_throw` : observer retire/ lève et clip half-star récupère ; prochain input/frame
valide.

Ajouter `examples/features/rating.cpp`, compilable par le consommateur public, avec mode
`--self-test` vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
