# Document de conception — Framework UI C++ Pugl + Skia

**Statut :** proposition d'architecture  
**Date :** 6 septembre 2026  
**Cible :** Windows, macOS, Linux  
**Langage :** C++20  
**Distribution :** bibliothèques statiques  
**Windowing :** Pugl (Win32 / Cocoa / X11)  
**Rendu :** Skia Ganesh/OpenGL (fenêtré) + Skia raster (headless)  

---

## 1. Objectif

Extraire et généraliser **uniquement la partie UI** du prototype actuel afin d'obtenir un framework C++ autonome pouvant être utilisé :

- dans une application standalone ;
- dans une vue embarquée dans un plugin ;
- sans dépendance à CLAP, VST3, AU, AAX ou à un modèle de paramètres audio ;
- avec **Pugl** comme couche de fenêtrage/événements native sur Windows, macOS et X11 ;
- avec Skia comme moteur de rendu unique ;
- avec une intégration simple sous forme de bibliothèques statiques.

Le framework doit conserver les éléments intéressants du POC :

- DSL C++ déclarative ;
- arbre de composants retained-mode ;
- layout `Row` / `Column` ;
- focus, hit-testing et interactions ;
- widgets composables (`Header`, `Knob`, `Toggle`, `TextInput`, etc.) ;
- invalidation à la demande ;
- texte UTF-8 et presse-papiers ;
- possibilité d'ajouter un composant custom sans modifier le core.

Le nouveau framework **ne reprend pas** :

- `Parameter<T>` ;
- `ChangeOrigin` ;
- `Gesture` liée aux paramètres de plugin ;
- automation ;
- host callbacks ;
- logique CLAP/VST/AU ;
- DSP ou audio.

Le support des plugins signifie uniquement que la bibliothèque sait créer une **vue native enfant** dans un parent natif fourni par un adaptateur externe.

Pugl reste un **détail d'implémentation** : aucun widget ni code applicatif n'inclut ses headers.

## 2. Décisions principales

### 2.1 Fenêtrage et événements : Pugl

Le framework **n'implémente plus directement Win32, Cocoa et X11**.

Pugl devient la couche de portabilité native pour :

- création/destruction des vues ;
- standalone et embedded ;
- parent natif via `puglSetParent()` ;
- clavier, texte Unicode commité, souris, wheel et focus ;
- curseurs ;
- clipboard et drag-and-drop ;
- timers ;
- scale factor ;
- invalidation/expose ;
- event pump.

Pugl est explicitement conçu pour les applications et les GUIs embarquées dans des plugins, sans état global mutable implicite et avec static linking possible.

Modes retenus :

```cpp
// standalone
puglNewWorld(PUGL_PROGRAM, 0);

// vue embarquée dans un host
puglNewWorld(PUGL_MODULE, 0);
```

En mode embedded :

```cpp
puglSetParent(view, parentNativeHandle);
```

La boucle d'événements est différente :

```cpp
// standalone : peut attendre
puglUpdate(world, -1.0);

// plugin/embedded : ne bloque jamais le host
puglUpdate(world, 0.0);
```

Le core UI n'expose pas Pugl. Les classes publiques restent `StandaloneWindow`, `EmbeddedView`, `View` et les types d'input NativeUI.

### 2.2 Intégration Pugl : vendored/static et commit pinné

NativeUI utilise Pugl comme petite dépendance source statique :

- repository : `lv2/pugl` ;
- licence ISC ;
- commit exact pinné dans le lock de dépendances ;
- source récupérée par CPM.cmake à un commit exact ;
- Pugl est ensuite compilé statiquement directement par les targets CMake de NativeUI ;
- pas de dépendance Meson imposée au consommateur.

Pugl documente explicitement le vendoring manuel : seuls les headers, sources communes, source plateforme et backend graphique nécessaires doivent être compilés.

Le pin exact est obligatoire car Pugl indique que son API est encore susceptible d'évoluer avant stabilisation long terme.

### 2.3 Rendu fenêtré v1 : Pugl OpenGL + Skia Ganesh GL

Pour maximiser la simplification cross-platform, le renderer de fenêtre v1 utilise :

```text
Pugl native view
      |
      v
Pugl OpenGL backend
      |
      v
current GL context
      |
      v
Skia Ganesh / OpenGL
      |
      v
SkSurface / SkCanvas
```

Cette décision supprime entièrement les presenters maison :

- pas de `CreateDIBSection` ;
- pas de CoreGraphics/CALayer raster presenter ;
- pas de `XImage` / MIT-SHM ;
- pas de code de création de contexte OpenGL propre à NativeUI.

`skia-builder` compile déjà ses packages desktop GPU avec `skia_use_gl=true`.

Le renderer headless de tests utilise toujours `SkSurfaces::Raster()` et ne dépend pas de Pugl/OpenGL.

### 2.4 GPU futur

OpenGL/Ganesh est choisi comme **baseline portable**, pas comme engagement architectural définitif.

Après stabilisation, un backend Skia Graphite peut utiliser les artefacts Dawn de `skia-builder` :

```text
macOS   -> Metal / Dawn / Graphite
Windows -> D3D12 / Dawn / Graphite
Linux   -> Vulkan / Dawn / Graphite
```

Le toolkit, les composants et la DSL ne changent pas.

### 2.5 Coordonnées logiques

