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

## Related public surfaces

- [`Component::semantics()`](../include/nativeui/component_base.hpp) — custom component projection seam;
- [`virtual_list.hpp`](../include/nativeui/virtual_list.hpp) — virtual collection owner/state;
- [Accessibility](accessibility.md) — platform mapping, availability/focus rules and native proxy design;
- [Composition, layout and widgets](v1-composition-layout-and-widgets.md) — retained component and virtual-list behavior.
