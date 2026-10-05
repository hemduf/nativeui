# Link

Statut : **nouveau à implémenter**.

[Catalogue des composants](widgets.md)

Sources étudiées : NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c` ; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`.

## 1. Objectif et état actuel

Présenter un texte qui déclenche une navigation applicative. Le composant n’effectue aucune
ouverture de navigateur ni interprétation automatique de route.

NativeUI a TextService et Button, mais aucun Link public dans
[widgets.hpp](../include/nativeui/widgets.hpp). Réutiliser les fondations de texte et d’activation
plutôt que porter le moteur MyGo.

MyGo : `ui/widgets.go`, fonction `Link` ; elle ouvre une URL ou pousse un chemin dans Router,
souligne au survol et s’intègre au texte enrichi. Cible NativeUI : destination possédée et callback
injecté ; l’application décide URL, route et validation.

## 2. API publique et composition

API cible proposée, non implémentée ; les déclarations suivantes sont dans `namespace ui`.

```cpp
class Link {
public:
  using NavigateCallback = std::function<void(const std::string&)>;
  Link(std::string label, std::string destination, NavigateCallback navigate);
  Link&& style(LinkStyle value) &&;
  Link&& wrap(bool value = true) &&;
  Spec spec() &&;
};
```

Exemple utilisant l’API cible proposée :

```cpp
auto help = ui::Link("Aide", "/aide", [](const std::string&) {}).spec();
```

LinkStyle cible contient TextStyle, couleurs normal/hovered/disabled, soulignement au repos et au
survol, métrique du focus. wrap(false) est la valeur par défaut ; aucun slot Theme nouveau n’est
présumé.

Pour une portée dans RichText, le moteur de runs doit consommer destination et action équivalentes ;
cette page ne demande pas qu’un composant retenu indépendant soit installé à chaque mot.

## 3. État, propriété et notifications

Texte, destination, callback et style sont copiés/déplacés dans la Spec. Survol, capture et focus
sont locaux ; aucune adresse de texte temporaire n’est conservée.

Changer de destination via reconstruction compatible met à jour le callback au checkpoint. Une
interaction déjà armée est annulée si son identité de lien est remplacée.

Le composant ne conserve pas d’historique et n’observe pas de route globale. Le callback reçoit un
instantané de destination, même si l’action reconstruit la page.

## 4. Interactions

Clic principal : activation au relâchement à l’intérieur après capture. Sortie ou PointerCancel
annulent ; la molette remonte au parent.

Tab peut donner le focus ; Entrée active au premier KeyDown, répétitions supprimées jusqu’à KeyUp.
Espace utilise l’activation au relâchement pour rester utilisable au clavier, sans défilement
concurrent.

Disabled rend l’action indisponible ; ReadOnly n’interdit pas une navigation qui ne modifie pas la
valeur du lien. Aucun menu URL, téléchargement, visite automatique ou gesture système n’est ajouté.

La sélection de texte appartient à RichText ; Link autonome constitue une zone d’action et ne
sélectionne pas son label au glissement.

## 5. Mesure et layout

Sans wrap, largeur intrinsèque du texte et hauteur de ligne, avec aire du ring. Avec wrap, mesurer
via TextService sous la largeur allouée, sans convertir les coordonnées logiques en pixels dans le
widget.

Un parent étroit clippe ou répartit en lignes selon wrap. Le hit-test suit la boîte allouée du
composant ; les fragments de RichText ont leur propre géométrie dans le moteur de runs.

Modification de police, de texte ou de largeur sous wrap invalide layout ; destination seule ne
change pas la géométrie.

## 6. Présentation et invalidation

Couleur d’action issue du thème et soulignement au survol ; ring visible au clavier. Un état visité
n’est pas implicitement enregistré : une application peut donner un style adapté.

Le trait de soulignement est peint avec les métriques de texte, sans remplacer le contenu par un
bouton rectangulaire. Couleur/hover ne relancent que paint.

Pas de chargement d’icônes ou ouverture réseau dans paint ; le callback intervient uniquement lors
d’une activation complète.

## 7. Accessibilité

SemanticRole ne contient actuellement pas Link. Utiliser Custom avec nom, description destination et
action Activate ; ajouter un rôle Link est une évolution séparée du modèle sémantique.

La destination n’est pas le nom : un label “Aide” et sa cible doivent être exposés séparément. Un
lien sans callback ou destination vide garde du texte mais n’annonce pas Activate.

SemanticInfo et SemanticAction sont les interfaces backend-neutres déjà disponibles. Le rôle indiqué
ici est un contrat cible : sa présence dans l’enum ne prouve pas sa publication par le composant
actuel.

Les ponts natifs restent différés (T068). Aucun résultat VoiceOver, UIA ou AT-SPI n’est revendiqué ;
vérifier le snapshot headless indépendamment du futur pont.

## 8. Cycle de vie et récupération

Copier destination et callback, désarmer l’activation puis appeler l’application. Si navigate lève,
conserver le lien utilisable ; aucune navigation n’est considérée rejouable.

Retrait du lien pendant un appui ou reconstruction d’un RichText annule le geste ; un lien de même
texte ne récupère pas une identité périmée.

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

Dépendances : TextService, Button/PressActivationState, disponibilité et focus communs ; RichText
utilise seulement le contrat de navigation.

Destination vide ou callback vide : aucune activation navigante, pas d’exception. Les caractères de
destination sont opaques au toolkit ; filtrage des schemes, permissions et URI relatives
appartiennent à l’application.

Des libellés identiques vers plusieurs destinations doivent rester des instances distinctes. Aucun
global “dernier lien visité” ni Router MyGo porté.

## 10. Fichiers et compatibilité

Cible : `include/nativeui/link.hpp` et `src/link.cpp`. Le header expose les déclarations publiques
et uniquement les adaptateurs templates nécessaires ; le .cpp doit porter un véritable noyau retenu,
interactions, mesure et rendu, jamais un fichier vide.

LinkStyle et NavigateCallback restent dans le couple Link. Le code du callback injecté ne pénètre
pas la couche platform ; l’adaptateur d’ouverture d’URL reste extérieur.

Inscrire `src/link.cpp` dans NativeUI::Core lors de l’implémentation. Préserver les includes
collectifs historiques comme points d’entrée compatibles ; aucun type Pugl, Skia, OS ou plugin dans
l’API publique.

Cette page spécifie le travail futur ; l’extraction, les ajouts C++ et la modification CMake ne sont
pas réalisés par le présent lot documentaire.

## 11. Tests et critères d’acceptation

Tests requis lors de l’implémentation ; aucun résultat d’exécution n’est annoncé par cette
documentation.

`link_destination` : callback reçoit exactement la destination possédée ; label distinct.

`link_keyboard` : Entrée et Espace activent une fois ; Tab et molette restent conformes au groupe.

`link_cancel_replace` : PointerCancel et remplacement de destination armée annulent sans mauvaise
navigation.

`link_wrap` : texte long et multi-octets garde mesure, clipping et focus sous resize.

`link_navigate_throw` : callback lève ou retire son sous-arbre ; captures et prochaine action
récupèrent.

Ajouter `examples/features/link.cpp`, compilable par le consommateur public, avec mode `--self-test`
vérifiant les transitions ci-dessus sans fenêtre interactive.

Acceptation : cas comportementaux et de récupération passent, rendu headless comparé à géométrie
stable, deux instances indépendantes, includes historiques compilables et nouvelles sources sans
avertissement.