Toute l'UI travaille en **logical pixels**. Pugl expose sa géométrie native en pixels physiques ; NativeUI divise tailles et positions Pugl par `puglGetScaleFactor()` avant de les transmettre au toolkit. Le framebuffer Skia conserve la taille physique Pugl et le canvas est mis à l'échelle une seule fois.

```text
Pugl physical geometry
      |
      | / scaleFactor
      v
Component layout (logical)
      |
      v
Skia Canvas scaled once
      |
      v
Physical framebuffer
```

### 2.6 Bibliothèques statiques

Le consommateur final doit idéalement écrire :

```cmake
find_package(NativeUI CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE NativeUI::NativeUI)
```

Il ne gère directement ni Pugl ni les dizaines de bibliothèques internes Skia.

## 3. Étude des projets existants

### 3.1 Pugl — backend windowing retenu

Repository : `lv2/pugl`

Pugl n'est plus seulement une référence : **il devient l'implémentation windowing de NativeUI**.

Raisons :

- objectif explicite : GUIs embeddables et plugins ;
- Windows, macOS et X11 ;
- `puglSetParent()` pour l'embarquement ;
- `PUGL_PROGRAM` / `PUGL_MODULE` ;
- event loop non bloquante adaptée aux plugins ;
- clipboard et drag-and-drop ;
- texte Unicode généré par le système d'input ;
- timers, cursor, focus, scale factor ;
- OpenGL backend disponible ;
- zéro dépendance hors bibliothèques système ;
- static linking et vendoring explicitement supportés ;
- aucun état global mutable obligatoire.

Limites connues :

- pas de backend Wayland natif actuellement ;
- API encore susceptible d'évoluer : pin exact obligatoire ;
- `PUGL_TEXT` fournit le texte Unicode commité mais Pugl n'expose pas aujourd'hui une API complète de préédition IME/marked text. NativeUI traite cela comme une extension ultérieure, pas comme une raison de réécrire tout le windowing.

### 3.2 iPlug2 / IGraphics — référence plugin + Skia

Repository : `iPlug2/iPlug2`

À étudier surtout pour :

- lifecycle répété dans les hosts ;
- ressources/fontes dans les bundles plugins ;
- resize/scale ;
- intégration Skia ;
- comportement standalone vs plugin.

À éviter : le couplage paramètres/audio.

### 3.3 `olilarkin/skia-builder` — source de vérité pour Skia

Repository : `olilarkin/skia-builder`

NativeUI ne maintient **aucun pipeline Skia parallèle**. `skia-builder` reste la source de vérité pour :

- version Skia ;
- GN args ;
- LLVM/clang ;
- macOS, Windows et Linux ;
- `/MT` / `/MD` Windows ;
- headers et libs statiques ;
- OpenGL ;
- Dawn/Graphite futur.

Au 6 septembre 2026, la CI upstream couvre notamment macOS universal/arm64/x86_64, Windows x64 `/MT` et `/MD`, et Linux x64. La release observée pendant cette conception est `chrome/m149`.

Le package contient notamment :

```text
skia
skshaper
skparagraph
skunicode_core
skunicode_icu
```

et les modules GPU utiles plus tard.

### 3.4 DPF/DGL — validation pratique de Pugl dans des plugins audio

DPF/DGL vendorise Pugl et l'utilise pour des GUIs de plugins. C'est une référence pratique utile pour :

- embedding ;
- static vendoring ;
- comportement host ;
- tests de compatibilité X11.

NativeUI n'adopte pas DPF : seul le pattern d'intégration Pugl est pertinent.

### 3.5 Skia officiel

Skia reste consommé via `skia-builder`. NativeUI n'exécute ni GN ni Ninja et n'utilise pas le générateur CMake de Skia comme pipeline production.

## 4. Architecture générale

```text
                         Application / Plugin adapter
                                    |
                                    v
+------------------------------------------------------------------+
|                         NativeUI public API                       |
|                                                                  |
|   DSL      Components      Layout      Focus/Input      State     |
+----------------------------+-------------------------------------+
                             |
                             v
                      Runtime Component Tree
                             |
              +--------------+--------------+
              |                             |
              v                             v
        detail::PuglHost                SkiaRenderer
              |                             |
      Pugl world/view                  Ganesh GL
              |                             |
      native OS window                 SkCanvas
              |                             |
              +-------------+---------------+
                            v
                    Pugl OpenGL context
```

### Principe de dépendances

```text
widgets/core
    |
    +----> abstract NativeUI input/services
    |
    +----> PaintContext / SkCanvas

platform integration
    |
    +----> Pugl

render integration
    |
    +----> Skia
    +----> Pugl OpenGL backend
```

Aucun widget ne dépend de Pugl, OpenGL, Win32, AppKit ou Xlib.

### Modules

```text
nativeui/
├── include/nativeui/
│   ├── ui.hpp
│   ├── component.hpp
│   ├── geometry.hpp
│   ├── input.hpp
│   ├── state.hpp
│   ├── theme.hpp
│   ├── window.hpp
│   ├── paint.hpp
│   ├── layout/
│   └── widgets/
│
├── src/core/
│   ├── tree.cpp
│   ├── layout.cpp
│   ├── focus.cpp
│   ├── input_dispatch.cpp
│   ├── invalidation.cpp
│   └── animation.cpp
│
├── src/platform/pugl/
│   ├── pugl_host.cpp
│   ├── pugl_events.cpp
│   ├── pugl_clipboard.cpp
│   └── pugl_text.cpp
│
├── src/render/skia/
│   ├── skia_gl_renderer.cpp
│   ├── skia_headless_renderer.cpp
│   ├── fonts_skia.cpp
│   └── text_skia.cpp
│
├── src/platform/extensions/          # uniquement si nécessaire
│   ├── ime_win.cpp
│   ├── ime_mac.mm
│   └── ime_x11.cpp
│
├── cmake/
│   ├── NativeUIConfig.cmake.in
│   ├── PuglVendored.cmake
│   └── SkiaBuilderPackage.cmake
│
└── tests/
    ├── core/
    ├── rendering/
    ├── golden/
    └── integration/
```

