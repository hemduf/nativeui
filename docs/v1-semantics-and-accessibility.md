# NativeUI 1.0 semantic and accessibility data model

NativeUI exposes a backend-neutral semantic data model in [`semantics.hpp`](../include/nativeui/semantics.hpp). Components publish owned semantic values; platform accessibility integration can consume immutable identities/snapshots without exposing AppKit, UI Automation, AT-SPI, Pugl or retained-tree pointers in normal public signatures.

This chapter describes the public C++ data model. Platform mapping and the broader accessibility design are documented separately in [Accessibility](accessibility.md).

## Identity

`SemanticId` is the stable identity type for an exposed semantic node. `0` (`kInvalidSemanticId`) is reserved as invalid.

Semantic identity is deliberately separate from a native accessibility object address. Public snapshots reference parents/children by IDs, which lets a platform layer treat removed identities as absent rather than retaining raw component/tree pointers.

Virtualized collections use a second identity type: `VirtualSemanticItemToken`. A token belongs to one virtual collection lifetime and is **not a hash**. The collection mints a non-zero token for a logical item, preserves it while that logical key remains present through reorder/metadata updates, and does not reuse it for a different live/stale identity.

## Roles and actions

`SemanticRole` is NativeUI's closed platform-neutral role set. It includes common controls such as Button, Checkbox, Slider, TextInput, ListView, Tab, Dialog, Image and Custom.

`SemanticRole::None` is important: it means the node itself does not need to be an exposed semantic object. A semantic-tree builder can flatten that wrapper while preserving meaningful descendants.

`SemanticAction` describes actions that a node explicitly advertises:

- Activate / Toggle;
- Focus;
- Increment / Decrement / SetValue;
- Select;
- Expand / Collapse.

Consumers must test the advertised action set rather than inferring actions solely from role. `SemanticInfo::supports(action)` performs that exact membership test.

## SemanticInfo

`SemanticInfo` is an owned value description:

| Field | Meaning |
| --- | --- |
| `role` | backend-neutral role |
| `name`, `description` | owned accessible strings |
| `text_value` | optional textual value |
| `numeric_value` | optional numeric/application value |
| `value_range` | optional `minimum`, `maximum`, `step` |
| `enabled`, `read_only` | effective availability/capability state |
| `checked` | NotApplicable / Unchecked / Checked / Mixed |
| `selected` | current logical selection |
| `expanded` | NotApplicable / Collapsed / Expanded |
| `focusable`, `focused` | keyboard/accessibility focus state |
| `actions` | explicitly supported semantic actions |

The type owns its strings, optionals and action vector. It contains no borrowed native object or retained component pointer.

Custom components publish this data through the public `Component::semantics() const` hook. The default returns role None, so custom components only become direct semantic objects when they choose a meaningful semantic description.

Example:

```cpp
class GainControl : public ui::Component {
public:
    ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Slider;
        info.name = "Gain";
        info.numeric_value = gain_;
        info.value_range = ui::SemanticValueRange{-60.0, 12.0, 0.1};
        info.enabled = effective_enabled();
        info.read_only = effective_read_only();
        info.focusable = true;
        info.actions = {
            ui::SemanticAction::Focus,
            ui::SemanticAction::Increment,
            ui::SemanticAction::Decrement,
            ui::SemanticAction::SetValue,
        };
        return info;
    }

    // measure / paint / input omitted
};
```

Semantic information is a projection of component state, not a second source of truth. Application/widget state remains authoritative.

## Semantic changes

`SemanticChange` provides five coarse categories for generation-to-generation notifications:

- `StructureChanged`;
- `FocusChanged`;
- `SelectionChanged`;
- `ValueChanged`;
- `BoundsChanged`.

A consumer may coalesce these categories, but a value or bounds update need not imply a structure rebuild.

## Virtualized collection semantics

A large `ListView` can expose its full logical semantic dataset without mounting every visual row.

`VirtualSemanticItemMetadata` contains immutable per-item semantic metadata. `VirtualSemanticChildren` combines a shared immutable `MetadataSnapshot` with cheap current state:

- dataset generation;
- selected item token;
- list logical bounds;
- fixed row height;
- vertical scroll offset.

`item_at(index)` materializes one `VirtualSemanticItem` with role ListItem and computes its logical bounds arithmetically. It never calls the application row factory and never mounts a visual row.

`metadata_snapshot()` returns a borrowed reference to the shared immutable metadata owner. That reference is valid only while the `VirtualSemanticChildren` object remains alive; copy the shared pointer when a longer lifetime is required.

`index_of_selected_item()` resolves the selected token against the metadata and returns nullopt when there is no selected token or that token is absent.

