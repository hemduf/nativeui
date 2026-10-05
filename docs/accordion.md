# Accordion

Status: **new — implementation required**.

[Component catalog](widgets.md)

## 1. Purpose and current state

Grouped disclosure sections with navigation between headers and an opening policy. NativeUI has no Accordion; the target is based on [Collapsible](collapsible.md).

MyGo `ui/collapsible.go`: `Accordion`, `AccordionItem`, `accordionKeys`. MyGo sections are independent; the application closes the others to open one exclusively. NativeUI makes this policy explicit in the builder.

Reviewed versions: NativeUI `e10077ff39b8cb977669a7d5604562f66d07cb4c`; MyGo `40df43e3f7fd759616f7d3896c74ef9c51d5d6dc`. Source descriptions refer to the current implementation; the requirements below define the target contract.

## 2. Public API and composition

Proposed target API:

```cpp
enum class AccordionMode { Multiple, Single };
Accordion(Binding<std::vector<std::string>> open_keys);
Accordion(State<std::vector<std::string>>& open_keys);
template<class Child>
Accordion&& section(std::string key, std::string title, Child&& content,
                     bool enabled=true) &&;
Accordion&& mode(AccordionMode) &&;
Accordion&& content_policy(DisclosureContentPolicy) &&;
Accordion&& on_change(std::function<void(const std::vector<std::string>&)>) &&;
Accordion&& style(AccordionStyle) &&;
Spec spec() &&;
```

```cpp
ui::State<std::vector<std::string>> open_keys{{"general"}};
auto panels = ui::Accordion{open_keys}.mode(ui::AccordionMode::Single)
    .section("general","General",ui::Label{"Options"})
    .section("advanced","Advanced",ui::Label{"Settings"});
```

Defaults: Multiple, Retain. Section keys are unique, non-empty strings; items remain internal models in the same header/source pair.

All signatures belong to the `ui` namespace. The builder is consumed by `Spec spec() &&`; its children become owned `Spec` objects. Proposed declarations do not claim to be an API that has already shipped. Signature blocks are fragments of the members of the type being described, rather than complete programs; `Key` or `T` refers to that type’s template parameter where one exists.

## 3. State, ownership and notifications

Open keys are ordered according to section order, with duplicates removed. Multiple allows several sections; Single opening replaces the sole key, and closing permits zero open sections. An external snapshot containing several keys in Single displays the first matching section without a silent rewrite. A new gesture publishes a canonical vector. Unknown keys are ignored visually.

The component, its state and its notifications are confined to the UI/main thread. Historical APIs that genuinely borrow `State<T>&` directly require the borrowed State to remain alive; constructors that delegate to `state.binding()` retain the safe control block rather than the State. After State destruction, `valid()` becomes false, `get()` retains the last value, `set()` is ignored and `observe()` is inactive; destruction does not automatically send a notification. Objects captured by application models retain their own lifetime requirements.

## 4. Interactions

Up/Down move focus between enabled headers; Home/End move to the first/last. Enter/Space toggles the focused section. Tab accesses open content; a single roving focus stop covers all headers. Header pointer behavior matches Collapsible. Closing focused content returns focus to the header; exclusive opening recovers focus from the previous content without duplicate notification.

Routing respects inherited availability, clipping and runtime capture. Do not add global shortcuts or access to audio parameters for this component.

## 5. Measurement and layout

A column of headers/panels with a grouped border and separators. Each section uses its header and open-content dimensions. Closed content receives no allocation. In Single, publish opening/closing together to avoid an intermediate empty frame. Accordion overflow does not install implicit scrolling.

Sizes and positions use NativeUI logical coordinates. The backend applies the scale-factor conversion exactly once; the component does not handle native screen coordinates.

## 6. Presentation and invalidation

AccordionStyle extends CollapsibleStyle with container radius/border, separator and padding. Inset the focus ring so the border does not clip it. Invalidating a section does not require repainting unchanged sections. Animations are independent but share an opening controller; reduced motion matches Collapsible.

Invalidation distinguishes pixel changes from metric changes. An unchanged effective state is a no-op; the component does not force a repaint of the entire window when repainting its bounds is sufficient.

## 7. Accessibility

Group with an optional name, Custom/Button headers and expanded state. Expanded keys reflect the exact effective policy. Closed-panel descendants are not accessible targets. An Accordion role would be a separate extension; no promise of already shipped native support.

The contract uses the backend-neutral hooks and snapshots in [semantics.hpp](../include/nativeui/semantics.hpp) and [accessibility.md](accessibility.md). Native bridges belong to the deferred T068 work; this specification does not validate VoiceOver, UIA or AT-SPI. Any role absent from the current enum requires a separate extension; until then, use `None`, `Group` or `Custom` as appropriate.

## 8. Lifecycle and recovery

Validate keys before consuming Spec. A Single change prepares the complete vector, commits once to Binding and invokes one callback. A reentrant callback changing the open list wins at the next reconciliation. Removing a section during capture/focus invalidates its identity and releases animations; do not access an old index.

Subscriptions, invalidators and captures are per instance and released through RAII. A callback that has started and throws is never automatically replayed; invariants are restored before the C++ exception propagates. Unmounting is no-throw and does not invoke application destruction callbacks. Destruction of the UI owner from a callback stack must be deferred to the existing safe checkpoint.

## 9. Dependencies and edge cases

Collapsible, Binding<vector<string>>, focus/roving navigation and style. Cover empty sections/long titles, all disabled, unknown keys, duplicate section keys causing invalid_argument, expired binding and reentrancy. External State must live long enough; no process-global header list.

Reuse the `Tree`, focus, availability and invalidation services; do not create competing local copies. IME limitations remain those described in [DESIGN.md §17.4](../DESIGN.md): native transport of committed text is available; full preedit/IME transport and candidate rectangles are deferred. Do not confuse this platform limitation with an inability to test text models headlessly.

## 10. Files and compatibility

Target: `include/nativeui/accordion.hpp` and `src/accordion.cpp`.

accordion.hpp contains the internal Section, public if needed, policy/style and the section template; accordion.cpp contains mode, roving headers, normalization and layout/input/paint. Reuse the Collapsible core without creating a file for each AccordionItem.

The header contains declarations and only the necessary template adapters. The `.cpp` must contain a real retained implementation for measurement/layout and, where applicable, input/paint; do not add empty files or a central widget switch. Add this `.cpp` to `NativeUI::Core` during implementation. Public signatures must not expose Pugl, Skia, OS, plugin or automation types.

## 11. Tests and acceptance criteria

- `accordion_multiple_single`: policies and zero open sections.
- `accordion_external_noncanonical`: deterministic display without hidden writes.
- `accordion_header_roving`: Up/Down/Home/End, skipping disabled headers.
- `accordion_exclusive_focus`: closing the previous panel while it has focus.
- `accordion_keys_validation`: duplicates rejected before commit.
- `accordion_reentrant_change`: at-most-once callback and stable final state.
- `accordion_section_teardown`: timers/captures are per instance.

Create the future public example `examples/features/accordion.cpp`; `--self-test` runs the assertions specific to this page and then exits without manual interaction. Add headless rendering of relevant states, destruction/remounting, two simultaneous instances and exception injection at application boundaries.

These tests are to be implemented with the component. This documentation work verifies sources, signatures and links; it does not report execution of these tests. Acceptance requires all named cases to pass, no regressions in historical APIs, no mutable global dependencies and no NativeUI warnings.