`src/platform/extensions` est optionnel et doit rester très petit. Il n'est utilisé que pour les capacités non exposées par Pugl, par exemple une future préédition IME complète.

## 5. API publique

### 5.1 Composition déclarative

La syntaxe du POC reste un objectif :

```cpp
ui::UI ui {
    ui::Column {
        ui::Header{"LIVING INSTRUMENTS"},

        ui::Row {
            ui::Knob{"Drive", drive},
            ui::Knob{"Tone", tone},
            ui::Knob{"Mix", mix},
        },

        ui::Toggle{"Bypass", bypass},
        ui::TextInput{"Preset name", presetName}
    }
    .padding(24)
    .gap(18)
};
```

### 5.2 Aucun type de paramètre audio

`drive`, `tone`, etc. ne sont plus des paramètres de plugin.

Le framework fournit seulement un état/binding UI générique :

```cpp
ui::State<float> drive{0.5f};
ui::State<bool> bypass{false};
ui::State<std::string> presetName{"Init"};
```

`ui::State<T>` est volontairement minimal :

```cpp
template<class T>
class State {
public:
    const T& get() const;
    void set(T value);
    Subscription observe(Callback<T>);
};
```

Il ne contient aucune notion de :

- begin/end edit ;
- automation ;
- host ;
- normalized value ;
- parameter ID ;
- audio thread.

Un plugin peut créer son propre bridge externe :

```text
Plugin parameter <---- adapter externe ----> ui::State<float>
```

Ce bridge ne fait pas partie de NativeUI.

### 5.3 Composant custom

```cpp
class Oscilloscope final : public ui::Component {
public:
    ui::Size measure(const ui::MeasureContext&) const override;
    void paint(ui::PaintContext&) const override;
    void input(const ui::InputEvent&, ui::InputContext&) override;
};
```

Ajouter `Oscilloscope` ne nécessite aucune modification du core, du renderer ou du backend plateforme.

---

## 6. Compilation de la DSL en arbre runtime

La DSL est utilisée uniquement à la construction.

```text
Column<Row<Knob, Knob>, Toggle, TextInput>
                 |
                 | compile
                 v
              Node
          /     |      \
       Node    Node    Node
        |       |       |
  Component* Component* Component*
```

Le runtime utilise du type erasure :

```cpp
struct Node {
    std::unique_ptr<Component> component;
    std::vector<std::unique_ptr<Node>> children;
    Rect bounds;
};
```

Bénéfices :

- API déclarative agréable ;
- aucune explosion de types templates à l'exécution ;
- recompilation limitée ;
- composants dynamiques possibles ;
- arbre introspectable pour debug/accessibility.

---

## 7. Standalone vs Embedded

Le core UI ne sait toujours pas s'il tourne dans un plugin. La différence est confinée à `detail::PuglHost`.

### 7.1 Standalone

```cpp
ui::StandaloneWindow window {
    ui::WindowDesc{
        .title = "Demo",
        .size = {800, 500},
        .resizable = true
    },
    uiTree
};

return ui::runApplication(window);
```

Implémentation :

```cpp
world = puglNewWorld(PUGL_PROGRAM, 0);
view  = puglNewView(world);
```

La boucle standalone peut bloquer entre les événements :

```cpp
while (!quit)
    puglUpdate(world, -1.0);
```

Lorsqu'une animation est active, un timeout borné ou les timers Pugl sont utilisés.

### 7.2 Embedded / plugin

```cpp
ui::EmbeddedView embedded{uiTree};
embedded.attach(parentHandle, {640, 420});
```

Implémentation :

```cpp
world = puglNewWorld(PUGL_MODULE, 0);
view  = puglNewView(world);
puglSetParent(view, parentHandle);
```

Règle absolue : l'event pump plugin ne bloque jamais :

```cpp
embedded.poll(); // appelle puglUpdate(world, 0.0)
```

L'adaptateur CLAP/VST3/AU appelle `poll()` depuis son mécanisme idle/timer approprié. Cette logique reste hors NativeUI.

### 7.3 Lifecycle

Les instances Pugl sont détenues par la vue :

```text
EmbeddedView / StandaloneWindow
        |
        v
     PuglWorld
        |
        v
     PuglView
        |
        v
   SkiaRenderer
```

Ordre de destruction : renderer/context resources, Pugl view, Pugl world.

## 8. Intégration Pugl interne

Il n'y a plus d'interface virtuelle Win32/Cocoa/X11 publique.

NativeUI encapsule Pugl dans une classe interne :

```cpp
class PuglHost final {
public:
    bool createProgram(const WindowDesc&);
    bool createModule(NativeParentHandle, Size);

    void poll(double timeoutSeconds);
    void requestRedraw(Rect dirty);
    void setCursor(Cursor);
    void startTimer(uintptr_t id, double seconds);
    void stopTimer(uintptr_t id);

    float scaleFactor() const;
    NativeHandle nativeHandle() const;

    void setClipboardText(std::string_view);
    void requestClipboardText();
};
```