## Immutable tree snapshots

`SemanticNodeSnapshot` contains:

- stable `id` and `parent` semantic IDs;
- one owned `SemanticInfo`;
- logical view-relative bounds;
- ordered child semantic IDs;
- optional virtual children.

`SemanticTreeSnapshot` groups nodes into one generation and identifies the semantic root.

Snapshots are data-only: they do not expose `Node*`, `Component*` or native accessibility objects. This supports retaining an older immutable generation while the UI thread publishes a newer generation.

## Geometry, threading and ownership

Semantic bounds use NativeUI logical view coordinates. Native/screen coordinate conversion belongs to the platform boundary and must not be applied twice by application code.

Ordinary component semantic production happens in the retained UI domain. Immutable snapshot values can be owned/read according to the lifetime of the published snapshot; mutating semantic actions must ultimately be validated against current live UI state rather than mutating snapshot data.

Semantic state is per UI/view. NativeUI's public data model does not define a process-global semantic registry or current semantic root.

## Declaration-level ownership, validation and failure contracts

The public semantic structs are ordinary owned C++ values rather than live accessibility handles. Identity values do not keep a component, tree, view or native proxy alive. `kInvalidSemanticId` and `kInvalidVirtualSemanticItemToken` are zero sentinels; runtime-produced live identities are non-zero and meaningful only within the owner that minted them.

`SemanticInfo` owns its UTF-8 strings, optional values and action vector. Default construction produces `role=None`, `enabled=true`, no checked/expanded state, no value, no focus and no advertised actions. Numeric values and `SemanticValueRange::{minimum,maximum,step}` are application-domain values, not pixels. `SemanticValueRange` performs no clamping, ordering or finite-value validation: the producer must publish a coherent range. Defaulted equality is exact, including floating-point fields and action-vector ordering.

Actions are explicit capabilities. `SemanticInfo::supports()` is a linear allocation-free membership test; it does not infer actions from role and does not deduplicate repeated entries. Copying semantic values can allocate because strings and vectors are owned. Component semantic production and snapshot construction are UI/application-domain work rather than audio/DSP real-time APIs.

`VirtualSemanticChildren::from_metadata()` shares a non-null `MetadataSnapshot` without recopying O(N) item metadata. A null handle is normalized to a newly allocated empty immutable vector. Dataset generation, selected token, list bounds, row height and vertical scroll offset are carried verbatim; this low-level constructor does not sanitize finite/positive geometry. NativeUI's virtual-list producer validates those invariants before publication, and another producer must do the same.

Published virtual metadata must remain immutable. Although the public handle is `shared_ptr<const Metadata>`, code retaining another mutable alias must stop mutating it after publication. Copying `metadata_snapshot()` extends metadata lifetime in O(1); retaining only the returned reference does not extend the `VirtualSemanticChildren` object's lifetime.

`size()` is O(1). `index_of_selected_item()` is an allocation-free O(N) token scan. `item_at()` never calls the visual row factory or mutates the retained tree, but it copies owned name/description/action data into a new `SemanticInfo`; those copies may allocate and throw. Item bounds are logical and are not clipped to the visible list rectangle, so off-screen logical rows may legitimately lie outside it.

A `SemanticNodeSnapshot` owns its `SemanticInfo`, child-ID vector and optional virtual-children value. The root uses `kInvalidSemanticId` as its parent. `SemanticTreeSnapshot::generation` is producer-owned: NativeUI publishes coherent generations, while the aggregate itself does not enforce monotonicity. Copying a tree creates an independent node vector while virtual O(N) metadata remains intentionally shared through immutable metadata owners.

The snapshot aggregates provide no internal locking. Once published, a generation should be treated as immutable; concurrent mutation requires external synchronization. References or pointers into owned strings/vectors follow normal C++ invalidation rules and must not be retained across mutation or destruction.

```cpp
auto virtual_children = controller.semantic_children(list_bounds);
auto metadata = virtual_children.metadata_snapshot(); // O(1) shared ownership

if (auto item = virtual_children.item_at(42)) {
    use_semantic_item(*item); // materialized info owns copied strings/actions
}

// metadata can outlive virtual_children.
```

## Related public surfaces

- [`Component::semantics()`](../include/nativeui/component_base.hpp) — custom component projection seam;
- [`virtual_list.hpp`](../include/nativeui/virtual_list.hpp) — virtual collection owner/state;
- [Accessibility](accessibility.md) — platform mapping, availability/focus rules and native proxy design;
- [Composition, layout and widgets](v1-composition-layout-and-widgets.md) — retained component and virtual-list behavior.
