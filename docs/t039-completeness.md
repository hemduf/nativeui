# T039 scoped style inheritance — completeness matrix

T039 is complete. PR #260 was squash-merged from frozen executable head `3cf0c8e33cb472648cef880ece780dbda6b3fd38` as `1b998491306ae3fff9771339bedca7e14007f355` after exact-head normal CI `34778524773`, T066 Window Controls `34778524854`, T042 Lifecycle Stress `34779287474`, and T052 v0.1 Release Gate `34779287449` all completed successfully. Independent Integration final `CODE_REVIEW.md` recorded **Blocking=0 / Important=0**.

## Public and retained contract

| Requirement | Implementation | Deterministic evidence | Final status |
| --- | --- | --- | --- |
| Typed lexical scope values only | `StyleScopeOverrides` with palette, typography, spacing, radii and control-default families | compile-time absence checks for structural `width`/`enabled`; umbrella compile through feature example | Complete |
| Theme -> outer -> inner -> explicit component -> visual state | outer-to-inner `apply_style_scope_overrides()`, retained ancestry through `ThemeBinding::descendant_theme()`, existing T038 resolver seam | pure precedence/partial-field checks + nested retained geometry + sampled headless matrix | Complete |
| Nearest scope wins per field without erasing unrelated outer values | optional typed fields merged independently | pure merge assertions and nested retained scope self-test | Complete |
| Scope affects descendants only | Tree binds child theme from nearest retained ancestor; no global registry/cache | sibling isolation + bounded paint dirtiness | Complete |
| Explicit component style wins | T038 explicit recipe applied after inherited base | explicit Button fill in sampled headless matrix | Complete |
| Non-inheritable layout/state/callback properties absent | scope API exposes only Theme-derived defaults | `HasWidthMember` / `HasEnabledMember` compile checks | Complete |
| Equal effective replacement is a no-op | `StyleScopeComponent::replace_overrides()` classifies resolved Theme before invalidation | repeated equal `State<StyleScopeOverrides>` set leaves Tree clean | Complete |
| Paint-only replacement stays scoped | retained NodeId callback routes `ThemeInvalidation::Paint` to node paint invalidation | dirty-region test rejects whole-viewport paint invalidation | Complete |
| Layout-affecting replacement invalidates layout + paint | retained NodeId callback routes `ThemeInvalidation::Layout` through `invalidate_layout_from()` | control-height replacement marks layout + paint | Complete |
| Removal/replacement restores outer Theme | retained dynamic branch removes/replaces and reinserts the inner `StyleScope` | inner scope -> fallback outer scope -> inner reinsert proves fresh ancestry with no stale snapshot | Complete |
| T058 inserted descendants resolve current lexical ancestry | `mount_node()` resolves through `inherited_theme_for()`; no second reconciliation engine | dynamic false -> true insertion under active scope proves inherited scoped value | Complete |
| Two UI trees remain isolated | resolved Theme/state/invalidator ownership is retained-tree/component local | mutate first scoped State; second Tree stays clean and pixel-stable | Complete |
| Stable nested-scope headless visual contract | normal widgets consume resolved scoped Theme through T038 defaults | sampled outer, nearest-inner, explicit-local and dynamically inserted content | Complete |
| Dedicated feature example | `examples/features/t039_style_scope.cpp` | interactive nested scope demo + deterministic `--self-test` | Complete |

## Layout invalidation locality boundary

T039 does not invent a second retained layout engine. For layout-affecting scope changes, `apply_scoped_theme_change()` begins layout invalidation at the scoped retained node through the existing `invalidate_layout_from()` path. The current layout architecture may conservatively expand paint dirtiness when moved bounds require repainting old and new pixels. The issue requires subtree/path locality only where the current invalidation architecture can express it safely; T039 therefore does not claim paint-region locality for a layout-changing scope mutation. Paint-only scope changes remain bounded to the scoped retained node/descendants and are separately tested.

## Required tests checklist

- [x] Pure outer/inner/component precedence tests.
- [x] Partial-field merge tests.
- [x] Sibling isolation test.
- [x] Compile/API structural-property exclusion test.
- [x] Paint-vs-layout scoped invalidation tests.
- [x] Structural scope removal/restoration/reinsertion test.
- [x] T058 dynamic descendant insertion ancestry test.
- [x] Two-UI isolation test.
- [x] Stable headless nested-scope sampled visual matrix.
- [x] Exact-head normal CI and matching T066 workflow green.
- [x] Final executable-head review with zero Blocking/Important findings.
- [x] Final-candidate T042/T052 qualification green.

## CODE_REVIEW.md final record

- **Instance isolation:** PASS — no shared current scope, cascade registry or global resolved-theme cache; each `StyleScopeComponent` and retained Tree own their state/invalidators.
- **Globals/statics:** PASS — no mutable process-global or `thread_local` scope state.
- **Threading/RT:** PASS — ordinary State/scope mutation remains UI-thread-domain work; no audio/RT contract introduced.
- **Lifetime/reentrancy:** PASS — State observation uses teardown-safe weak retained state; unmount clears subscription/apply callback and Tree invalidator before destruction; invalidators use stable `NodeId` rather than raw retained pointers.
- **Platform/Objective-C:** PASS / not applicable — generic retained C++ only, no new platform or Objective-C runtime surface.
- **Public boundary:** PASS — no Pugl/Skia/platform type, selector/CSS property bag, arbitrary string-key map or second reconciler introduced.
- **Remaining Blocking/Important findings:** none.

## Non-goals checked

- no CSS selectors/specificity/classes/IDs/pseudo-selectors;
- no arbitrary property bag;
- no inherited callbacks/model values/visibility/enabled/read-only/layout constraints;
- no T061 overlay styling exception;
- no animation/interpolation;
- no second T058 reconciliation path.

## Closure

T039 is closed as `Done/status:done`. New styling features do not belong in this ticket; the v1 critical path continues with T069 public API freeze.