`NativeParentHandle` reste un type opaque NativeUI capable de transporter le handle natif attendu par Pugl.

### Traduction des événements

```text
PuglEvent
   |
   v
PuglHost::onEvent()
   |
   v
NativeUI InputEvent / WindowEvent
   |
   v
Tree dispatch
```

Mapping principal :

```text
PUGL_KEY_PRESS/RELEASE -> KeyDown/KeyUp
PUGL_TEXT              -> TextInput
PUGL_BUTTON_*          -> PointerDown/PointerUp
PUGL_MOTION            -> PointerMove
PUGL_SCROLL            -> PointerWheel
PUGL_FOCUS_*           -> FocusIn/FocusOut
PUGL_CONFIGURE         -> Resize/ScaleChanged
PUGL_EXPOSE            -> render
PUGL_TIMER             -> Animation/Timer
PUGL_DATA_*            -> Clipboard/Drop
```

### Wayland

Pugl ne supporte pas actuellement Wayland. NativeUI ne crée pas d'abstraction générale supplémentaire tant qu'un besoin concret n'existe pas.

Si Wayland standalone devient obligatoire, deux options seront évaluées :

1. contribution d'un backend Wayland à Pugl ;
2. ajout d'un host Wayland alternatif derrière la même API publique `StandaloneWindow`.

Le plugin Linux de référence reste X11.

## 9. Backend Pugl : responsabilités

Pugl remplace les anciens backends Windows/macOS/X11 maison.

NativeUI délègue à Pugl :

- création du child window ou top-level window ;
- relation parent/enfant ;
- dispatch d'événements ;
- gestion du contexte OpenGL ;
- focus fenêtre ;
- curseurs ;
- clipboard ;
- drag-and-drop ;
- timers ;
- scale factor ;
- expose/invalidation ;
- lifecycle plateforme.

NativeUI conserve :

- hit-testing des composants ;
- focus **interne au toolkit** (`Tab`, `Shift+Tab`) ;
- capture logique d'une interaction ;
- état hover/pressed/drag ;
- TextInput editing model ;
- layout ;
- Skia drawing.

Le backend Pugl est considéré comme une dépendance interne remplaçable, mais on n'ajoute pas de couche virtuelle supplémentaire tant qu'aucun second backend réel n'existe.

## 10. Renderer fenêtre : Pugl OpenGL + Skia

### 10.1 Création

La vue Pugl utilise son backend OpenGL :

```cpp
puglSetBackend(view, puglGlBackend());
```

Les hints OpenGL sont fixés avant `puglRealize()`.

Lors d'un `PUGL_EXPOSE`, Pugl a rendu le contexte approprié courant ; NativeUI crée/utilise alors un contexte Skia Ganesh associé à OpenGL.

### 10.2 Surface

Le renderer enveloppe le framebuffer courant dans une `SkSurface` adaptée à la taille physique de la vue.

```text
logical size * scaleFactor
          |
          v
physical framebuffer size
          |
          v
GrBackendRenderTarget
          |
          v
SkSurface
```

Le `SkCanvas` est exposé aux composants via `PaintContext`.

### 10.3 Présentation

Après le dessin :

```text
Skia flush
   |
   v
OpenGL framebuffer
   |
   v
Pugl backend leave/swap
```

NativeUI ne fait pas de swap plateforme manuel.

### 10.4 Tests headless

Les golden tests n'utilisent ni Pugl ni OpenGL :

```cpp
auto surface = SkSurfaces::Raster(...);
```

Cela garde les tests de layout/widgets déterministes et exécutables en CI sans display.

## 11. Plateformes supportées et limites

### Windows

Pugl utilise Win32 en interne. NativeUI n'appelle pas directement `CreateWindowExW`, `WndProc`, `SetCapture`, etc. dans le chemin normal.

Le parent plugin reste un `HWND` converti vers `PuglNativeView` par l'adaptateur externe.

### macOS

Pugl utilise Cocoa/AppKit en interne. NativeUI ne crée pas de `NSWindow`/`NSView` custom pour le windowing normal.

Le parent plugin reste un `NSView*` transmis comme native parent.

### Linux

Pugl utilise X11. Le parent plugin est un X11 `Window`/XID.

Le standalone Wayland natif n'est pas dans la v1.

### Escape hatches plateforme

Les appels plateforme directs ne sont autorisés que dans `src/platform/extensions/` lorsqu'une capacité nécessaire n'est réellement pas fournie par Pugl.

Premier candidat : préédition IME complète / positionnement précis du candidat. Toute extension doit rester derrière une petite interface et être couverte par tests plateforme.

## 12. Renderer Skia

### 12.1 Renderer fenêtré

```cpp
class SkiaGlRenderer {
public:
    bool initialize(PuglView&);
    void resize(Size logical, float scale);
    SkCanvas& beginFrame(const DirtyRegion&);
    void endFrame();
};
```

Le renderer suppose que Pugl a rendu le contexte OpenGL courant dans le callback expose.

### 12.2 `PaintContext`

```cpp
class PaintContext {
public:
    SkCanvas& canvas();
    const Theme& theme() const;
    Rect bounds() const;
    float scaleFactor() const;
    Painter& painter();
};
```

