#include "example_support.hpp"

#include <type_traits>

namespace {

[[nodiscard]] bool same_color(ui::Color a, ui::Color b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

template <class T>
concept HasWidthMember = requires(T value) { value.width; };

template <class T>
concept HasEnabledMember = requires(T value) { value.enabled; };

ui::StyleScopeOverrides outer_scope() {
    ui::StyleScopeOverrides result;
    result.palette.accent = ui::Color{0.18f, 0.52f, 0.88f, 1.0f};
    result.palette.text = ui::Color{0.86f, 0.90f, 0.96f, 1.0f};
    result.typography.control_size = 15.0f;
    result.spacing.large = 18.0f;
    result.controls.control_height = 38.0f;
    return result;
}

ui::StyleScopeOverrides inner_scope() {
    ui::StyleScopeOverrides result;
    result.palette.accent = ui::Color{0.92f, 0.42f, 0.20f, 1.0f};
    result.radii.medium = 11.0f;
    return result;
}

int verify_retained_scope_invalidation() {
    ui::StyleScopeOverrides initial;
    initial.palette.control_background = ui::Color{0.12f, 0.18f, 0.25f, 1.0f};
    ui::State<ui::StyleScopeOverrides> scoped{initial};

    ui::UI tree{ui::Row{
        ui::StyleScope{scoped, ui::Button{"Scoped", [] {}}},
        ui::Button{"Sibling", [] {}}
    }.gap(40.0f)};
    const ui::Size size{600.0f, 160.0f};
    tree.resize(size);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) return example::fail("retained scope baseline render failed");
    if (tree.dirty()) return example::fail("baseline render left retained scope dirty");

    auto paint_only = scoped.get();
    paint_only.palette.control_background = ui::Color{0.36f, 0.16f, 0.10f, 1.0f};
    scoped.set(paint_only);
    if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("paint-only scope replacement did not stay paint-only");
    }
    if (tree.dirty_regions().empty()) {
        return example::fail("paint-only scope replacement produced no scoped dirty region");
    }
    for (const auto rect : tree.dirty_regions()) {
        if (rect.w >= size.w && rect.h >= size.h) {
            return example::fail("paint-only scope replacement dirtied the whole UI");
        }
    }

    if (!renderer.render(tree)) return example::fail("paint-only scope rerender failed");
    scoped.set(paint_only);
    if (tree.dirty()) return example::fail("equal scope replacement invalidated the tree");

    auto layout = scoped.get();
    layout.controls.control_height = 82.0f;
    scoped.set(layout);
    if (!tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("layout-affecting scope replacement missed layout+paint invalidation");
    }
    return 0;
}

int verify_nested_retained_precedence() {
    ui::StyleScopeOverrides outer;
    outer.palette.control_background = ui::Color{0.14f, 0.20f, 0.28f, 1.0f};
    outer.controls.control_height = 48.0f;

    ui::StyleScopeOverrides inner;
    inner.controls.control_height = 86.0f;

    ui::UI outer_only{ui::StyleScope{outer, ui::Button{"Outer", [] {}}}};
    ui::UI nested{ui::StyleScope{
        outer,
        ui::StyleScope{inner, ui::Button{"Nested", [] {}}}}};

    const auto outer_metrics = outer_only.measure();
    const auto nested_metrics = nested.measure();
    if (nested_metrics.preferred.h <= outer_metrics.preferred.h) {
        return example::fail("nearest retained scope did not override inherited control geometry");
    }
    return 0;
}

int self_test() {
    static_assert(!HasWidthMember<ui::StyleScopeOverrides>);
    static_assert(!HasEnabledMember<ui::StyleScopeOverrides>);

    const auto base = ui::default_theme();
    const auto outer = ui::apply_style_scope_overrides(base, outer_scope());
    const auto nested = ui::apply_style_scope_overrides(outer, inner_scope());

    if (!same_color(outer.palette.accent, *outer_scope().palette.accent) ||
        !same_color(nested.palette.accent, *inner_scope().palette.accent)) {
        return example::fail("nearest scope did not win for the explicitly overridden field");
    }
    if (!same_color(nested.palette.text, *outer_scope().palette.text) ||
        nested.typography.control_size != 15.0f || nested.spacing.large != 18.0f ||
        nested.controls.control_height != 38.0f) {
        return example::fail("inner partial override erased unrelated outer fields");
    }
    if (nested.radii.medium != 11.0f || nested.radii.sm != base.radii.sm) {
        return example::fail("typed radius override did not preserve inherited siblings");
    }

    auto paint_before = outer_scope();
    auto paint_after = paint_before;
    paint_after.palette.accent = ui::Color{0.20f, 0.56f, 0.92f, 1.0f};
    if (ui::classify_style_scope_change(base, paint_before, paint_after) !=
        ui::ThemeInvalidation::Paint) {
        return example::fail("paint-only scope change was not classified as paint");
    }

    auto layout_after = paint_before;
    layout_after.typography.control_size = 19.0f;
    if (ui::classify_style_scope_change(base, paint_before, layout_after) !=
        ui::ThemeInvalidation::Layout) {
        return example::fail("typography scope change was not classified as layout");
    }
    if (ui::classify_style_scope_change(base, paint_before, paint_before) !=
        ui::ThemeInvalidation::None) {
        return example::fail("equal scope replacement was not a no-op");
    }

    // T038 consumes an already-resolved inherited recipe before applying the
    // component-local recipe and visual-state patch. This proves the T039 pure
    // layer feeds that existing seam without inventing a second style engine.
    const auto inherited_button = ui::default_button_style(nested);
    ui::ButtonStyle explicit_button;
    explicit_button.base.control_height = 51.0f;
    explicit_button.base.fill = ui::Color{0.07f, 0.09f, 0.12f, 1.0f};
    const auto resolved = ui::resolve_button_style(
        inherited_button,
        explicit_button,
        ui::VisualState{.enabled = true, .hovered = true});
    if (resolved.control_height != 51.0f ||
        !same_color(resolved.fill, nested.palette.control_hover)) {
        return example::fail("component/state precedence did not follow the T038 resolver seam");
    }

    if (const auto retained = verify_retained_scope_invalidation(); retained != 0) return retained;
    if (const auto retained = verify_nested_retained_precedence(); retained != 0) return retained;
    return 0;
}

ui::UI make_demo() {
    auto outer = outer_scope();
    auto inner = inner_scope();
    return ui::UI{ui::Column{
        ui::Header{"T039 — Scoped Style Resolution"},
        ui::Label{"Outer/inner lexical scopes affect only their retained descendants."},
        ui::StyleScope{
            outer,
            ui::Column{
                ui::Button{"Outer scope", [] {}},
                ui::StyleScope{inner, ui::Button{"Nearest inner scope", [] {}}}
            }.gap(outer.spacing.large.value_or(12.0f))},
        ui::Button{"Unscoped sibling", [] {}}
    }.gap(14.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto ui = make_demo();
    return example::run_window(ui, "NativeUI T039 Style Scope", {720.0f, 360.0f});
}
