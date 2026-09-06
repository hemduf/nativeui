# NativeUI implementation plan

## Strategy

Evolve the existing POC incrementally. Avoid a rewrite.

The current architecture is sufficient. Work should concentrate on missing toolkit primitives and quality rather than introducing additional abstraction layers.

## Phase A — Stabilize what already exists

1. Strengthen unit tests around `State<T>`, tree lifecycle, focus and Canvas input.
2. Separate the monolithic header into public modules while preserving source compatibility through `nativeui.hpp`.
3. Define event consumption and focus-change rules explicitly.
4. Add stable node IDs and lifecycle/deactivation semantics.
5. Replace the global dirty boolean with layout/paint invalidation and dirty-region aggregation.
6. Create a headless renderer/test target using Skia raster.

Reason: every later widget/layout feature depends on reliable lifecycle, input and invalidation.

## Phase B — Complete layout and input primitives

1. Add constraints (`min`, `max`, `preferred`).
2. Add alignment/distribution to Row/Column.
3. Add flex grow/shrink.
4. Add Grid.
5. Add clipping/overflow and ScrollView layout support.
6. Introduce handled/bubble input results.
7. Add focus scopes and command routing.
8. Harden pointer capture and drag helpers.

Reason: standard widgets should not invent their own layout or gesture systems.

## Phase C — Complete drawing and text primitives

1. Expand `Painter`/`CanvasContext2D` with save/restore, transforms, paths and gradients.
2. Add image/SVG/resource support.
3. Add font/typeface cache.
4. Add shaped text measurement.
5. Split the TextInput editing model from its visual component.
6. Add multiline TextArea and IME composition extension.
7. Build golden-image infrastructure.

Reason: this becomes the stable substrate for all visual components.

## Phase D — Build the standard widget library

Order widgets from simplest to most structurally demanding:

1. Button;
2. Checkbox/Radio;
3. Slider/RangeSlider;
4. ProgressBar/Meter;
5. Image;
6. ScrollView;
7. ComboBox/PopupMenu;
8. ListView;
9. Tabs.

Each widget gets:

- keyboard behavior;
- pointer behavior;
- focus behavior;
- disabled behavior;
- style states;
- core tests;
- rendering/golden test where applicable.

## Phase E — Theme and animation

1. Define typed theme tokens (spacing, typography, colors, radii, control metrics).
2. Add per-widget styles with state variants.
3. Add scoped inheritance.
4. Add animation scheduler/tweens bound to invalidation.
5. Verify no redraw loop exists while idle.

## Phase F — Embedded/platform hardening

Run dedicated smoke harnesses for:

- top-level window create/show/resize/destroy;
- embedded child create/attach/resize/destroy;
- repeated cycles;
- multiple instances;
- scale changes;
- clipboard;
- keyboard/text;
- pointer and drag;
- close during an active interaction.

Use real host/plugin adapters only in an external integration example/test project. NativeUI itself stays plugin-format neutral.

## Phase G — Packaging and release

1. export a stable CMake target;
2. install headers/library/config files;
3. provide minimal consumer samples;
4. add CI matrix;
5. add debug inspector/benchmark utilities;
6. document supported platform/compiler combinations;
7. tag v0.1 after the release gate in `ROADMAP.md` is satisfied.

## Continuous quality requirements

Throughout all phases:

- C++20;
- TDD for behavior;
- small units;
- no central component kind registry;
- no plugin parameter semantics;
- no direct platform code in widgets;
- CMake + CPM only for dependency acquisition;
- Pugl and skia-builder remain pinned;
- performance-sensitive paths avoid unnecessary allocation;
- every substantial change gets correctness, runtime and integration review passes.