### 12.3 Renderer headless

```cpp
class SkiaRasterRenderer;
```

Utilisé uniquement pour :

- tests ;
- génération de golden images ;
- éventuels outils headless.

### 12.4 Fonts et texte

Le framework gère :

- fontes système ;
- fontes embarquées ;
- cache `SkTypeface` ;
- fallback ;
- shaping Unicode via les modules fournis par `skia-builder`.

Aucune fonte n'est chargée directement depuis un chemin hardcodé dans un widget.

## 13. Invalidation et rendu

Principe : aucun redraw permanent.

```text
Input / State change
       |
       v
Component::invalidate(rect)
       |
       v
Tree dirty region
       |
       v
PuglHost::requestRedraw(rect)
       |
       v
puglObscureRegion()/puglObscureView()
       |
       v
PUGL_EXPOSE
       |
       v
Skia render
```

Le framework agrège les dirty rects côté core. L'événement `PUGL_EXPOSE` reste l'unique point normal de dessin fenêtré.

Au repos, `PUGL_MODULE` est pollé avec timeout zéro uniquement lorsque l'intégrateur le demande ; aucun timer 60 Hz n'est créé.

Pour les animations, NativeUI utilise `puglStartTimer()`/`puglStopTimer()` ou un timeout standalone adapté, uniquement tant qu'une animation est active.

## 14. Layout

Le layout actuel `measure -> place` est conservé.

Interface :

```cpp
class Component {
public:
    virtual Size measure(const MeasureContext&) const = 0;
    virtual void layout(const LayoutContext&) {}
    virtual void paint(PaintContext&) const = 0;
};
```

Containers MVP :

- `Row` ;
- `Column` ;
- `Stack` ;
- `Spacer` ;
- `Padding`.

Puis :

- `Grid` ;
- `Flex` si le besoin réel apparaît.

Ne pas introduire immédiatement un moteur CSS/Flexbox complet.

---

## 15. Input, focus et interactions

### Événements système

Pugl fournit les événements système. `PuglHost` les convertit immédiatement en types NativeUI indépendants.

```cpp
using InputEvent = std::variant<
    PointerDown,
    PointerMove,
    PointerUp,
    PointerWheel,
    KeyDown,
    KeyUp,
    TextInput,
    FocusIn,
    FocusOut
>;
```

### Focus toolkit

Le `Tree` reste responsable de :

- hit-test ;
- focus order ;
- `Tab` / `Shift+Tab` ;
- hover ;
- pressed ;
- interaction drag ;
- dispatch vers le composant ciblé.

Le focus OS de la vue est géré par Pugl (`puglGrabFocus`, événements focus).

### InputContext

```cpp
ctx.invalidate();
ctx.beginPointerInteraction();
ctx.endPointerInteraction();
ctx.requestFocus();
ctx.setCursor(Cursor::ResizeHorizontal);
```

La capture du pointeur au niveau toolkit est d'abord **logique** : pendant un drag actif, les mouvements reçus sont routés vers le composant initiateur. Si une vraie capture OS supplémentaire s'avère nécessaire sur une plateforme, elle passe par une extension Pugl ciblée, pas par les widgets.

## 16. TextInput

Le modèle d'édition du `TextInput` est un objet public headless `ui::TextEditModel`, indépendant de Skia/Pugl. `TextInputComponent` ne conserve que les responsabilités de vue : hit-test pixel, scroll horizontal, focus/clipboard et peinture.

`TextEditModel` possède :

- texte UTF-8 ;
- curseur et ancre de sélection en offsets byte toujours alignés sur des frontières de codepoint ;
- navigation codepoint/mot/document ;
- insertion, suppression et limite de longueur en codepoints ;
- undo/redo borné ;
- sélection de mot et sélection totale.

Features v1 du composant :

- UTF-8 ;
- insertion/suppression ;
- caret ;
- sélection ;
- Shift + navigation ;
- Home/End ;
- navigation par mot ;
- sélection souris ;
- double clic mot ;
- triple clic all ;
- clipboard ;
- undo/redo ;
- placeholder ;
- max length ;
- scroll horizontal ;
- submit ;
- Escape/revert.

### Saisie texte

Pugl envoie `PUGL_TEXT` avec le caractère Unicode et une représentation UTF-8 issue du système d'input. Cette voie sert à l'insertion normale et aux dead keys/input methods qui produisent du texte commité.

### Clipboard

NativeUI utilise directement :

- `puglSetClipboard()` ;
- `puglPaste()` ;
- `PUGL_DATA_OFFER` ;
- `puglAcceptOffer()` ;
- `PUGL_DATA` ;
- `puglGetClipboard()`.

### IME avancé

La v1 garantit le **texte Unicode commité**. La préédition IME complète (marked text, candidate rectangle, composition temporaire) n'est pas considérée comme fournie par l'API Pugl actuelle.

Si elle devient un critère de release, NativeUI ajoute une petite interface :

```cpp
class TextCompositionBridge {
public:
    void begin(const Rect& caretRect);
    void updateCaret(const Rect&);
    void end();
};
```

avec implémentations plateforme confinées à `src/platform/extensions/`. On ne réécrit pas le windowing pour ce seul besoin.

## 17. Static build et intégration

### 17.1 Target final

```cmake
find_package(NativeUI CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE NativeUI::NativeUI)
```

`NativeUI::NativeUI` contient :

