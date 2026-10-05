# StyleScope

Status: **existing — extraction required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

`StyleScope` applies typed lexical overrides to the inherited Theme. Source: [style_scope.hpp](../include/nativeui/style_scope.hpp), `StyleScope`, `StyleScopeOverrides`, `apply_style_scope_overrides`, `classify_style_scope_change`, `StyleScopeComponent`.

No independent MyGo family needs porting: MyGo inherits style properties. The NativeUI API is already public, with immutable overrides or Binding; extraction must preserve field-by-field precedence.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Existing API:

```cpp
template<class Child> StyleScope(StyleScopeOverrides overrides, Child&& child);
template<class Child> StyleScope(Binding<StyleScopeOverrides> overrides, Child&& child);
template<class Child> StyleScope(State<StyleScopeOverrides>& overrides, Child&& child);
Spec spec() &&;
```

Verified existing example:

```cpp
ui::StyleScopeOverrides patch;
patch.palette.accent=ui::Color{0.8f,0.3f,0.1f,1.0f};
auto scope = ui::StyleScope{patch, ui::Button{"Action", []{}}};
```

Preserve all Palette/Typography/Spacing/Radii/ControlOverrides types and the apply/classify functions. Options remain optional and absence means inherit; do not add constraints or a model to the patch.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

The value form is immutable for its retained lifetime; Binding form replacement is UI-thread confined. State already delegates to binding and retains no State*. Resolve from outermost to innermost, nearest-scope-wins per field, then apply the widget’s local recipe. Each scope owns its resolved Theme and weak observer.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

No input/focus/capture of its own. Controls retain their interaction and availability; changing the palette does not disable a child. Scopes can nest without a global “active style” mode. No theme shortcut.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

Pass through constraints, child minimum/preferred size and bounds. Typography/control overrides may change descendant metrics, but explicit width/height/Flex/Grid/scroll state cannot be represented in StyleScopeOverrides and remains authoritative.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

Compare the effective Theme before/after rather than only the raw patch: effective equality produces None, color produces Paint and metrics produce Layout according to the existing classify_theme_change. Do not remeasure for accent alone. An override equal to the inherited value can be an effective no-op; a parent change must recalculate descendants.

- Color comparison preserves the existing Color type’s r/g/b/a components.
- An empty family override is an explicit value; it does not mean nullopt/inherit.
- A changed fallback font is a potential metric change for the Theme resolver to classify.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Wrapper `None`. Style changes no name, role or action. Colors must preserve the contrasts chosen by the recipe/application; do not promise an automated contrast audit delivered by this component.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Disconnect the weak observer before destruction; the inherited Theme must belong to the live runtime rather than a temporary. Prepare the next Theme before publication and classify invalidation; an invalidator callback failure leaves durable pending work without a dangling theme pointer. Two scopes must not share a mutable resolved Theme.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Depends on Theme, ThemeBinding and classify_theme_change; preserve existing float types. Cases: an empty patch, an absent inner field, an equal palette, changed font-family/fallbacks, changed parent Theme and expired state. No availability/callback/model override is accepted.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/style_scope.hpp` and `src/style_scope.cpp`.

A header with this name already exists: retain it as the canonical API and move non-template implementations into style_scope.cpp. Preserve public functions/equality and types; only the three template child conversions remain inline. Detail types do not become a global service.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `style_scope_nearest_field`: per-field precedence, inheritance through absence.
- `style_scope_effective_noop`: a patch equal to the current Theme causes no invalidation.
- `style_scope_paint_layout`: color produces Paint, typography produces Layout.
- `style_scope_parent_change`: recalculation of a nested partial patch.
- `style_scope_local_recipe`: the widget recipe is applied after the scope.
- `style_scope_lifetime_fault`: expiration/throw followed by an inert stale callback.

Create the future public example `examples/features/style_scope.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
