# T039 scoped style inheritance — completeness matrix

This record maps issue #39 to the current implementation and deterministic evidence. It is deliberately conservative: a green aggregate CI run does not make T039 complete while an acceptance requirement or final review item remains unproven.

Closeout baseline before this batch: `5f68cdee5223338f6335d58afd7cf80a26de6a0e`. Normal CI `34770001334` and T066 Window Controls `34770001228` are green on that retained-scope baseline. The current closeout batch adds the remaining dynamic/removal/isolation/headless-golden evidence on top of that baseline; its exact qualification head is recorded in the PR/event stream because a file cannot embed the SHA of the commit that contains itself.

## Public and retained contract

| Requirement | Implementation | Deterministic evidence | Status before current-head CI |
| --- | --- | --- | --- |
| Typed lexical scope values only | `StyleScopeOverrides` with palette, typography, spacing, radii and control-default families | compile-time absence checks for structural `width`/`enabled`; umbrella compile through feature example | Complete on prior green head |
| Theme -> outer -> inner -> explicit component -> visual state | outer-to-inner `apply_style_scope_overrides()`, retained ancestry binding through `ThemeBinding::descendant_theme()`, existing T038 resolver seam | pure precedence/partial-field checks + nested retained geometry + sampled headless golden | Covered; current closeout golden pending CI |
| Nearest scope wins per field without erasing unrelated outer values | optional typed fields merged independently | pure merge assertions and nested retained scope self-test | Complete on prior green head |
| Scope affects descendants only | Tree binds child theme from nearest retained ancestor; no global registry/cache | sibling measurement isolation + paint-only dirty region remains non-global | Current closeout evidence pending CI |
| Explicit component style wins | existing T038 explicit recipe applied after inherited base | explicit Button fill in headless golden matrix | Current closeout evidence pending CI |
| Non-inheritable layout/state/callback properties absent | scope API exposes only Theme-derived defaults | `HasWidthMember` / `HasEnabledMember` compile checks | Complete on prior green head |
| Equal effective replacement is a no-op | `StyleScopeComponent::replace_overrides()` classifies resolved Theme before invalidation | repeated equal `State<StyleScopeOverrides>` set leaves Tree clean | Complete on prior green head |
| Paint-only replacement stays scoped | retained NodeId callback routes `ThemeInvalidation::Paint` to node paint invalidation | dirty-region test rejects whole-viewport paint invalidation | Complete on prior green head |
| Layout-affecting replacement invalidates layout + paint | retained NodeId callback routes `ThemeInvalidation::Layout` through `invalidate_layout_from()` | control-height replacement marks layout + paint | Complete on prior green head |
| Removal/replacement restores outer Theme | empty replacement re-resolves the same descendant subtree from retained ancestry | inner override -> empty override pixel restoration check | Current closeout evidence pending CI |
| T058 inserted descendants resolve current lexical ancestry | `mount_node()` binds a new node through `inherited_theme_for()`; no second reconciliation engine | `If` false -> true under `StyleScope`, headless pixel proves inherited scoped fill | Current closeout evidence pending CI |
| Two UI trees remain isolated | resolved Theme/state/invalidator ownership is retained-tree/component local | mutate first scoped State; second Tree stays clean and pixel-stable | Current closeout evidence pending CI |
| Stable nested-scope headless visual contract | normal widgets consume resolved scoped Theme through existing T038 style defaults | four-sample headless golden matrix: outer, nearest-inner, explicit-local, dynamically inserted | Current closeout evidence pending CI |
| Dedicated feature example | `examples/features/t039_style_scope.cpp` | interactive nested scope demo + deterministic `--self-test` | Present |

## Layout invalidation locality boundary

T039 does not invent a second retained layout engine. For layout-affecting scope changes, `apply_scoped_theme_change()` begins layout invalidation at the scoped retained node via the existing `invalidate_layout_from()` path. The current layout architecture may conservatively expand paint dirtiness when moved bounds require repainting old and new pixels. The issue explicitly requires subtree/path locality only **where the current invalidation architecture can express this safely**; T039 therefore does not claim paint-region locality for a layout-changing scope mutation. Paint-only scope changes remain bounded to the scoped retained node/descendants and are separately tested.

## Required tests checklist

- [x] Pure outer/inner/component precedence tests.
- [x] Partial-field merge tests.
- [x] Sibling isolation test.
- [x] Compile/API structural-property exclusion test.
- [x] Paint-vs-layout scoped invalidation tests.
- [x] Scope removal/restoration test.
- [x] T058 dynamic descendant insertion ancestry test.
- [x] Stable headless nested-scope sampled golden matrix.
- [ ] Current closeout-head normal CI and every matching path-scoped workflow green.
- [ ] Final self `CODE_REVIEW.md` audit on that exact executable head with zero Blocking/Important findings.
- [ ] Independent Integration review/final qualification.

## CODE_REVIEW.md self-audit boundary

The current implementation is designed around instance-owned value/state data and retained-tree callbacks:

- **Instance isolation:** no shared current scope, cascade registry or global resolved-theme cache; each `StyleScopeComponent` owns its patch/resolved Theme and each Tree owns its nodes/invalidators.
- **Globals/statics:** no mutable process-global or `thread_local` scope state.
- **Threading/RT:** ordinary State/scope mutation is UI-thread-domain work; no audio/RT contract is introduced.
- **Lifetime/reentrancy:** State observation uses a weak retained observer token; unmount resets subscription/apply callback and Tree clears the theme-change invalidator before node destruction. The invalidator captures stable `NodeId`, not a raw Node/Component pointer.
- **Platform/Objective-C:** generic retained C++ only; no platform or Objective-C runtime surface changes.
- **Public boundary:** no Pugl/Skia/platform type, selector/CSS property bag, arbitrary string-key style map or new global reconciler is introduced.

Final self-review is intentionally not declared PASS until the new exact head executes successfully. If CI reveals a product defect, it becomes one consolidated correction batch before `SOURCE_READY`.

## Non-goals checked

- no CSS selectors/specificity/classes/IDs/pseudo-selectors;
- no arbitrary property bag;
- no inherited callbacks/model values/visibility/enabled/read-only/layout constraints;
- no T061 overlay styling exception;
- no animation/interpolation;
- no second T058 reconciliation path.

## Closure boundary

No new styling feature belongs in T039 after this closeout evidence batch. Remaining work is exact-head qualification, final self review, `SOURCE_READY`, independent Integration closeout, and same-cycle project-state documentation/status bookkeeping at merge.