- core UI ;
- widgets ;
- Pugl core + backend OpenGL compilés statiquement ;
- intégration Skia ;
- liens système transitifs nécessaires.

Skia reste fourni comme imported static libraries depuis `skia-builder`.

### 17.2 Pugl via CMake + CPM, sans Meson imposé

Toutes les dépendances externes sont déclarées via CPM.cmake. Pour Pugl, CPM récupère **uniquement le source pinné**, puis NativeUI compile statiquement les fichiers requis elle-même :

```cmake
CPMAddPackage(
  NAME pugl
  GITHUB_REPOSITORY lv2/pugl
  GIT_TAG <exact-commit>
  DOWNLOAD_ONLY YES
)

add_library(nativeui_pugl STATIC
  ${pugl_SOURCE_DIR}/src/common.c
  ${pugl_SOURCE_DIR}/src/internal.c
  # + source plateforme
  # + backend OpenGL plateforme
)
```

La liste exacte des sources est centralisée dans `cmake/PuglVendored.cmake` et validée à chaque bump Pugl.

Le consommateur n'a besoin ni de Meson ni d'un package Pugl système.

### 17.3 Skia via CPM + artefacts `skia-builder`

Il n'existe toujours aucun build Skia dans NativeUI. CPM télécharge l'archive binaire pinnée de la release `olilarkin/skia-builder` correspondant à la plateforme, puis CMake crée les imported targets :

```cmake
-DNATIVEUI_SKIA_ROOT=/path/to/extracted/skia-builder-release
```

ou, par défaut, téléchargement de l'asset GitHub Release pinné via `CPMAddPackage(URL ... URL_HASH ...)`.

### 17.4 Locks

NativeUI pinne deux dépendances :

```text
Pugl:
  repository = lv2/pugl
  commit

Skia:
  repository = olilarkin/skia-builder
  release_tag
  asset_name
  sha256
```

Aucun fork préventif n'est maintenu.

## 18. Utilisation des variantes `skia-builder`

### 18.1 Renderer fenêtré v1

NativeUI utilise les assets `*-gpu-*` de `skia-builder`, mais **ne lie pas Dawn** dans la v1.

Le renderer utilise Ganesh/OpenGL :

```text
Pugl GL context
    |
    v
Skia Ganesh GL
```

Modules minimum :

```text
skia
+ modules texte réellement utilisés
+ OpenGL/system libs nécessaires
```

### 18.2 Tests headless

Le même `libskia` sert au renderer raster headless :

```cpp
SkSurfaces::Raster(...);
```

Il n'y a donc toujours qu'un seul package Skia par plateforme/configuration.

### 18.3 Renderer GPU futur

Le même package permet plus tard de lier `dawn_combined` pour Graphite/Metal/D3D/Vulkan sans modifier le pipeline de dépendances.

## 19. Texte et modules Skia provenant de `skia-builder`

Le script upstream construit déjà les modules suivants pour macOS, Windows et Linux :

```text
skia
skottie
sksg
skshaper
skparagraph
svg
skunicode_core
skunicode_icu
```

Pour NativeUI v1, le set de link recommandé est :

```text
skia
skshaper
skparagraph       # si multiline / paragraph layout utilisé
skunicode_core
skunicode_icu
```

Le principe est : **ne pas modifier le build upstream pour enlever des modules ; simplement ne pas les lier**.

Cela réduit fortement la maintenance et laisse à `skia-builder` la responsabilité de suivre les changements de Skia.

---

## 20. Dépendances système

NativeUI dépend uniquement de :

1. Pugl statiquement compilé dans NativeUI ;
2. Skia statique fourni par `skia-builder` ;
3. OpenGL et bibliothèques système nécessaires à Pugl/Skia.

### Windows

- Win32 libs transitivement nécessaires à Pugl ;
- OpenGL (`opengl32`) ;
- asset Skia `/MT` ou `/MD` correspondant au runtime du consommateur.

### macOS

- Cocoa/AppKit/CoreFoundation/CoreText/CoreGraphics selon les besoins de Pugl/Skia ;
- OpenGL framework pour la baseline Ganesh/GL.

### Linux/X11

- X11 et dépendances Pugl X11 ;
- OpenGL/GLX ;
- `pthread`/`dl` et dépendances requises par Skia.

Aucune dépendance SDL, GLFW, Qt, JUCE ou GTK.

Wayland n'est pas une dépendance de la v1.

## 21. ABI et toolchain

Le framework est distribué prioritairement comme **static C++ library**, donc il n'essaie pas de fournir une ABI C++ stable entre compilateurs incompatibles.

Contraintes :

- C++20 ;
- même famille de runtime C/C++ entre Skia et le consommateur ;
- Windows : sélectionner directement les packages `/MT` ou `/MD` publiés par `skia-builder` ;
- Debug et Release sélectionnés via les assets upstream correspondants ;
- pin exact sur le tag de release `skia-builder` ;
- symbole de version du framework exposé pour diagnostic.

Pour les utilisateurs construisant depuis source, `NativeUI::NativeUI` reste la voie recommandée.

---

## 22. Ressources

Le framework doit disposer d'un système indépendant du format plugin :

```cpp
class ResourceProvider {
public:
    virtual Blob load(std::string_view id) = 0;
};
```

Providers :

- filesystem pour standalone/dev ;
- embedded byte arrays ;
- Win32 resources ;
- macOS bundle ;
- callback externe.

Cela évite le problème classique « standalone fonctionne, plugin ne trouve pas la fonte ».

Les widgets ne chargent jamais directement un chemin du système.

---

## 23. Threading

Règles strictes :

- toute mutation de l'arbre UI sur le UI thread ;
- aucune dépendance à un audio thread ;
- aucune lock interne nécessaire pour le parcours UI normal ;
- `post()` permet d'envoyer une tâche au UI thread depuis une source externe ;
- aucun worker obligatoire ;
- le renderer est utilisé sur le UI thread dans le backend raster initial.

```cpp
view.post([](ui::View& v) {
    // modification sûre sur le thread UI
});
```

---

## 24. Resize et scale pour intégration plugin

Le framework expose uniquement des mécanismes neutres :

```cpp
view.setSize({640, 420});
view.setScaleFactor(1.5f);
view.onPreferredSizeChanged(...);
```

La négociation avec un host CLAP/VST3/AU reste dans l'adaptateur du plugin.

```text
NativeUI requests preferred size
              |
              v
external plugin adapter
              |
              v
CLAP/VST/AU host API
```

---

## 25. Tests

### Core sans fenêtre

Tester :

- DSL → tree ;
- measure/layout ;
- hit-test ;
- focus ;
- interactions drag ;
- TextInput ;
- `State<T>` ;
- invalidation.

### Rendering headless

Créer une `SkSurface` raster et comparer des golden PNG.

### Pugl integration smoke tests

Pour chaque OS :

1. `PUGL_PROGRAM` create/show/resize/destroy ;
2. `PUGL_MODULE` + parent native ;
3. `puglUpdate(..., 0.0)` non bloquant ;
4. repeated attach/detach ;
5. multiple instances ;
6. scale factor ;
7. keyboard/text ;
8. mouse/wheel ;
9. clipboard ;
10. timers/invalidation ;
11. OpenGL context + Skia frame ;
12. fermeture du host pendant interaction.

### Matrice hosts plugin

Même si NativeUI ne contient aucun code plugin, un repository/examples d'intégration doit valider l'embedded view dans plusieurs hosts réels sur :

- Windows ;
- macOS ;
- Linux/X11.

Le but est de détecter les problèmes de parentage/event pump/contexte OpenGL spécifiques aux hosts.

### Static integration test

Projet externe minimal avec uniquement :

```cmake
target_link_libraries(smoke PRIVATE NativeUI::NativeUI)
```

Aucune installation système Pugl ne doit être requise.

## 26. CMake public

Options proposées :

```cmake
NATIVEUI_BUILD_TESTS=OFF
NATIVEUI_BUILD_EXAMPLES=OFF

# Pugl (CPM source dependency)
NATIVEUI_PUGL_SOURCE=              # optional offline override
NATIVEUI_PUGL_COMMIT=b7637149ebe53124e5be90559e02a0185bbcbd73

# skia-builder (CPM binary archive dependency)
NATIVEUI_SKIA_ROOT=                # optional offline/extracted override
NATIVEUI_SKIA_TAG=chrome/m149
NATIVEUI_SKIA_WINDOWS_CRT=MD
NATIVEUI_SKIA_CONFIG=Release

# Renderer
NATIVEUI_RENDERER=ganesh_gl      # v1
NATIVEUI_ENABLE_HEADLESS=ON

# Future/platform extensions
NATIVEUI_ENABLE_ADVANCED_IME=OFF
NATIVEUI_ENABLE_ACCESSIBILITY=OFF
```

Targets internes :

```text
NativeUI::NativeUI
NativeUI::Pugl
SkiaBuilder::skia
SkiaBuilder::skshaper
SkiaBuilder::skparagraph
SkiaBuilder::skunicode_core
SkiaBuilder::skunicode_icu
```

Le consommateur normal ne lie que `NativeUI::NativeUI`.

## 27. Ce qu'il ne faut pas faire

### Ne pas réécrire le windowing Pugl

Pas de backend parallèle Win32/Cocoa/X11 tant qu'un manque Pugl précis n'a pas été démontré.

Une fonctionnalité manquante doit d'abord être traitée par :

1. API Pugl existante ;
2. petite extension locale ciblée ;
3. contribution upstream Pugl.

### Ne pas exposer Pugl dans les widgets

Un composant ne doit pas inclure `pugl.h`.

### Ne pas mettre de code plugin dans le core

Pas de CLAP, VST3, AU, `ParameterId`, host ou automation.

### Ne pas reconstruire Skia

NativeUI consomme les releases `olilarkin/skia-builder` et ne maintient ni GN args ni scripts de build Skia.

### Ne pas introduire SDL/GLFW/Qt/JUCE

Pugl couvre déjà le besoin windowing/embedding retenu.

### Ne pas multiplier les backends graphiques dans la v1

Ganesh/OpenGL est la baseline fenêtre. Raster est réservé au headless. Graphite/Dawn vient seulement après validation du besoin.

## 28. Roadmap d'implémentation

### Phase 0 — extraction du POC

- supprimer SDL/NanoVG ;
- supprimer logique `Parameter<T>` plugin ;
- conserver DSL, tree, layout, focus, TextInput ;
- introduire `ui::State<T>` générique ;
- adopter `PaintContext` Skia ;
- normaliser le modèle DPI : Pugl = pixels physiques, toolkit = coordonnées logiques.

**Sortie :** tests core headless.

### Phase 1 — dépendances

- pinner Pugl à un commit exact ;
- implémenter `PuglVendored.cmake` ;
- importer la release `skia-builder` pinnée ;
- implémenter `SkiaBuilderPackage.cmake`.

**Sortie :** bibliothèque statique minimale linkable sur les 3 OS.

### Phase 2 — Pugl standalone

- `PUGL_PROGRAM` ;
- event translation ;
- OpenGL backend ;
- Skia Ganesh GL ;
- scale/invalidation/timers ;
- clavier/souris/clipboard.

**Sortie :** même demo standalone Win/mac/X11.

### Phase 3 — embedded/plugin windowing

- `PUGL_MODULE` ;
- `puglSetParent()` ;
- `poll()` timeout zéro ;
- repeated attach/detach ;
- multiple instances ;
- tests host réels.

**Sortie :** vue embarquée fiable sans code plugin dans le core.

### Phase 4 — TextInput hardening

- PUGL_TEXT ;
- clipboard Pugl ;
- dead keys ;
- tests Unicode ;
- décider si advanced IME nécessite les petits shims plateforme.

### Phase 5 — packaging

- `NativeUI::NativeUI` ;
- install/export CMake ;
- resource provider ;
- static consumer smoke tests.

### Phase 6 — améliorations ultérieures

- accessibility ;
- Wayland ;
- Graphite/Dawn ;
- advanced IME complet ;
- drag-and-drop fichiers avancé.

## 29. Critères de réussite v1

La v1 est utilisable quand :

1. la même UI C++ compile sans `#ifdef` applicatif sur Windows/macOS/Linux X11 ;
2. elle fonctionne en standalone via Pugl sur les trois OS ;
3. elle fonctionne en embedded avec `PUGL_MODULE` et `puglSetParent()` ;
4. le poll embedded est non bloquant ;
5. `Tab`/`Shift+Tab`, souris, clavier, texte Unicode commité et clipboard fonctionnent ;
6. l'UI idle ne redraw pas ;
7. High-DPI/scale fonctionne ;
8. plusieurs instances Pugl/NativeUI peuvent vivre simultanément ;
9. open/close et attach/detach répétés ne fuient pas ;
10. Skia Ganesh/OpenGL rend correctement dans le contexte Pugl ;
11. les golden tests utilisent le renderer raster headless ;
12. le package final s'intègre avec un seul target CMake ;
13. aucun header plugin/audio n'est présent dans le framework ;
14. Pugl et `skia-builder` sont pinnés/reproductibles ;
15. aucun backend Win32/Cocoa/X11 maison n'est requis pour le chemin normal.

## 30. Recommandation finale

L'architecture retenue est :

```text
                    DSL C++
                       |
                       v
                Component Tree
              /       |        \
          Layout     Input      Paint
                      |           |
                      v           v
                 Pugl events     Skia
                      |           |
                      +-----+-----+
                            v
                    Pugl OpenGL View
                   /       |       \
               Win32     Cocoa     X11
```

Le pari technique devient beaucoup plus simple :

> **NativeUI développe un toolkit, pas un système de fenêtrage. Pugl gère le native windowing/embedding et Skia gère le dessin.**

Les responsabilités spécifiques à NativeUI se concentrent sur ce qui crée réellement de la valeur :

- DSL déclarative ;
- composants ;
- layout ;
- focus interne ;
- interaction ;
- TextInput model ;
- styling/theme ;
- binding générique ;
- invalidation ;
- ressources ;
- packaging CMake simple.

Les deux dépendances structurantes sont :

- `lv2/pugl` pinné, vendored/static ;
- `olilarkin/skia-builder` comme fournisseur des binaires Skia statiques.

Les deux risques principaux restant sont clairement isolés :

1. Pugl ne couvre pas Wayland aujourd'hui ;
2. la préédition IME complète peut nécessiter une extension plateforme minimale.

Ces risques ne justifient pas de réécrire Win32/Cocoa/X11.

## 31. Sources et projets de référence

- Pugl — https://github.com/lv2/pugl
- Pugl documentation — https://lv2.gitlab.io/pugl/c/html/
- DPF/DGL — https://github.com/DISTRHO/DPF
- iPlug2 — https://github.com/iPlug2/iPlug2
- skia-builder — https://github.com/olilarkin/skia-builder
- Skia official build documentation — https://skia.org/docs/user/build/

### Constats issus de l'étude

- Pugl se définit comme une couche portable minimale pour GUIs embeddables, adaptée aux plugins et applications, avec static linking/vendoring et sans état global mutable obligatoire.
- Pugl supporte actuellement Windows, macOS et X11 et fournit OpenGL, Vulkan, Cairo et un backend stub ; NativeUI retient OpenGL pour la v1.
- Pugl expose embedding, scale factor, focus, cursor, timers, clipboard, drag-and-drop et événements texte Unicode commité.
- Les plugins doivent appeler `puglUpdate()` avec timeout zéro afin de ne pas bloquer le host.
- Pugl documente explicitement le vendoring manuel sans imposer son build Meson au projet consommateur.
- `olilarkin/skia-builder` publie des artefacts statiques GPU pour macOS, Windows et Linux et active OpenGL dans sa configuration Skia desktop ; ces artefacts conviennent à Ganesh/GL et au raster headless.
- DPF constitue une preuve d'intégration pratique de Pugl dans des plugins audio, mais NativeUI reste indépendant de DPF et de toute API de plugin.

